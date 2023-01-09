// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_STREAM_ADAPTER_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_STREAM_ADAPTER_H_

#include <memory>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_context.h"

namespace arc {

// ShareInfoFileStreamAdapter will stream read data based on the provided
// FileSystemURL container for a file in a ARC virtual filesystem (not
// backed by the Linux VFS). It will convert the transferred data by writing to
// a scoped file descriptor. This class is instantiated and lives on the UI
// thread. Internal file stream functions and object destruction is on the IO
// thread. Errors that occur mid-stream will abort and cleanup.
class ShareInfoFileStreamAdapter
    : public base::RefCountedThreadSafe<
          ShareInfoFileStreamAdapter,
          content::BrowserThread::DeleteOnIOThread> {
 public:
  // |result| denotes whether the requested size of data in bytes was streamed.
  using ResultCallback = base::OnceCallback<void(bool result)>;

  // Constructor used when streaming into file descriptor.
  ShareInfoFileStreamAdapter(scoped_refptr<storage::FileSystemContext> context,
                             const storage::FileSystemURL& url,
                             int64_t offset,
                             int64_t file_size,
                             int buf_size,
                             base::ScopedFD dest_fd,
                             ResultCallback result_callback);

  ShareInfoFileStreamAdapter(const ShareInfoFileStreamAdapter&) = delete;
  ShareInfoFileStreamAdapter& operator=(const ShareInfoFileStreamAdapter&) =
      delete;

  // Create and start task runner for blocking IO threads.
  void StartRunner();

  // Used only for testing.
  void StartRunnerForTesting();

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::IO>;
  friend class base::DeleteHelper<ShareInfoFileStreamAdapter>;

  ~ShareInfoFileStreamAdapter();

  // Used to stream content to file.
  void StartFileStreaming();
  void PerformReadFileStream();
  void WriteToFile(int bytes_read);

  // Called when intermediate read transfers actual size of |bytes_read|.
  void OnReadFile(int bytes_read);

  // Called when intermediate write is finished.
  void OnWriteFinished(bool result);

  // Called when all transactions are completed to notify caller.
  void OnStreamingFinished(bool result);

  scoped_refptr<storage::FileSystemContext> context_;
  const storage::FileSystemURL url_;
  const int64_t offset_;
  int64_t bytes_remaining_;
  base::ScopedFD dest_fd_;
  ResultCallback result_callback_;

  std::unique_ptr<storage::FileStreamReader> stream_reader_;
  // Use task runner for blocking IO.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Contains the latest data read from |stream_reader_|, which will be written
  // to |dest_fd_|.
  scoped_refptr<net::IOBufferWithSize> net_iobuf_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ShareInfoFileStreamAdapter> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_SHARE_INFO_FILE_STREAM_ADAPTER_H_
