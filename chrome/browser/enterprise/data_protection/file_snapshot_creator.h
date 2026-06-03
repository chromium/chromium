// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_FILE_SNAPSHOT_CREATOR_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_FILE_SNAPSHOT_CREATOR_H_

#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_system_url.h"
namespace storage {
class FileSystemContext;
}  // namespace storage

namespace enterprise_data_protection {

// Create a local snapshot of a virtual file on disk, and passes its path to a
// callback
class FileSnapshotCreator
    : public base::RefCountedThreadSafe<FileSnapshotCreator> {
 public:
  using SnapshotCreatedCallback =
      base::OnceCallback<void(const base::FilePath&)>;

  // Exposed for testing.
  static constexpr size_t CHUNK_SIZE = 64 * 1024;

  // Uses the FileSystemContext to create a local snapshot of a virtual file
  // with the virtual path in `url`. The local path of the file is passed as an
  // argument to `callback`.
  static void Start(scoped_refptr<storage::FileSystemContext> context,
                    const storage::FileSystemURL& url,
                    SnapshotCreatedCallback callback);

  FileSnapshotCreator(scoped_refptr<storage::FileSystemContext> context,
                      const storage::FileSystemURL& url,
                      SnapshotCreatedCallback ui_callback);

 private:
  friend base::RefCountedThreadSafe<FileSnapshotCreator>;

  ~FileSnapshotCreator();

  void StartOnIOThread();

  // This function only runs if the file snapshot creation fails at any point.
  // It ensures that any artifacts are deleted from disc.
  void CompleteWithError();

  // Create a temporary file on disc. This must run in a blocking thread.
  void CreateTempFile();

  // Files are read in chunks, in order to avoid out-of-memory errors.
  void ReadNextChunk();

  // Callback for FileStreamReader::Read().
  void OnReadCompleted(int bytes_read);

  // Write the chunk read in ReadNextChunk() to the temp file. Also called on a
  // blocking thread.
  void WriteChunkToFile(int bytes_to_write);

  // Close the file and run the callback.
  void CloseFileAndFinish();

  scoped_refptr<storage::FileSystemContext> context_;
  storage::FileSystemURL url_;
  SnapshotCreatedCallback ui_callback_;
  std::unique_ptr<storage::FileStreamReader> reader_;
  scoped_refptr<net::IOBufferWithSize> buffer_;
  base::FilePath local_path_;
  base::File file_;
};
}  // namespace enterprise_data_protection

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_FILE_SNAPSHOT_CREATOR_H_
