// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_ARCHIVE_MINIZIP_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_ARCHIVE_MINIZIP_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/optional.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/minizip_helpers.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_archive.h"
#include "third_party/minizip/src/mz_strm.h"

// Defines an implementation of VolumeArchive that wraps all minizip
// operations.
class VolumeArchiveMinizip : public VolumeArchive {
 public:
  explicit VolumeArchiveMinizip(std::unique_ptr<VolumeReader> reader);

  ~VolumeArchiveMinizip() override;

  // See volume_archive_interface.h.
  bool Init(const std::string& encoding) override;

  // See volume_archive_interface.h.
  VolumeArchive::Result GetCurrentFileInfo(std::string* path_name,
                                           bool* isEncodedInUtf8,
                                           int64_t* size,
                                           bool* is_directory,
                                           time_t* modification_time) override;

  VolumeArchive::Result GoToNextFile() override;

  // See volume_archive_interface.h.
  bool SeekHeader(const std::string& path_name) override;

  // See volume_archive_interface.h.
  int64_t ReadData(int64_t offset,
                   int64_t length,
                   const char** buffer) override;

  // See volume_archive_interface.h.
  void MaybeDecompressAhead() override;

 private:
  static mz_stream_vtbl minizip_vtable;
  struct MinizipStream;

  // Stream functions used by minizip. In all cases, |stream| points to
  // |this->stream_|.
  static int32_t MinizipRead(void* stream, void* buf, int32_t size);
  static int64_t MinizipTell(void* stream);
  static int32_t MinizipSeek(void* stream, int64_t offset, int32_t origin);

  // Implementation of stream functions used by minizip.
  int32_t StreamRead(void* buf, int32_t size);
  int64_t StreamTell();
  int32_t StreamSeek(int64_t offset, int32_t origin);

  // Read cache.
  int64_t DynamicCache(int64_t unz_size);

  // Decompress length bytes of data starting from offset.
  void DecompressData(int64_t offset, int64_t length);

  // Closes the current zip file entry.
  bool CloseZipEntry();

  // The size of the requested data from VolumeReader.
  int64_t reader_data_size_;

  // The minizip stream used to read the archive file.
  std::unique_ptr<MinizipStream> stream_;

  // We use two kinds of cache strategies here: dynamic and static.
  // Dynamic cache is a common cache strategy used in most of IO streams such as
  // fread. When a file chunk is requested and if the size of the requested
  // chunk is small, we load larger size of bytes from the archive and cache
  // them in dynamic_cache_. If the range of the next requested chunk is within
  // the cache, we don't read the archive and just return the data in the cache.
  std::unique_ptr<char[]> dynamic_cache_;

  // The offset from which dynamic_cache_ has the data of the archive.
  int64_t dynamic_cache_offset_;

  // The size of the data in dynamic_cache_.
  int64_t dynamic_cache_size_;

  // Although dynamic cache works in most situations, it doesn't work when
  // MiniZip is looking for the front index of the central directory. Since
  // MiniZip reads the data little by little backwards from the end to find the
  // index, dynamic_cache will be reloaded every time. To avoid this, we first
  // cache a certain length of data from the end into static_cache_. The data
  // in this buffer is also used when the data in the central directory is
  // requested by MiniZip later.
  std::unique_ptr<char[]> static_cache_;

  // The offset from which static_cache_ has the data of the archive.
  int64_t static_cache_offset_;

  // The size of the data in static_cache_. The End Of Central Directory header
  // is guaranteed to be in the last 64(global comment) + 1(other fields) of the
  // file. This cache is used to store the header.
  int64_t static_cache_size_;

  // The data offset, which will be offset + length after last read
  // operation, where offset and length are method parameters for
  // VolumeArchiveMinizip::ReadData. Data offset is used to improve
  // performance for consecutive calls to VolumeArchiveMinizip::ReadData.
  //
  // Intead of starting the read from the beginning for every
  // VolumeArchiveMinizip::ReadData, the next call will start
  // from last_read_data_offset_ in case the offset parameter of
  // VolumeArchiveMinizip::ReadData has the same value as
  // last_read_data_offset_. This avoids decompressing again the bytes at
  // the begninning of the file, which is the average case scenario.
  // But in case the offset parameter is different than last_read_data_offset_,
  // then dummy_buffer_ will be used to ignore unused bytes.
  int64_t last_read_data_offset_;

  // The length of the last VolumeArchiveMinizip::ReadData. Used for
  // decompress ahead.
  int64_t last_read_data_length_;

  // Dummy buffer for unused data read using VolumeArchiveMinizip::ReadData.
  // Sometimes VolumeArchiveMinizip::ReadData can require reading from
  // offsets different from last_read_data_offset_. In this case some bytes
  // must be skipped. Because seeking is not possible inside compressed files,
  // the bytes will be discarded using this buffer.
  std::unique_ptr<char[]> dummy_buffer_;

  // The address where the decompressed data starting from
  // decompressed_offset_ is stored. It should point to a valid location
  // inside decompressed_data_buffer_. Necesssary in order to NOT throw
  // away unused decompressed bytes as throwing them away would mean in some
  // situations restarting decompressing the file from the beginning.
  char* decompressed_data_;

  // The actual buffer that contains the decompressed data.
  std::unique_ptr<char[]> decompressed_data_buffer_;

  // The size of valid data starting from decompressed_data_ that is stored
  // inside decompressed_data_buffer_.
  int64_t decompressed_data_size_;

  // True if VolumeArchiveMinizip::DecompressData failed.
  bool decompressed_error_;

  // The password cache to access password protected files.
  base::Optional<std::string> password_cache_;

  // Map of file name to zip file offset.
  std::unordered_map<std::string, int64_t> file_offset_map_;

  // The minizip correspondent archive object.
  // This must be destroyed before the archive stream and any of its buffers
  // because AES encryption in minizip will try to read the file on close.
  ScopedMzZip zip_file_;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_CPP_VOLUME_ARCHIVE_MINIZIP_H_
