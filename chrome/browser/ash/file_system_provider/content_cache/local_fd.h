// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_LOCAL_FD_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_LOCAL_FD_H_

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"

namespace ash::file_system_provider {

using FileErrorOrFileAndBytesRead =
    base::FileErrorOr<std::pair<std::unique_ptr<base::File>, int>>;

using FileErrorOrFile = base::FileErrorOr<std::unique_ptr<base::File>>;

using FileErrorOrBytesReadCallback =
    base::OnceCallback<void(base::FileErrorOr<int>)>;

// A thin wrapper around a `base::File` that enables re-use of the FD. On top of
// this, it ensures files are properly closed on their blocking task runner even
// if the events come in interleaved (e.g. a `ReadFile` is still responding when
// a `CloseFile` is received, the `CloseFile` will happen only after the
// `ReadFile` returns).
class LocalFD {
 public:
  LocalFD(const base::FilePath& file_path,
            scoped_refptr<base::SequencedTaskRunner> blocking_task_runner);

  LocalFD(const LocalFD&) = delete;
  LocalFD& operator=(const LocalFD&) = delete;

  ~LocalFD();

  // Write the bytes in `buffer` at `offset` for `length` into the underlying
  // `file_` that was opened. If `file_` is `nullptr` one will be created with
  // the appropriate base::File::Flags. In the event the file was previously
  // only opened for read, the old FD will get closed and a new one will be
  // opened for writing.
  void WriteBytes(scoped_refptr<net::IOBuffer> buffer,
                  int64_t offset,
                  int length,
                  base::OnceCallback<void(base::File::Error)> callback);

  // Read the bytes in `buffer` at `offset` for `length` into the underlying
  // `file_` that was opened. If `file_` is `nullptr` one will be created with
  // the appropriate base::File::Flags.
  void ReadBytes(scoped_refptr<net::IOBuffer> buffer,
                 int64_t offset,
                 int length,
                 FileErrorOrBytesReadCallback callback);

  // Attempts to close the open `file_`. The `close_closure` will be invoked
  // when the file is actually closed, this may not be immediately due to
  // operations that are still pending.
  void Close(base::OnceClosure close_closure);

 private:
  void OnBytesWritten(base::OnceCallback<void(base::File::Error)> callback,
                      FileErrorOrFile error_or_file);

  void OnBytesRead(FileErrorOrBytesReadCallback callback,
                   FileErrorOrFileAndBytesRead error_or_file_and_bytes_read);

  // A `base::File` is move-only, so in the event the file hasn't received a
  // `Close` call, move the `file` back into `file_`.
  void CloseOrCacheFile(std::unique_ptr<base::File> file);

  // Deletes the `file_` on the `blocking_task_runner_` (`base::File::~File`
  // runs close on destruction).
  void CloseFile();

  // Given the async nature of reading from a file, a `Close` can be
  // requested whilst an operation is still in-progress. To avoid losing the
  // close file attempt and thus keeping the `file_` around forever, keep track
  // of the close via `schedule_close_` and when `in_progress_operation_ =
  // false`, then close the file on completion.
  bool in_progress_operation_ = false;

  // Invoked when the file is atually closed.
  base::OnceClosure close_closure_;

  // If a `ReadBytes` operation is started, the underlying `file_` is in
  // read-only mode (i.e. `base::File::FLAG_READ`), in the event a `WriteBytes`
  // is attempted the `file_` will need to be re-opened for write operations and
  // this bool will be false.
  bool read_only_ = true;

  base::FilePath file_path_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<base::File> file_;

  base::WeakPtrFactory<LocalFD> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_LOCAL_FD_H_
