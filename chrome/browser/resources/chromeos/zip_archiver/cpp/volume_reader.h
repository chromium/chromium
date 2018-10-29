// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_READER_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_READER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/files/file.h"

// Defines a reader for archive volumes. This class is used by minizip
// for custom reads.
class VolumeReader {
 public:
  virtual ~VolumeReader() {}

  // Tries to read bytes_to_read from the archive. The result will be stored at
  // *destination_buffer, which is the address of a buffer handled by
  // VolumeReaderJavaScriptStream. *destination_buffer must be available until
  // the next VolumeReader:Read call or until VolumeReader is destructed.
  //
  // The operation must be synchronous (minizip requirement), so it
  // should NOT be done on the main thread. bytes_to_read should be > 0.
  //
  // Returns the actual number of read bytes or -1 in case of failure.
  virtual int64_t Read(int64_t bytes_to_read,
                       const void** destination_buffer) = 0;

  // Tries to seek to offset from whence. Returns the resulting offset location
  // or -1 in case of errors. Similar to
  // http://www.cplusplus.com/reference/cstdio/fseek/
  virtual int64_t Seek(int64_t offset, base::File::Whence whence) = 0;

  // Fetches a passphrase for reading. If the passphrase is not available it
  // returns nullptr.
  virtual std::unique_ptr<std::string> Passphrase() = 0;

  virtual int64_t offset() = 0;

  virtual int64_t archive_size() = 0;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_READER_H_
