// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_ARCHIVE_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_ARCHIVE_H_

#include <time.h>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_reader.h"

// Defines a wrapper for operations executed on an archive. API is not meant
// to be thread safe and its methods shouldn't be called in parallel.
class VolumeArchive {
 public:
  explicit VolumeArchive(std::unique_ptr<VolumeReader> reader);

  virtual ~VolumeArchive();

  // For functions that need to return more than pass/fail results.
  enum Result {
    RESULT_SUCCESS,
    RESULT_EOF,
    RESULT_FAIL,
  };

  // Initializes VolumeArchive. Should be called only once.
  // In case of any errors, the error message can be obtained with
  // VolumeArchive::error_message(). Encoding is the default encoding. Note,
  // that other encoding may be used if specified in the archive file.
  virtual bool Init(const std::string& encoding) = 0;

  // Gets the next header. If path_name is set to nullptr, then there are no
  // more available headers. Returns true if reading next header was successful.
  // In case of failure the error message can be obtained with
  // VolumeArchive::error_message().
  virtual VolumeArchive::Result GetCurrentFileInfo(
      std::string* path_name,
      bool* isEncodedInUtf8,
      int64_t* size,
      bool* is_directory,
      time_t* modification_time) = 0;

  virtual VolumeArchive::Result GoToNextFile() = 0;

  // Seeks to the header whose pathname is path_name.
  virtual bool SeekHeader(const std::string& path_name) = 0;

  // Gets data from offset to offset + length for the file reached with
  // VolumeArchive::GetNextHeader. The data is stored in an internal buffer
  // in the implementation of VolumeArchive and it will be returned
  // via *buffer parameter to avoid an extra copy. *buffer is owned by
  // VolumeArchive.
  //
  // Supports file seek by using the offset parameter. In case offset is less
  // then last VolumeArchive::ReadData offset, then the read will be restarted
  // from the beginning of the archive.
  //
  // For improving perfomance use VolumeArchive::MaybeDecompressAhead. Using
  // VolumeArchive::MaybeDecompressAhead is not mandatory, but without it
  // performance will suffer.
  //
  // The API assumes offset >= 0 and length > 0. length can be as big as
  // possible, but its up to the implementation to avoid big memory usage.
  // It can return up to length bytes of data, however 0 is returned only in
  // case of EOF.
  //
  // Returns the actual number of read bytes. The API ensures that *buffer will
  // have available as many bytes as returned. In case of failure, returns a
  // negative value and the error message can be obtained with
  // VolumeArchive::error_message().
  virtual int64_t ReadData(int64_t offset,
                           int64_t length,
                           const char** buffer) = 0;

  // Decompress ahead in case there are no more available bytes in the internal
  // buffer.
  virtual void MaybeDecompressAhead() = 0;

  VolumeReader* reader() const { return reader_.get(); }
  std::string error_message() const { return error_message_; }

 protected:
  void set_error_message(const std::string& error_message) {
    error_message_ = error_message;
  }

 private:
  // The reader that actually reads the archive data.
  std::unique_ptr<VolumeReader> reader_;
  std::string error_message_;  // An error message set in case of any errors.
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_ARCHIVE_H_
