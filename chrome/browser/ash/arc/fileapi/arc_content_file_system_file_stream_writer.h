// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_WRITER_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_WRITER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner_util.h"
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

  ArcContentFileSystemFileStreamWriter(
      const ArcContentFileSystemFileStreamWriter&) = delete;
  ArcContentFileSystemFileStreamWriter& operator=(
      const ArcContentFileSystemFileStreamWriter&) = delete;

  ~ArcContentFileSystemFileStreamWriter() override;

  // storage::FileStreamWriter override:
  int Write(net::IOBuffer* buffer,
            int buffer_length,
            net::CompletionOnceCallback callback) override;
  int Cancel(net::CompletionOnceCallback callback) override;
  int Flush(storage::FlushMode flush_mode,
            net::CompletionOnceCallback callback) override;

 private:
  using CloseStatus = file_system_operation_runner_util::CloseStatus;

  // Called to close the ARC file descriptor when operations are completed.
  void CloseInternal(const CloseStatus status);

  // Actually performs write.
  void WriteInternal(net::IOBuffer* buffer,
                     int buffer_length,
                     net::CompletionOnceCallback callback);

  // Called when write completes.
  void OnWrite(net::CompletionOnceCallback callback,
               std::optional<size_t> result);

  // Called when opening file session completes.
  void OnOpenFileSession(scoped_refptr<net::IOBuffer> buf,
                         int buffer_length,
                         net::CompletionOnceCallback callback,
                         mojom::FileSessionPtr maybe_file_handle);

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
  std::string session_id_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<base::File> file_;
  bool has_pending_operation_;
  net::CompletionOnceCallback cancel_callback_;

  base::WeakPtrFactory<ArcContentFileSystemFileStreamWriter> weak_ptr_factory_{
      this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_WRITER_H_
