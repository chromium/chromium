// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/fileapi/fallback_copy_in_foreign_file.h"

#include <tuple>

#include "base/files/file_error_or.h"
#include "base/files/safe_base_name.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"

namespace ash {

namespace {

constexpr size_t kBufferSize = 65536;  // 64 KiB.

storage::FileSystemURL CreateTempURL(const storage::FileSystemURL& dest_url) {
  static const char kTempPrefix[] = ".chromium_temp_";

  std::string token = base::UnguessableToken::Create().ToString();
  return dest_url.CreateSibling(*base::SafeBaseName::Create(
      base::FilePath::FromASCII(base::StrCat({kTempPrefix, token}))));
}

// Duplicates a FileSystemOperationContext, returning something (a unique_ptr)
// with independent ownership.
std::unique_ptr<storage::FileSystemOperationContext>
DuplicateFileSystemOperationContext(
    storage::FileSystemOperationContext& original) {
  return std::make_unique<storage::FileSystemOperationContext>(
      original.file_system_context(), original.task_runner());
}

struct FileAndInt {
  base::File file;
  int n;

  FileAndInt(base::File file_arg, int n_arg)
      : file(std::move(file_arg)), n(n_arg) {}
};

// Helps implement FallbackCopyInForeignFile.
//
// It holds the state as the original FallbackCopyInForeignFile call is
// implemented by hopping between TaskRunners as part of Chrome storage's async
// C++ API.
//
// Those callbacks also hop between the original TaskRunner (typically "the IO
// thread" which, despite its name, doesn't allow blocking I/O) and the
// FileSystemOperationContext::task_runner(), which may block.
//
// ----
//
// "Copy in a foreign file (a 'real' kernel-visible file, identified by a
// base::FilePath named src_file_path) to a virtual-file-location (identified
// by a storage::FileSystemURL named dest_url)" is implemented in stages,
// writing the bytes to a temp_url before moving temp_url over dest_url.
//
// temp_url is a sibling (in the FileSystemURL::CreateSibling sense) of the
// dest_url. Its base name joins kTempPrefix and a base::UnguessableToken (a
// 128 bit randomly generated number) in string form.
//
// The stages are:
//
//   1. A new, temporary, virtual file is created at temp_url.
//   2. The src_file_path is opened for reading (as a base::File) and the
//      temp_url is opened for writing (as a storage::FileStreamWriter).
//   3. Repeatedly Read from the source and Write to the temp_url. The loop
//      ends when Read hits EOF (End Of File).
//   4. Flush (if necessary) the FileStreamWriter.
//   5. Rename temp_url to dest_url.
//
// If a stage encounters an error then the remaining stages are not performed.
// Nonetheless, Copier::Finish will be called regardless of failure (early
// exit) or success. Copier::Finish will remove the temp_url virtual file if
// necessary and then run the original callback that was passed to
// FallbackCopyInForeignFile.
//
// ----
//
// The FileStreamWriter is created for temp_url instead of dest_url. The
// indirection minimizes the amount of time that the dest_url virtual file is
// in a partially-written state. Step 3 (the read-write copy loop) can be
// time-consuming and relatively unreliable, if the virtual file system is
// backed by the cloud instead of local disk.
class Copier {
 public:
  Copier(storage::AsyncFileUtil& async_file_util,
         std::unique_ptr<storage::FileSystemOperationContext> context,
         const base::FilePath& src_file_path,
         const storage::FileSystemURL& dest_url);
  ~Copier() = default;

  Copier(const Copier&) = delete;
  Copier& operator=(const Copier&) = delete;

  void Start(storage::AsyncFileUtil::StatusCallback callback);

 private:
  void OnEnsureFileExists(base::File::Error result, bool created);

  void OnOpen(base::File file);
  void CallRead();
  void OnRead(base::FileErrorOr<FileAndInt> result);
  void CallWrite(scoped_refptr<net::DrainableIOBuffer> drainable_buffer);
  void OnWrite(scoped_refptr<net::DrainableIOBuffer> drainable_buffer,
               int result);
  void CallFlushIfNecesssary();
  void OnFlush(int result);
  void OnGetFileInfo(base::File::Error result,
                     const base::File::Info& file_info);
  void OnMoveTempToDest(base::File::Error result);

  void Finish(base::File::Error result);

  // Per the async_file_util.h comments, the async_file_util_ reference stays
  // alive as long as the context_ is alive.
  const raw_ref<storage::AsyncFileUtil> async_file_util_;

  // This is the FileSystemOperationContext originally passed to
  // FallbackCopyInForeignFile.
  std::unique_ptr<storage::FileSystemOperationContext> context_;

  const base::FilePath src_file_path_;
  const storage::FileSystemURL dest_url_;
  const storage::FileSystemURL temp_url_;
  storage::AsyncFileUtil::StatusCallback callback_;

  bool temp_url_needs_deleting_;

  scoped_refptr<net::IOBuffer> io_buffer_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<storage::FileStreamWriter> fs_writer_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::File file_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

Copier::Copier(storage::AsyncFileUtil& async_file_util,
               std::unique_ptr<storage::FileSystemOperationContext> context,
               const base::FilePath& src_file_path,
               const storage::FileSystemURL& dest_url)
    : async_file_util_(async_file_util),
      context_(std::move(context)),
      src_file_path_(src_file_path),
      dest_url_(dest_url),
      temp_url_(CreateTempURL(dest_url)),
      temp_url_needs_deleting_(false),
      io_buffer_(base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize)),
      fs_writer_(nullptr),
      file_(base::File::FILE_ERROR_FAILED) {}

void Copier::Start(storage::AsyncFileUtil::StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (src_file_path_.empty() || !dest_url_.is_valid() ||
      !temp_url_.is_valid()) {
    std::move(callback).Run(base::File::Error::FILE_ERROR_FAILED);
    return;
  }

  callback_ = std::move(callback);

  async_file_util_->EnsureFileExists(
      DuplicateFileSystemOperationContext(*context_), temp_url_,
      base::BindOnce(&Copier::OnEnsureFileExists,
                     // base::Unretained is safe as callback_ owns this.
                     base::Unretained(this)));
}

void Copier::OnEnsureFileExists(base::File::Error result, bool created) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result != base::File::Error::FILE_OK) {
    Finish(result);
    return;
  }

  temp_url_needs_deleting_ = true;
  fs_writer_ =
      context_->file_system_context()->CreateFileStreamWriter(temp_url_, 0);

  static constexpr auto open_file_off_the_io_thread =
      [](const base::FilePath src_file_path) {
        return base::File(src_file_path,
                          base::File::FLAG_OPEN | base::File::FLAG_READ);
      };

  context_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(open_file_off_the_io_thread, src_file_path_),
      base::BindOnce(&Copier::OnOpen,
                     // base::Unretained is safe as callback_ owns this.
                     base::Unretained(this)));
}

void Copier::OnOpen(base::File file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!file.IsValid()) {
    Finish(base::File::Error::FILE_ERROR_NOT_FOUND);
    return;
  }

  file_ = std::move(file);
  CallRead();
}

void Copier::CallRead() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static constexpr auto read_file_off_the_io_thread =
      [](scoped_refptr<net::IOBuffer> buffer,
         base::File file) -> base::FileErrorOr<FileAndInt> {
    std::optional<size_t> num_bytes_read =
        file.ReadAtCurrentPosNoBestEffort(buffer->span().first(kBufferSize));
    if (!num_bytes_read.has_value()) {
      return base::unexpected(base::File::GetLastFileError());
    }
    return FileAndInt(std::move(file),
                      base::checked_cast<int>(num_bytes_read.value()));
  };

  scoped_refptr<net::IOBuffer> buffer = io_buffer_;
  base::File file = std::move(file_);

  context_->task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(read_file_off_the_io_thread, std::move(buffer),
                     std::move(file)),
      base::BindOnce(&Copier::OnRead,
                     // base::Unretained is safe as callback_ owns this.
                     base::Unretained(this)));
}

void Copier::OnRead(base::FileErrorOr<FileAndInt> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    Finish(result.error());
    return;
  }

  file_ = std::move(result->file);

  if (int num_bytes_read = result->n; num_bytes_read > 0) {
    scoped_refptr<net::DrainableIOBuffer> drainable_buffer =
        base::MakeRefCounted<net::DrainableIOBuffer>(io_buffer_.get(),
                                                     num_bytes_read);
    CallWrite(std::move(drainable_buffer));
  } else {
    CallFlushIfNecesssary();
  }
}

void Copier::CallWrite(scoped_refptr<net::DrainableIOBuffer> drainable_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto pair = base::SplitOnceCallback(
      base::BindOnce(&Copier::OnWrite,
                     // base::Unretained is safe as callback_ owns this.
                     base::Unretained(this), drainable_buffer));

  int write_result = fs_writer_->Write(drainable_buffer.get(),
                                       drainable_buffer->BytesRemaining(),
                                       std::move(pair.first));
  if (write_result != net::ERR_IO_PENDING) {  // The write was synchronous.
    std::move(pair.second).Run(write_result);
  }
}

void Copier::OnWrite(scoped_refptr<net::DrainableIOBuffer> drainable_buffer,
                     int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result < 0) {
    Finish(storage::NetErrorToFileError(result));
    return;
  } else if (result > drainable_buffer->BytesRemaining()) {
    NOTREACHED_IN_MIGRATION();
  }

  drainable_buffer->DidConsume(result);
  if (drainable_buffer->BytesRemaining() > 0) {
    CallWrite(std::move(drainable_buffer));
    return;
  }

  CallRead();
}

void Copier::CallFlushIfNecesssary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (temp_url_.mount_option().flush_policy() ==
      storage::FlushPolicy::NO_FLUSH_ON_COMPLETION) {
    OnFlush(net::OK);
    return;
  }

  auto pair = base::SplitOnceCallback(
      base::BindOnce(&Copier::OnFlush,
                     // base::Unretained is safe as callback_ owns this.
                     base::Unretained(this)));

  int flush_result =
      fs_writer_->Flush(storage::FlushMode::kEndOfFile, std::move(pair.first));
  if (flush_result != net::ERR_IO_PENDING) {  // The flush was synchronous.
    std::move(pair.second).Run(flush_result);
  }
}

void Copier::OnFlush(int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result < 0) {
    Finish(storage::NetErrorToFileError(result));
    return;
  }

  // Destroying this storage::FileStreamWriter closes the temp_url file. We're
  // about to rename (or delete, on failure) that file, having finished writing
  // and flushing its contents, but some AsyncFileUtil implementations disallow
  // that on files that are still open.
  fs_writer_.reset();

  async_file_util_->GetFileInfo(
      DuplicateFileSystemOperationContext(*context_), dest_url_,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
      base::BindOnce(&Copier::OnGetFileInfo,
                     // base::Unretained is safe as callback_ owns this.
                     base::Unretained(this)));
}

void Copier::OnGetFileInfo(base::File::Error result,
                           const base::File::Info& file_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result == base::File::FILE_ERROR_NOT_FOUND) {
    // No-op. It's OK if the dest_url_ didn't already exist.
  } else if (result != base::File::FILE_OK) {
    Finish(result);
    return;
  } else if (file_info.is_directory) {
    Finish(base::File::FILE_ERROR_NOT_A_FILE);
    return;
  }

  async_file_util_->MoveFileLocal(
      DuplicateFileSystemOperationContext(*context_), temp_url_, dest_url_,
      storage::FileSystemOperation::CopyOrMoveOptionSet(),
      base::BindOnce(&Copier::OnMoveTempToDest,
                     // base::Unretained is safe as callback_ owns this.
                     base::Unretained(this)));
}

void Copier::OnMoveTempToDest(base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  temp_url_needs_deleting_ = (result != base::File::Error::FILE_OK);
  Finish(result);
}

void Copier::Finish(base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!temp_url_needs_deleting_) {
    std::move(callback_).Run(result);
    return;
  }

  // Normally, the storage::FileStreamWriter is created in OnEnsureFileExists
  // and destroyed in OnFlush. However, Finish is still called if an error
  // occurs in between. Just like the fs_writer_.reset() call in OnFlush, we
  // have to call the storage::FileStreamWriter destructor to close the
  // temp_url file before we try to delete that file.
  //
  // If we failed before OnEnsureFileExists or after OnFlush, reset is
  // idempotent. A second reset is harmless.
  fs_writer_.reset();

  async_file_util_->DeleteFile(
      DuplicateFileSystemOperationContext(*context_), temp_url_,
      base::BindOnce(
          [](storage::AsyncFileUtil::StatusCallback callback,
             base::File::Error finish_result,
             base::File::Error delete_file_result) {
            // Ignore delete_file_result. It's like the "-f" in "rm -f". Just
            // run the callback originally passed to FallbackCopyInForeignFile.
            std::move(callback).Run(finish_result);
          },
          std::move(callback_), result));
}

}  // namespace

void FallbackCopyInForeignFile(
    storage::AsyncFileUtil& async_file_util,
    std::unique_ptr<storage::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const storage::FileSystemURL& dest_url,
    storage::AsyncFileUtil::StatusCallback callback) {
  CHECK(context);
  CHECK(callback);

  std::unique_ptr<Copier> copier = std::make_unique<Copier>(
      async_file_util, std::move(context), src_file_path, dest_url);

  // Allows the unique pointer to be bound to the lambda below so the Copier
  // stays alive until the operation completes.
  Copier* raw_copier = copier.get();

  raw_copier->Start(base::BindOnce(
      [](std::unique_ptr<Copier> copier,
         storage::AsyncFileUtil::StatusCallback callback,
         base::File::Error result) { std::move(callback).Run(result); },
      std::move(copier), std::move(callback)));
}

}  // namespace ash
