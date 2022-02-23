// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_SINGLE_FILE_TAR_READER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_SINGLE_FILE_TAR_READER_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
namespace image_writer {

class SingleFileTarReaderTest;

// SingleFileTarReader is a reader of tar archives with limited function. It
// only supports a tar archive with a single file entry. An archive with
// multiple files is rejected as error.
class SingleFileTarReader {
 public:
  enum class Result { kSuccess, kFailure, kShouldWait };

  // An interface that delegates file I/O of SingleFileTarReader.
  class Delegate {
   public:
    using Result = SingleFileTarReader::Result;

    virtual ~Delegate() = default;

    // Reads input data and returns kSuccess if it succeeds.
    // The input data will be written to |data|. |*size| is initially the size
    // of the |data| buffer. |*size| will be set to the amount actually read.
    // Returns kShouldWait if the data is still not available.
    // Returns kFailure and sets |error_id| if it fails.
    virtual Result ReadTarFile(char* data,
                               uint32_t* size,
                               std::string* error_id) = 0;

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
  // Returns kShouldWait if the input data is still not available. The caller
  // has to call ExtractChunk() again when the data is ready. The detail depends
  // on the implementation of the delegate.
  // Returns kFailure if it fails. error_id() identifies the reason of the
  // error.
  Result ExtractChunk();

  bool IsComplete() const;

  absl::optional<uint64_t> total_bytes() const { return total_bytes_; }
  uint64_t curr_bytes() const { return curr_bytes_; }

  const std::string& error_id() const { return error_id_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(SingleFileTarReaderTest, ReadOctalNumber);

  // Read a number in Tar file header. It is normally a null-terminated octal
  // ASCII number but can be big-endian integer with padding when GNU extension
  // is used. |length| must greater than 8.
  static uint64_t ReadOctalNumber(const char* buffer, size_t length);

  const raw_ptr<Delegate> delegate_;

  // Populated once the size has been parsed. The value 0 means the file in
  // the tar is empty.
  absl::optional<uint64_t> total_bytes_;
  uint64_t curr_bytes_ = 0;

  std::vector<char> buffer_;

  std::string error_id_;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_SINGLE_FILE_TAR_READER_H_
