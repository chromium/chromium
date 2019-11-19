// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_WRITER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_WRITER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/system/handle.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "url/gurl.h"

namespace base {
class File;
class SequencedTaskRunner;
}  // namespace base

namespace net {
class IOBuffer;
}  // namespace net

namespace arc {

// FileStreamWriter implementation for ARC content file system.
class ArcContentFileSystemFileStreamWriter : public storage::FileStreamWriter {
 public:
  ArcContentFileSystemFileStreamWriter(const GURL& arc_url, int64_t offset);
  ~ArcContentFileSystemFileStreamWriter() override;

  // storage::FileStreamReader override:
  int Write(net::IOBuffer* buffer,
            int bufffer_length,
            net::CompletionOnceCallback callback) override;
  int Cancel(net::CompletionOnceCallback callback) override;
  int Flush(net::CompletionOnceCallback callback) override;

 private:
  // Actually performs read.
  void WriteInternal(net::IOBuffer* buffer,
                     int buffer_length,
                     net::CompletionOnceCallback callback);

  // Called when read completes.
  void OnWrite(net::CompletionOnceCallback callback, int result);

  // Called when opening file completes.
  void OnOpenFile(scoped_refptr<net::IOBuffer> buf,
                  int buffer_length,
                  net::CompletionOnceCallback callback,
                  mojo::ScopedHandle handle);

  // Called when seek completes.
  void OnSeekFile(scoped_refptr<net::IOBuffer> buf,
                  int buffer_length,
                  net::CompletionOnceCallback callback,
                  int seek_result);

  // Called when flush completes
  void OnFlushFile(net::CompletionOnceCallback callback, int flush_result);

  // Stops the in-flight operation and calls |cancel_callback_| if it has been
  // set by Cancel() for the current operation.
  bool CancelIfRequested();

  const GURL arc_url_;
  const int64_t offset_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<base::File> file_;
  bool has_pending_operation_;
  net::CompletionOnceCallback cancel_callback_;

  base::WeakPtrFactory<ArcContentFileSystemFileStreamWriter> weak_ptr_factory_{
      this};
  DISALLOW_COPY_AND_ASSIGN(ArcContentFileSystemFileStreamWriter);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_WRITER_H_
