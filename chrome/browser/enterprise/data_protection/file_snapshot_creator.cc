// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/file_snapshot_creator.h"

#include "base/files/file_util.h"
#include "base/task/bind_post_task.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"

namespace enterprise_data_protection {

void FileSnapshotCreator::Start(
    scoped_refptr<storage::FileSystemContext> context,
    const storage::FileSystemURL& url,
    FileSnapshotCreator::SnapshotCreatedCallback ui_callback) {
  auto creator = base::MakeRefCounted<FileSnapshotCreator>(
      std::move(context), url, std::move(ui_callback));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSnapshotCreator::StartOnIOThread, creator));
}

FileSnapshotCreator::FileSnapshotCreator(
    scoped_refptr<storage::FileSystemContext> context,
    const storage::FileSystemURL& url,
    SnapshotCreatedCallback ui_callback)
    : context_(std::move(context)),
      url_(url),
      ui_callback_(base::BindPostTaskToCurrentDefault(std::move(ui_callback))),
      buffer_(base::MakeRefCounted<net::IOBufferWithSize>(
          FileSnapshotCreator::CHUNK_SIZE)) {}

FileSnapshotCreator::~FileSnapshotCreator() = default;

void FileSnapshotCreator::StartOnIOThread() {
  // The file stream reader needs to be created on the IO thread
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  reader_ = context_->CreateFileStreamReader(url_, 0, storage::kMaximumLength,
                                             base::Time());
  if (!reader_) {
    CompleteWithError();
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&FileSnapshotCreator::CreateTempFile,
                     base::WrapRefCounted(this)));
}

void FileSnapshotCreator::CompleteWithError() {
  if (!local_path_.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::GetDeleteFileCallback(local_path_));
  }
  if (!ui_callback_.is_null()) {
    std::move(ui_callback_).Run(base::FilePath());
  }
}

void FileSnapshotCreator::CreateTempFile() {
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    CompleteWithError();
    return;
  }

  base::File file(temp_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  if (!file.IsValid()) {
    CompleteWithError();
    return;
  }

  local_path_ = temp_path;
  file_ = std::move(file);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileSnapshotCreator::ReadNextChunk,
                                base::WrapRefCounted(this)));
}

void FileSnapshotCreator::ReadNextChunk() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  int result =
      reader_->Read(buffer_.get(), buffer_->size(),
                    base::BindOnce(&FileSnapshotCreator::OnReadCompleted,
                                   base::WrapRefCounted(this)));
  if (result != net::ERR_IO_PENDING) {
    OnReadCompleted(result);
  }
}

void FileSnapshotCreator::OnReadCompleted(int bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (bytes_read < 0) {
    CompleteWithError();
    return;
  }

  if (bytes_read == 0) {
    // Successfully copied entire stream. Close file and return path.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
        base::BindOnce(&FileSnapshotCreator::CloseFileAndFinish,
                       base::WrapRefCounted(this)));
    return;
  }

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&FileSnapshotCreator::WriteChunkToFile,
                     base::WrapRefCounted(this), bytes_read));
}

void FileSnapshotCreator::WriteChunkToFile(int bytes_to_write) {
  std::optional<size_t> bytes_written = file_.WriteAtCurrentPos(
      buffer_->span().first(static_cast<size_t>(bytes_to_write)));

  if (!bytes_written || *bytes_written != static_cast<size_t>(bytes_to_write)) {
    CompleteWithError();
    return;
  }
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&FileSnapshotCreator::ReadNextChunk,
                                base::WrapRefCounted(this)));
}

void FileSnapshotCreator::CloseFileAndFinish() {
  file_.Close();
  std::move(ui_callback_).Run(local_path_);
}

}  // namespace enterprise_data_protection
