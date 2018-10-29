// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_

#include <memory>
#include <string>

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_archive.h"
#include "third_party/minizip/src/unzip.h"
#include "third_party/minizip/src/zip.h"

class CompressorStream;

class CompressorArchiveMinizip : public CompressorArchive {
 public:
  explicit CompressorArchiveMinizip(CompressorStream* compressor_stream);

  ~CompressorArchiveMinizip() override;

  // Creates an archive object.
  bool CreateArchive() override;

  // Closes the archive.
  bool CloseArchive(bool has_error) override;

  // Cancels the compression process.
  void CancelArchive() override;

  // Adds an entry to the archive.
  bool AddToArchive(const std::string& filename,
                    int64_t file_size,
                    base::Time modification_time,
                    bool is_directory) override;

 private:
  // Stream functions used by minizip. In all cases, |compressor| points to
  // |this|.
  static uint32_t MinizipWrite(void* compressor,
                               void* stream,
                               const void* buffer,
                               uint32_t length);
  static long MinizipTell(void* compressor, void* stream);
  static long MinizipSeek(void* compressor,
                          void* stream,
                          uint32_t offset,
                          int origin);

  // Implementation of stream functions used by minizip.
  uint32_t StreamWrite(const void* buffer, uint32_t length);
  long StreamTell();
  long StreamSeek(uint32_t offset, int origin);

  // An instance that takes care of all IO operations.
  CompressorStream* compressor_stream_;

  // The minizip correspondent archive object.
  zipFile zip_file_;

  // The buffer used to store the data read from JavaScript.
  std::unique_ptr<char[]> destination_buffer_;

  // The current offset of the zip archive file.
  int64_t offset_;
  // The size of the zip archive file.
  int64_t length_;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_COMPRESSOR_ARCHIVE_MINIZIP_H_
