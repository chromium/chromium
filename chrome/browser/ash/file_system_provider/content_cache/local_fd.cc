// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_system_provider/content_cache/local_fd.h"

#include "base/files/file_error_or.h"
#include "base/logging.h"

namespace ash::file_system_provider {

namespace {

base::FileErrorOr<std::unique_ptr<base::File>> WriteBytesBlocking(
    std::unique_ptr<base::File> file,
    const base::FilePath& path,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  VLOG(2) << "WriteBytesBlocking: {path = '" << path.value() << "', offset = '"
          << offset << "', length = '" << length << "'}";

  if (!file) {
    file = std::make_unique<base::File>(
        path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  }

  if (file->Write(offset, buffer->data(), length) != length) {
    PLOG(ERROR) << "Failed to write bytes to file";
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  }

  return file;
}

FileErrorOrFileAndBytesRead ReadBytesBlocking(
    std::unique_ptr<base::File> file,
    const base::FilePath& path,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length) {
  if (!file) {
    file = std::make_unique<base::File>(
        path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }
  int bytes_read = file->Read(offset, buffer->data(), length);
  if (bytes_read < 0) {
    PLOG(ERROR) << "Failed to read bytes from file";
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  }

  VLOG(2) << "ReadBytesBlocking: {bytes_read = '" << bytes_read
          << "', file.GetLength = '" << file->GetLength() << "', offset = '"
          << offset << "', length = '" << length << "'}";
  return std::make_pair(std::move(file), bytes_read);
}

}  // namespace

LocalFD::LocalFD(
    const base::FilePath& file_path,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : file_path_(file_path), blocking_task_runner_(blocking_task_runner) {
  VLOG(2) << "LocalFD instance created {file_path = '" << file_path_ << "'}";
}

LocalFD::~LocalFD() {
  DCHECK(!in_progress_operation_)
      << "Operation still pending when FD destroyed";
  VLOG(2) << "LocalFD instance destroyed {file_path = '" << file_path_
          << "'}";
  if (file_) {
    CloseFile();
  }
}

void LocalFD::WriteBytes(
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length,
    base::OnceCallback<void(base::File::Error)> callback) {
  if (in_progress_operation_) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }
  in_progress_operation_ = true;
  if (read_only_) {
    // In the event we've only ever read from this file but we're now writing to
    // the file, let's close the existing FD and instead start a new one that
    // opens for write.
    CloseFile();
  }
  read_only_ = false;
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteBytesBlocking, std::move(file_), file_path_, buffer,
                     offset, length),
      base::BindOnce(&LocalFD::OnBytesWritten, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void LocalFD::OnBytesWritten(
    base::OnceCallback<void(base::File::Error)> callback,
    FileErrorOrFile error_or_file) {
  base::File::Error result = error_or_file.error_or(base::File::FILE_OK);
  if (result == base::File::FILE_OK) {
    CloseOrCacheFile(std::move(error_or_file.value()));
  }
  std::move(callback).Run(result);
}

void LocalFD::ReadBytes(scoped_refptr<net::IOBuffer> buffer,
                          int64_t offset,
                          int length,
                          FileErrorOrBytesReadCallback callback) {
  if (in_progress_operation_) {
    std::move(callback).Run(base::unexpected(base::File::FILE_ERROR_IN_USE));
    return;
  }
  in_progress_operation_ = true;
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadBytesBlocking, std::move(file_), file_path_, buffer,
                     offset, length),
      base::BindOnce(&LocalFD::OnBytesRead, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void LocalFD::OnBytesRead(
    FileErrorOrBytesReadCallback callback,
    FileErrorOrFileAndBytesRead error_or_file_and_bytes_read) {
  base::File::Error result =
      error_or_file_and_bytes_read.error_or(base::File::FILE_OK);
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(base::unexpected(result));
    return;
  }

  auto [file, bytes_read] = std::move(error_or_file_and_bytes_read.value());
  CloseOrCacheFile(std::move(file));
  std::move(callback).Run(bytes_read);
}

void LocalFD::CloseOrCacheFile(std::unique_ptr<base::File> file) {
  in_progress_operation_ = false;
  if (!close_closure_.is_null()) {
    VLOG(2) << "File closed whilst reading/writing, closing FD";
    CloseFile();
  } else {
    VLOG(2) << "Caching FD for next operation";
    file_ = std::move(file);
  }
}

void LocalFD::Close(base::OnceClosure close_closure) {
  DCHECK(close_closure_.is_null());
  close_closure_ = std::move(close_closure);
  if (in_progress_operation_) {
    VLOG(2) << "FD is currently open, scheduling close";
    return;
  }

  VLOG(2) << "Directly closing FD";
  CloseFile();
}

void LocalFD::CloseFile() {
  if (close_closure_) {
    std::move(close_closure_).Run();
  }
  blocking_task_runner_->DeleteSoon(FROM_HERE, std::move(file_));
}

}  // namespace ash::file_system_provider
