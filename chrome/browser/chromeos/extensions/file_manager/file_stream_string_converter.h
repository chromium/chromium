// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_STREAM_STRING_CONVERTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_STREAM_STRING_CONVERTER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "net/base/io_buffer.h"

namespace storage {
class FileStreamReader;
}

namespace storage {

// Converts a file stream to a std::string.
class FileStreamStringConverter {
 public:
  using ResultCallback =
      base::OnceCallback<void(scoped_refptr<base::RefCountedString>)>;

  FileStreamStringConverter();
  ~FileStreamStringConverter();

  // Disallow Copy and Assign
  FileStreamStringConverter(const FileStreamStringConverter&) = delete;
  FileStreamStringConverter& operator=(const FileStreamStringConverter&) =
      delete;

  // Converts a file stream of data read from the given |streamReader| to a
  // string.  The work occurs asynchronously, and the string is returned via the
  // |callback|.  If an error occurs, |callback| is called with an empty string.
  // Only one stream can be processed at a time by each converter.  Do not call
  // ConvertFileStreamToString before the results of a previous call have been
  // returned.
  void ConvertFileStreamToString(
      std::unique_ptr<storage::FileStreamReader> stream_reader,
      ResultCallback callback);

 private:
  // Kicks off a read of the next chunk from the stream.
  void ReadNextChunk();
  // Handles the incoming chunk of data from a stream read.
  void OnChunkRead(int bytes_read);

  // Maximum chunk size for read operations.
  std::unique_ptr<storage::FileStreamReader> reader_;
  scoped_refptr<net::IOBuffer> buffer_;
  ResultCallback callback_;
  std::string local_buffer_;
};

}  // namespace storage

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_STREAM_STRING_CONVERTER_H_
