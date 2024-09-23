// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/scoped_file_opener.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"

namespace ash::file_system_provider {

using OpenFileCallback = ProvidedFileSystemInterface::OpenFileCallback;

// Opens and closes files automatically. Extracted from ScopedFileOpener to
// be able to make ScopedFileOpener *not* ref counted.
class ScopedFileOpener::Runner
    : public base::RefCounted<ScopedFileOpener::Runner> {
 public:
  static scoped_refptr<Runner> Create(ProvidedFileSystemInterface* file_system,
                                      const base::FilePath& file_path,
                                      OpenFileMode mode,
                                      OpenFileCallback callback) {
    auto runner =
        base::WrapRefCounted(new Runner(file_system, std::move(callback)));
    runner->abort_callback_ = file_system->OpenFile(
        file_path, mode,
        base::BindOnce(&ScopedFileOpener::Runner::OnOpenFileCompleted, runner));
    return runner;
  }

  // Aborts pending open operation, or closes a file if it's already opened.
  // Called from ScopedFileOpener::~ScopedFileOpener.
  void AbortOrClose() {
    if (!open_completed_) {
      aborting_requested_ = true;
      std::move(abort_callback_).Run();
      return;
    }

    if (open_completed_ && file_handle_ != 0) {
      if (file_system_.get()) {
        file_system_->CloseFile(
            file_handle_,
            base::BindOnce(
                &ScopedFileOpener::Runner::OnCloseFileAfterAbortCompleted, this,
                file_handle_));
      }
      return;
    }

    // Otherwise nothing to abort nor to close - the opening process has
    // completed, but the file failed to open, so there is no need to close it.
    DCHECK(open_completed_ && file_handle_ == 0);
  }

 private:
  friend class base::RefCounted<ScopedFileOpener::Runner>;

  Runner(ProvidedFileSystemInterface* file_system, OpenFileCallback callback)
      : file_system_(file_system->GetWeakPtr()),
        open_callback_(std::move(callback)),
        aborting_requested_(false),
        open_completed_(false),
        file_handle_(0) {}

  ~Runner() = default;

  // Called when opening is completed with either a success or an error.
  void OnOpenFileCompleted(int file_handle,
                           base::File::Error result,
                           std::unique_ptr<EntryMetadata> metadata) {
    open_completed_ = true;

    if (result != base::File::FILE_OK) {
      CallOpenCallbackOnce(file_handle, result);
      return;
    }

    file_handle_ = file_handle;
    if (aborting_requested_ && file_system_.get()) {
      // The opening request has been aborted earlier, but the abort request
      // either hasn't arrived yet the extension, or it abort request events are
      // not handled by the extension. In either case, close the file now.
      file_system_->CloseFile(
          file_handle,
          base::BindOnce(
              &ScopedFileOpener::Runner::OnCloseFileAfterAbortCompleted, this,
              file_handle_));
      return;
    }

    DCHECK_EQ(base::File::FILE_OK, result);
    CallOpenCallbackOnce(file_handle, base::File::FILE_OK);
  }

  // Called when automatic closing a file is completed. It's performed when
  // a file is opened successfully after the abort callback is called.
  void OnCloseFileAfterAbortCompleted(int file_handle,
                                      base::File::Error result) {
    if (result == base::File::FILE_ERROR_ABORT) {
      // Closing is aborted, so the file is still open. Call the OpenFile
      // callback with a success error code and mark the file as opened.
      //
      // This is not good, as callers, such as file stream readers may expect
      // aborting to *always* work, and leave the file opened permanently.
      // The problem will go away once we remove the dialog to abort slow
      // operations. See: crbug.com/475355.
      CallOpenCallbackOnce(file_handle, base::File::FILE_OK);
      return;
    }

    // Call the OpenFile callback with the ABORT error code, as the file is not
    // opened anymore.
    CallOpenCallbackOnce(file_handle, base::File::FILE_ERROR_ABORT);
  }

  // Calls the |open_callback_| only once with a result for opening a file.
  void CallOpenCallbackOnce(int file_handle, base::File::Error result) {
    if (open_callback_.is_null())
      return;

    std::move(open_callback_)
        .Run(file_handle, result, /*cloud_file_info=*/nullptr);
  }

  base::WeakPtr<ProvidedFileSystemInterface> file_system_;
  OpenFileCallback open_callback_;
  AbortCallback abort_callback_;
  bool aborting_requested_;
  bool open_completed_;
  int file_handle_;
};

ScopedFileOpener::ScopedFileOpener(ProvidedFileSystemInterface* file_system,
                                   const base::FilePath& file_path,
                                   OpenFileMode mode,
                                   OpenFileCallback callback)
    : runner_(
          Runner::Create(file_system, file_path, mode, std::move(callback))) {}

ScopedFileOpener::~ScopedFileOpener() {
  runner_->AbortOrClose();
}

}  // namespace ash::file_system_provider
