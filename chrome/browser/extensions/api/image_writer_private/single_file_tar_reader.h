// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_SINGLE_FILE_TAR_READER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_SINGLE_FILE_TAR_READER_H_

#include "base/gtest_prod_util.h"

#include <string>
#include <vector>

namespace extensions {
namespace image_writer {

class SingleFileTarReaderTest;

// SingleFileTarReader is a reader of tar archives with limited function. It
// only supports a tar archive with a single file entry. An archive with
// multiple files is rejected as error.
class SingleFileTarReader {
 public:
  // An interface that delegates file I/O of SingleFileTarReader.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Reads input data and returns the number of bytes that is actually read.
    // The input data will be written to |data|. |size| is the size of the
    // |data| buffer. Usually the return value is same as |size|, but it can be
    // smaller than |size| at the end of the file.
    // Returns a negative number and sets |error_id| if it fails.
    virtual int ReadTarFile(char* data, int size, std::string* error_id) = 0;

    // Writes the passed data. |size| is the size of the |data| buffer.
    // Returns false and sets |error_id| if it fails.
    virtual bool WriteContents(const char* data,
                               int size,
                               std::string* error_id) = 0;
  };

  explicit SingleFileTarReader(Delegate* delegate);
  SingleFileTarReader(const SingleFileTarReader&) = delete;
  SingleFileTarReader& operator=(const SingleFileTarReader&) = delete;
  ~SingleFileTarReader();

  // Extracts a chunk of the tar file. To fully extract the file, the caller has
  // to repeatedly call this function until IsComplete() returns true.
  // Returns false if it fails. error_id() identifies the reason of the error.
  bool ExtractChunk();

  bool IsComplete() const;

  uint64_t total_bytes() const { return total_bytes_; }
  uint64_t curr_bytes() const { return curr_bytes_; }

  const std::string& error_id() const { return error_id_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SingleFileTarReaderTest, ReadOctalNumber);

  // Read a number in Tar file header. It is normally a null-terminated octal
  // ASCII number but can be big-endian integer with padding when GNU extension
  // is used. |length| must greater than 8.
  static uint64_t ReadOctalNumber(const char* buffer, size_t length);

  Delegate* const delegate_;

  uint64_t total_bytes_ = 0;
  uint64_t curr_bytes_ = 0;

  std::vector<char> buffer_;

  std::string error_id_;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_SINGLE_FILE_TAR_READER_H_
