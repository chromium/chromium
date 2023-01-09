// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_FILE_STREAM_FORWARDER_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_FILE_STREAM_FORWARDER_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"

namespace arc {

// FileStreamForwarder reads data of the given FileSystemURL, and writes it to
// the given FD. While all internal actions happen on the IO thread, all public
// methods should be called on the UI thread. If this object is destroyed before
// the completion, the in-flight operation may abort before finishing, and the
// callback will be called with false when aborted.
class FileStreamForwarder {
 public:
  struct DestroyHelper {
    void operator()(FileStreamForwarder* object) const { object->Destroy(); }
  };

  // |result| is true when the specified amount of data was successfully
  // forwarded.
  using ResultCallback = base::OnceCallback<void(bool result)>;

  // Starts reading the part of the file specified by |offset| and |size|, and
  // writes the data to |fd_dest|, and runs |callback| with true iff |size|
  // bytes of data were successfully read and written.
  FileStreamForwarder(scoped_refptr<storage::FileSystemContext> context,
                      const storage::FileSystemURL& url,
                      int64_t offset,
                      int64_t size,
                      base::ScopedFD fd_dest,
                      ResultCallback callback);

  FileStreamForwarder(const FileStreamForwarder&) = delete;
  FileStreamForwarder& operator=(const FileStreamForwarder&) = delete;

  // Posts a task to destruct this object on the IO thread.
  // Must not be called multiple times for the same object.
  void Destroy();

 private:
  // Use Destroy() to destruct this object.
  ~FileStreamForwarder();

  // Destructs this object.
  void DestroyOnIOThread();

  // Starts reading the data.
  void Start();

  // Does the actual reading.
  void DoRead();

  // Called when read is completed. |result| is the number of bytes read, or an
  // error code if it's negative.
  void OnReadCompleted(int result);

  // Called when write is completed.
  void OnWriteCompleted(bool result);

  // Runs the result callback on the UI thread.
  void NotifyCompleted(bool result);

  scoped_refptr<storage::FileSystemContext> context_;
  const storage::FileSystemURL url_;
  const int64_t offset_;
  int64_t remaining_size_;
  base::ScopedFD fd_dest_;
  ResultCallback callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;  // For blocking IO.
  scoped_refptr<net::IOBufferWithSize> buf_;
  std::unique_ptr<storage::FileStreamReader> stream_reader_;

  base::WeakPtrFactory<FileStreamForwarder> weak_ptr_factory_{this};
};

// FileStreamForwarderPtr is unique_ptr with a custom deleter. Use this to
// safely handle the ownership of FileStreamForwarder.
using FileStreamForwarderPtr =
    std::unique_ptr<FileStreamForwarder, FileStreamForwarder::DestroyHelper>;

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_FILE_STREAM_FORWARDER_H_
