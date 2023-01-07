// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_STREAM_MD5_DIGESTER_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_STREAM_MD5_DIGESTER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/hash/md5.h"
#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"

namespace storage {
class FileStreamReader;
}

namespace drive {
namespace util {

// Computes the (base-16 encoded) MD5 digest of data extracted from a file
// stream.
class FileStreamMd5Digester
    : public base::RefCountedThreadSafe<FileStreamMd5Digester> {
 public:
  using ResultCallback = base::OnceCallback<void(std::string)>;

  FileStreamMd5Digester();

  FileStreamMd5Digester(const FileStreamMd5Digester&) = delete;
  FileStreamMd5Digester& operator=(const FileStreamMd5Digester&) = delete;

  // Computes an MD5 digest of data read from the given |streamReader|.  The
  // work occurs asynchronously, and the resulting hash is returned via the
  // |callback|.  If an error occurs, |callback| is called with an empty string.
  // Only one stream can be processed at a time by each digester.  Do not call
  // GetMd5Digest before the results of a previous call have been returned.
  void GetMd5Digest(std::unique_ptr<storage::FileStreamReader> stream_reader,
                    ResultCallback callback);

 private:
  friend class base::RefCountedThreadSafe<FileStreamMd5Digester>;
  ~FileStreamMd5Digester();

  // Kicks off a read of the next chunk from the stream.
  void ReadNextChunk();
  // Handles the incoming chunk of data from a stream read.
  void OnChunkRead(int bytes_read);

  // Maximum chunk size for read operations.
  std::unique_ptr<storage::FileStreamReader> reader_;
  scoped_refptr<net::IOBuffer> buffer_;
  base::MD5Context md5_context_;
  ResultCallback callback_;
};

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_FILE_STREAM_MD5_DIGESTER_H_
