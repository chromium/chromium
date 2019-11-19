// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_FILE_STREAM_WRITER_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_FILE_STREAM_WRITER_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_url.h"

namespace chromeos {
namespace file_system_provider {

// Implements a streamed file writer. It is lazily initialized by the first call
// to Write().
class FileStreamWriter : public storage::FileStreamWriter {
 public:
  FileStreamWriter(const storage::FileSystemURL& url, int64_t initial_offset);

  ~FileStreamWriter() override;

  // storage::FileStreamWriter overrides.
  int Write(net::IOBuffer* buf,
            int buf_len,
            net::CompletionOnceCallback callback) override;
  int Cancel(net::CompletionOnceCallback callback) override;
  int Flush(net::CompletionOnceCallback callback) override;

 private:
  // Helper class for executing operations on the provided file system. All
  // of its methods must be called on UI thread. Callbacks are called on IO
  // thread.
  class OperationRunner;

  // State of the file stream writer.
  enum State {
    NOT_INITIALIZED,
    INITIALIZING,
    INITIALIZED,
    EXECUTING,
    FAILED,
    CANCELLING
  };

  // Called when OperationRunner::WriteOnUIThread is completed.
  void OnWriteFileCompleted(int buffer_length,
                            net::CompletionOnceCallback callback,
                            base::File::Error result);

  // Called when Write() operation is completed with either a success or an
  // error.
  void OnWriteCompleted(int result);

  // Initializes the writer by opening the file. When completed with success,
  // runs the |pending_closure|. Otherwise, calls the |error_callback|.
  void Initialize(base::OnceClosure pending_closure,
                  net::CompletionOnceCallback error_callback);

  // Called when opening a file is completed with either a success or an error.
  void OnOpenFileCompleted(base::OnceClosure pending_closure,
                           net::CompletionOnceCallback error_callback,
                           base::File::Error result);

  // Same as Write(), but called after initializing is completed.
  void WriteAfterInitialized(scoped_refptr<net::IOBuffer> buffer,
                             int buffer_length,
                             net::CompletionOnceCallback callback);

  net::CompletionOnceCallback write_callback_;
  storage::FileSystemURL url_;
  int64_t current_offset_;
  scoped_refptr<OperationRunner> runner_;
  State state_;

  base::WeakPtrFactory<FileStreamWriter> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FileStreamWriter);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_FILEAPI_FILE_STREAM_WRITER_H_
