// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/components/arc/mojom/file_system.mojom-forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner_util.h"
#include "net/base/completion_once_callback.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "url/gurl.h"

namespace base {
class File;
class SequencedTaskRunner;
}

namespace net {
class IOBuffer;
class IOBufferWithSize;
}

namespace arc {

// FileStreamReader implementation for ARC content file system.
class ArcContentFileSystemFileStreamReader : public storage::FileStreamReader {
 public:
  ArcContentFileSystemFileStreamReader(const GURL& arc_url, int64_t offset);

  ArcContentFileSystemFileStreamReader(
      const ArcContentFileSystemFileStreamReader&) = delete;
  ArcContentFileSystemFileStreamReader& operator=(
      const ArcContentFileSystemFileStreamReader&) = delete;

  ~ArcContentFileSystemFileStreamReader() override;

  // storage::FileStreamReader override:
  int Read(net::IOBuffer* buffer,
           int buffer_length,
           net::CompletionOnceCallback callback) override;
  int64_t GetLength(net::Int64CompletionOnceCallback callback) override;

 private:
  using CloseStatus = file_system_operation_runner_util::CloseStatus;

  // Called to close the ARC file descriptor when operations are completed.
  void CloseInternal(const CloseStatus status);

  // Actually performs read.
  void ReadInternal(net::IOBuffer* buffer,
                    int buffer_length,
                    net::CompletionOnceCallback callback);

  // Called when read completes.
  void OnRead(net::CompletionOnceCallback callback,
              std::optional<size_t> result);

  // Called when GetFileSize() completes.
  void OnGetFileSize(net::Int64CompletionOnceCallback callback, int64_t size);

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

  // Reads the contents of the file to reach the offset.
  void ConsumeFileContents(
      scoped_refptr<net::IOBuffer> buf,
      int buffer_length,
      net::CompletionOnceCallback callback,
      scoped_refptr<net::IOBufferWithSize> temporary_buffer,
      int64_t num_bytes_to_consume);

  // Called to handle read result for ConsumeFileContents().
  void OnConsumeFileContents(
      scoped_refptr<net::IOBuffer> buf,
      int buffer_length,
      net::CompletionOnceCallback callback,
      scoped_refptr<net::IOBufferWithSize> temporary_buffer,
      int64_t num_bytes_to_consume,
      std::optional<size_t> read_result);

  GURL arc_url_;
  int64_t offset_;
  std::string session_id_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  std::unique_ptr<base::File> file_;

  base::WeakPtrFactory<ArcContentFileSystemFileStreamReader> weak_ptr_factory_{
      this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_FILE_STREAM_READER_H_
