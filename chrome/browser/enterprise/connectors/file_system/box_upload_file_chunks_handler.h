// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOAD_FILE_CHUNKS_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOAD_FILE_CHUNKS_HANDLER_H_

#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/hash/sha1.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_flow.h"

namespace enterprise_connectors {

// Helper class to read and prepare file content for chunked uploads to Box.
// API reference: https://developer.box.com/guides/uploads/chunked/.
//
// Note that although this class creates file digest using SHA-1, it is not
// meant for cryptographic security, so the corresponding API requests must be
// made via https for security/privacy. It uses SHA-1 only because Box requires
// SHA-1 for file integrity check, not to detect MitM attacks.
class BoxChunkedUploader::FileChunksHandler {
 public:
  // Arg: PartInfo for the last chunk read.
  using FilePartiallyReadCallback = base::RepeatingCallback<void(PartInfo)>;
  // Arg: SHA-1 digest of the entire file.
  using FileCompletelyReadCallback =
      base::OnceCallback<void(const std::string&)>;
  FileChunksHandler(const base::FilePath& path,
                    const size_t file_size,
                    const size_t chunk_size);
  ~FileChunksHandler();

  // Kick off reading of the file; this class calls |file_partially_read_cb|
  // each time a chunk is read, including the last chunk. After the last chunk
  // is read and OnPartUploaded() is called, owner of this class is notified via
  // |file_completedly_read_cb|.
  void StartReading(FilePartiallyReadCallback file_partially_read_cb,
                    FileCompletelyReadCallback file_completely_read_cb);
  // Called by owner of this class to continue to read the next chunk.
  void ContinueToReadChunk(size_t n);

  void SkipToOnFileChunkReadForTesting(
      std::string content,
      int bytes_read,
      FilePartiallyReadCallback file_partially_read_cb,
      FileCompletelyReadCallback file_completely_read_cb);

 private:
  using FileStatus = base::File::Error;
  void ReadIfValid(FileStatus file_status);
  void Read();
  void OnFileChunkRead(int bytes_read);
  void OnFileCompletelyRead(FileStatus file_status);

  // Helper methods to check if file open operation succeeded, and log failures.
  // Arg: bool indicates whether the file read was successful.
  using FileCheckCallback = base::OnceCallback<void(FileStatus)>;
  void CheckFileError(FileCheckCallback cb);
  void OnFileChecked(FileCheckCallback cb, bool file_valid);
  void OnFileError(FileCheckCallback cb, FileStatus file_status);

  base::SequenceBound<base::File> sequenced_file_;
  size_t chunks_read_{0U};  // # of chunks read; checked with byte_from/to_.
  // Byte range for the chunk read into chunk_content_, indexed as [0, size):
  size_t byte_from_{0U};        // Inclusive of 1st byte in the chunk read.
  size_t byte_to_{0U};          // Exclusive of last byte in the chunk read.
  std::string chunk_content_;   // Each chunk is byte [byte_from_, byte_to).
  base::SHA1Context sha1_ctx_;  // Streaming SHA-1; updated by each chunk read.
  // After the last chunk is read, sha1_ctx_ is finalized into sha1_digest,
  // passed back to owner via |file_completely_read_cb| provided in
  // StartReading().

  FilePartiallyReadCallback file_partially_read_cb_;
  FileCompletelyReadCallback file_completely_read_cb_;
  const size_t file_size_;   // Size of the entire file.
  const size_t chunk_size_;  // Size for each file chunk to be read/uploaded.

  base::WeakPtrFactory<FileChunksHandler> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_BOX_UPLOAD_FILE_CHUNKS_HANDLER_H_
