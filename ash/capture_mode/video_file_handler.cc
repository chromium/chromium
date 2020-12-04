// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/video_file_handler.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"

namespace ash {

namespace {

// The value used to call reserve() on the |buffered_chunks_| vector.
// TODO(afakhry): Choose a value more suitable when the recording service is
// wired up.
constexpr size_t kReservedNumberOfChunks = 100;

}  // namespace

// static
base::SequenceBound<VideoFileHandler> VideoFileHandler::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& path,
    size_t max_buffered_bytes,
    size_t low_disk_space_threshold_bytes,
    base::OnceClosure on_low_disk_space_callback) {
  DCHECK(base::CurrentUIThread::IsSet());

  return base::SequenceBound<VideoFileHandler>(
      blocking_task_runner, path, max_buffered_bytes,
      low_disk_space_threshold_bytes, std::move(on_low_disk_space_callback));
}

bool VideoFileHandler::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!file_.IsValid());

  file_.Initialize(path_,
                   base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  success_ = file_.IsValid();
  if (!success_)
    LOG(ERROR) << "Failed to create video file: " << path_;
  return success_;
}

bool VideoFileHandler::GetSuccessStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return success_;
}

bool VideoFileHandler::AppendChunk(std::string chunk) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const size_t chunk_size = chunk.size();
  if (current_size_ + chunk_size > max_buffered_bytes_) {
    if (!FlushBufferedChunks()) {
      LOG(ERROR) << "Failed to flush video chunks buffer to file: " << path_;
      return false;
    }
    DCHECK_EQ(current_size_, 0u);
  }

  if (chunk_size > max_buffered_bytes_) {
    // The order here matters; this check must come after the above flush, since
    // the chunks must be appended to the file in sequence.
    // This should never be allowed to happen. The client should choose a big
    // enough capacity to allow caching of many chunks before IO is needed. But
    // if it happens for some reason, we just write the chunk directly to disk
    // without caching.
    LOG(ERROR) << "Chunk is bigger than the buffer capacity. Select a more "
                  "suitable capacity value: chunk_size: "
               << chunk_size << ", capacity: " << max_buffered_bytes_;
    AppendToFile(chunk);
  } else {
    buffered_chunks_.push_back(std::move(chunk));
    current_size_ += chunk_size;
    DCHECK_LE(current_size_, max_buffered_bytes_);
  }

  return success_;
}

bool VideoFileHandler::FlushBufferedChunks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!current_size_)
    return success_;

  for (const auto& chunk : buffered_chunks_)
    AppendToFile(chunk);

  buffered_chunks_.clear();
  current_size_ = 0;
  return success_;
}

VideoFileHandler::VideoFileHandler(const base::FilePath& path,
                                   size_t max_buffered_bytes,
                                   size_t low_disk_space_threshold_bytes,
                                   base::OnceClosure on_low_disk_space_callback)
    : path_(path),
      max_buffered_bytes_(max_buffered_bytes),
      low_disk_space_threshold_bytes_(low_disk_space_threshold_bytes),
      on_low_disk_space_callback_(std::move(on_low_disk_space_callback)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(max_buffered_bytes_);

  buffered_chunks_.reserve(kReservedNumberOfChunks);
}

VideoFileHandler::~VideoFileHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  FlushBufferedChunks();
}

void VideoFileHandler::AppendToFile(const std::string& chunk) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  success_ &=
      file_.WriteAtCurrentPosAndCheck(base::as_bytes(base::make_span(chunk)));

  if (!on_low_disk_space_callback_)
    return;

  const int64_t remaining_disk_bytes =
      base::SysInfo::AmountOfFreeDiskSpace(path_);
  if (remaining_disk_bytes >= 0 &&
      size_t{remaining_disk_bytes} < low_disk_space_threshold_bytes_) {
    // Note that a low disk space is not considered an IO failure, and should
    // not affect |success_|. It simply means that the available disk space is
    // now below the threshold set by the owner of this object, and should not
    // affect the actual writing of the chunks until the file system actually
    // fails to write.
    std::move(on_low_disk_space_callback_).Run();
  }
}

}  // namespace ash
