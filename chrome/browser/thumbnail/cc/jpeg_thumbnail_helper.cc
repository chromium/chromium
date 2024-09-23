// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/thumbnail/cc/jpeg_thumbnail_helper.h"

#include <algorithm>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace thumbnail {
namespace {

SkBitmap ResizeBitmap(const SkBitmap& bitmap) {
  constexpr int kScale = 2;
  int width = bitmap.width() / kScale;
  int height = bitmap.height() / kScale;

  SkIRect dest_subset = {0, 0, width, height};

  SkBitmap output = skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::RESIZE_BETTER, width, height, dest_subset);
  output.setImmutable();
  return output;
}

void CompressTask(
    const SkBitmap& bitmap,
    base::OnceCallback<void(std::vector<uint8_t>)> post_processing_task) {
  constexpr int kCompressionQuality = 97;
  std::vector<uint8_t> data;
  const bool result =
      gfx::JPEGCodec::Encode(ResizeBitmap(bitmap), kCompressionQuality, &data);
  DCHECK(result);

  std::move(post_processing_task).Run(std::move(data));
}

void WriteTask(base::FilePath file_path,
               std::vector<uint8_t> compressed_data,
               base::OnceCallback<void(bool)> post_write_task) {
  DCHECK(!compressed_data.empty());

  if (!base::WriteFile(file_path, compressed_data)) {
    base::DeleteFile(file_path);
    std::move(post_write_task).Run(false);
    return;
  }

  std::move(post_write_task).Run(true);
}

void ReadTask(base::FilePath file_path,
              base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
                  post_read_task) {
  std::optional<std::vector<uint8_t>> read_data =
      base::ReadFileToBytes(file_path);

  if (!read_data.has_value()) {
    base::DeleteFile(file_path);
  }

  std::move(post_read_task).Run(std::move(read_data));
}

void DeleteTask(base::FilePath file_path) {
  if (base::PathExists(file_path)) {
    base::DeleteFile(file_path);
  }
}

}  // anonymous namespace

JpegThumbnailHelper::JpegThumbnailHelper(
    const base::FilePath& base_path,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : base_path_(base_path),
      default_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      file_task_runner_(file_task_runner) {}

JpegThumbnailHelper::~JpegThumbnailHelper() {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
}

void JpegThumbnailHelper::Compress(
    const SkBitmap& bitmap,
    base::OnceCallback<void(std::vector<uint8_t>)> post_processing_task) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CompressTask, bitmap,
                     base::BindPostTask(default_task_runner_,
                                        std::move(post_processing_task))));
}

void JpegThumbnailHelper::Write(
    TabId tab_id,
    std::vector<uint8_t> compressed_data,
    base::OnceCallback<void(bool)> post_write_task) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath file_path = GetJpegFilePath(tab_id);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WriteTask, file_path, compressed_data,
                     base::BindPostTask(default_task_runner_,
                                        std::move(post_write_task))));
}

void JpegThumbnailHelper::Read(
    TabId tab_id,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
        post_read_task) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath file_path = GetJpegFilePath(tab_id);
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ReadTask, file_path,
                                base::BindPostTask(default_task_runner_,
                                                   std::move(post_read_task))));
}

void JpegThumbnailHelper::Delete(TabId tab_id) {
  DCHECK(default_task_runner_->RunsTasksInCurrentSequence());
  base::FilePath file_path = GetJpegFilePath(tab_id);
  file_task_runner_->PostTask(FROM_HERE,
                              base::BindOnce(&DeleteTask, file_path));
}

base::FilePath JpegThumbnailHelper::GetJpegFilePath(TabId tab_id) {
  base::FilePath file_path = base_path_.Append(base::NumberToString(tab_id));
  return file_path.AddExtension(".jpeg");
}

}  // namespace thumbnail
