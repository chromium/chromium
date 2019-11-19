// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/volume_archive_minizip.h"

#include <time.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <limits>
#include <utility>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/minizip/src/mz.h"
#include "third_party/minizip/src/mz_strm.h"
#include "third_party/minizip/src/mz_zip.h"

namespace {

const char kArchiveOpenError[] = "Failed to open archive.";
const char kArchiveNextHeaderError[] =
    "Failed to open current file in archive.";
const char kArchiveReadDataError[] = "Failed to read archive data.";

// The size of the buffer used to skip unnecessary data. Should be positive and
// UINT16_MAX or less. unzReadCurrentFile in third_party/minizip/src/unzip.c
// supports to read a data up to UINT16_MAX at a time.
const int64_t kDummyBufferSize = UINT16_MAX;  // ~64 KB

// The size of the buffer used by ReadInProgress to decompress data. Should be
// positive and UINT16_MAX or less. unzReadCurrentFile in
// third_party/minizip/src/unzip.c supports to read a data up to UINT16_MAX at a
// time.
const int64_t kDecompressBufferSize = UINT16_MAX;  // ~64 KB.

// The maximum data chunk size for VolumeReader::Read requests.
// Should be positive.
const int64_t kMaximumDataChunkSize = 512 * 1024;  // 512 KB.

// The minimum data chunk size for VolumeReader::Read requests.
// Should be positive.
const int64_t kMinimumDataChunkSize = 32 * 1024;  // 32 KB.

// The size of the static cache. We need at least 64KB to cache whole
// 'end of central directory' data.
const int64_t kStaticCacheSize = 128 * 1024;

int32_t MinizipIsOpen(void* stream) {
  // The stream is always in an open state since it's tied to the life of the
  // VolumeArchiveMinizip instance.
  return MZ_OK;
}

}  // namespace

// vtable for the archive read stream provided to minizip. Only functions which
// are necessary to read the archive are provided.
mz_stream_vtbl VolumeArchiveMinizip::minizip_vtable = {
    nullptr,                             /* open */
    MinizipIsOpen, MinizipRead, nullptr, /* write */
    MinizipTell,   MinizipSeek, nullptr, /* close */
    nullptr,                             /* error */
    nullptr,                             /* create */
    nullptr,                             /* destroy */
    nullptr,                             /* get_prop_int64 */
    nullptr                              /* set_prop_int64 */
};

// Stream object used by minizip to access archive files.
struct VolumeArchiveMinizip::MinizipStream {
  // Must be the first element because minizip internally will cast this
  // struct into a mz_stream.
  mz_stream stream_ = {
      &minizip_vtable, nullptr,
  };

  VolumeArchiveMinizip* archive_;

  explicit MinizipStream(VolumeArchiveMinizip* archive) : archive_(archive) {
    DCHECK(archive);
  }
};

int32_t VolumeArchiveMinizip::MinizipRead(void* stream,
                                          void* buffer,
                                          int32_t size) {
  return static_cast<MinizipStream*>(stream)->archive_->StreamRead(buffer,
                                                                   size);
}

int32_t VolumeArchiveMinizip::StreamRead(void* buffer, int32_t size) {
  int64_t offset = reader()->offset();
  DCHECK(offset >= 0 && offset < reader()->archive_size());

  // Don't try to read more data than is available.
  int32_t read_size = static_cast<int32_t>(
      std::min(static_cast<int64_t>(size), reader()->archive_size() - offset));

  // When minizip requests a chunk in static_cache_.
  if (offset >= static_cache_offset_) {
    // Relative offset in the central directory.
    int64_t offset_in_cache = offset - static_cache_offset_;
    memcpy(buffer, static_cache_.get() + offset_in_cache, read_size);
    if (reader()->Seek(static_cast<int64_t>(read_size),
                       base::File::FROM_CURRENT) < 0) {
      return MZ_STREAM_ERROR;
    }
    return read_size;
  }

  char* unzip_buffer_pointer = static_cast<char*>(buffer);
  int64_t left_length = static_cast<int64_t>(read_size);

  do {
    offset = reader()->offset();
    // If dynamic_cache_ is empty or it cannot be reused, update the cache so
    // that it contains the chunk required by minizip.
    if (dynamic_cache_size_ == 0 || offset < dynamic_cache_offset_ ||
        dynamic_cache_offset_ + dynamic_cache_size_ < offset + read_size) {
      if (DynamicCache(read_size) < 0)
        return MZ_STREAM_ERROR;
    }

    // Just copy the required data from the cache.
    int64_t offset_in_cache = offset - dynamic_cache_offset_;
    int64_t copy_length =
        std::min(left_length, dynamic_cache_size_ - offset_in_cache);
    memcpy(unzip_buffer_pointer, dynamic_cache_.get() + offset_in_cache,
           copy_length);
    unzip_buffer_pointer += copy_length;
    left_length -= copy_length;
    if (reader()->Seek(static_cast<int64_t>(copy_length),
                       base::File::FROM_CURRENT) < 0) {
      return MZ_STREAM_ERROR;
    }
  } while (left_length > 0);

  return read_size;
}

int64_t VolumeArchiveMinizip::DynamicCache(int64_t unzip_size) {
  int64_t offset = reader()->offset();
  if (reader()->Seek(static_cast<int64_t>(offset), base::File::FROM_BEGIN) <
      0) {
    return -1 /* Error */;
  }

  int64_t bytes_to_read =
      std::min(kMaximumDataChunkSize, reader()->archive_size() - offset);
  DCHECK_GT(bytes_to_read, 0);
  int64_t left_length = bytes_to_read;
  char* buffer_pointer = dynamic_cache_.get();
  const void* destination_buffer;

  do {
    int64_t read_bytes = reader()->Read(left_length, &destination_buffer);
    // End of the zip file.
    if (read_bytes == 0)
      break;
    if (read_bytes < 0)
      return -1 /* Error */;
    memcpy(buffer_pointer, destination_buffer, read_bytes);
    left_length -= read_bytes;
    buffer_pointer += read_bytes;
  } while (left_length > 0);

  if (reader()->Seek(static_cast<int64_t>(offset), base::File::FROM_BEGIN) <
      0) {
    return -1 /* Error */;
  }
  dynamic_cache_size_ = bytes_to_read - left_length;
  dynamic_cache_offset_ = offset;

  return unzip_size - left_length;
}

int64_t VolumeArchiveMinizip::MinizipTell(void* stream) {
  return static_cast<MinizipStream*>(stream)->archive_->StreamTell();
}

int64_t VolumeArchiveMinizip::StreamTell() {
  return reader()->offset();
}

int32_t VolumeArchiveMinizip::MinizipSeek(void* stream,
                                          int64_t offset,
                                          int32_t origin) {
  return static_cast<MinizipStream*>(stream)->archive_->StreamSeek(offset,
                                                                   origin);
}

int32_t VolumeArchiveMinizip::StreamSeek(int64_t offset, int32_t origin) {
  base::File::Whence whence;
  switch (origin) {
    case MZ_SEEK_SET:
      whence = base::File::FROM_BEGIN;
      break;
    case MZ_SEEK_CUR:
      whence = base::File::FROM_CURRENT;
      break;
    case MZ_SEEK_END:
      whence = base::File::FROM_END;
      break;
    default:
      NOTREACHED();
      return MZ_STREAM_ERROR;
  }

  int64_t new_offset = reader()->Seek(offset, whence);
  if (new_offset < 0)
    return MZ_STREAM_ERROR;

  return MZ_OK;
}

VolumeArchiveMinizip::VolumeArchiveMinizip(std::unique_ptr<VolumeReader> reader)
    : VolumeArchive(std::move(reader)),
      reader_data_size_(kMinimumDataChunkSize),
      stream_(std::make_unique<MinizipStream>(this)),
      dynamic_cache_(std::make_unique<char[]>(kMaximumDataChunkSize)),
      dynamic_cache_offset_(0),
      dynamic_cache_size_(0),
      static_cache_(std::make_unique<char[]>(kStaticCacheSize)),
      static_cache_offset_(0),
      static_cache_size_(0),
      last_read_data_offset_(0),
      last_read_data_length_(0),
      dummy_buffer_(std::make_unique<char[]>(kDummyBufferSize)),
      decompressed_data_(nullptr),
      decompressed_data_buffer_(
          std::make_unique<char[]>(kDecompressBufferSize)),
      decompressed_data_size_(0),
      decompressed_error_(false) {
  static_assert(offsetof(VolumeArchiveMinizip::MinizipStream, stream_) == 0,
                "Bad mz_stream offset");
}

VolumeArchiveMinizip::~VolumeArchiveMinizip() = default;

bool VolumeArchiveMinizip::Init(const std::string& encoding) {
  // Load maximum static_cache_size_ bytes from the end of the archive to
  // static_cache_.
  static_cache_size_ = std::min(kStaticCacheSize, reader()->archive_size());
  int64_t previous_offset = reader()->offset();
  char* buffer_pointer = static_cache_.get();
  int64_t left_length = static_cache_size_;
  static_cache_offset_ = std::max(reader()->archive_size() - static_cache_size_,
                                  static_cast<int64_t>(0));
  if (reader()->Seek(static_cache_offset_, base::File::FROM_BEGIN) < 0) {
    set_error_message(kArchiveOpenError);
    return false /* Error */;
  }
  do {
    const void* destination_buffer;
    int64_t read_bytes = reader()->Read(left_length, &destination_buffer);
    memcpy(buffer_pointer, destination_buffer, read_bytes);
    left_length -= read_bytes;
    buffer_pointer += read_bytes;
  } while (left_length > 0);

  // Set the offset to the original position.
  if (reader()->Seek(previous_offset, base::File::FROM_BEGIN) < 0) {
    set_error_message(kArchiveOpenError);
    return false /* Error */;
  }

  zip_file_.reset(mz_zip_create(nullptr));
  int32_t result =
      mz_zip_open(zip_file_.get(), stream_.get(), MZ_OPEN_MODE_READ);
  if (result != MZ_OK) {
    set_error_message(kArchiveOpenError);
    return false;
  }
  result = mz_zip_goto_first_entry(zip_file_.get());
  if (result != MZ_OK) {
    set_error_message(kArchiveOpenError);
    return false;
  }

  return true;
}

VolumeArchive::Result VolumeArchiveMinizip::GetCurrentFileInfo(
    std::string* pathname,
    bool* is_encoded_in_utf8,
    int64_t* size,
    bool* is_directory,
    time_t* modification_time) {
  // Headers are being read from the central directory (in the ZIP format), so
  // use a large block size to save on IPC calls. The headers in EOCD are
  // grouped one by one.
  reader_data_size_ = kMaximumDataChunkSize;

  // Reset to 0 for new VolumeArchive::ReadData operation.
  last_read_data_offset_ = 0;
  decompressed_data_size_ = 0;

  if (mz_zip_get_entry(zip_file_.get()) < 0) {
    set_error_message(kArchiveNextHeaderError);
    return VolumeArchive::RESULT_FAIL;
  }

  // Get the information of the opened file.
  mz_zip_file* file_info = nullptr;
  if (mz_zip_entry_get_info(zip_file_.get(), &file_info) != MZ_OK) {
    set_error_message(kArchiveNextHeaderError);
    return VolumeArchive::RESULT_FAIL;
  }

  if (file_info->filename_size == 0 || file_info->filename[0] == '\0') {
    LOG(ERROR) << "null file name error";
    set_error_message(kArchiveNextHeaderError);
    return VolumeArchive::RESULT_FAIL;
  }

  *pathname = std::string(file_info->filename);
  *is_encoded_in_utf8 = (file_info->flag & MZ_ZIP_FLAG_UTF8) != 0;
  *size = file_info->uncompressed_size;
  *is_directory = (mz_zip_entry_is_dir(zip_file_.get()) == MZ_OK);
  *modification_time = file_info->modified_date;

  file_offset_map_[*pathname] = mz_zip_get_entry(zip_file_.get());

  return VolumeArchive::RESULT_SUCCESS;
}

VolumeArchive::Result VolumeArchiveMinizip::GoToNextFile() {
  if (!CloseZipEntry()) {
    LOG(ERROR) << "Error closing current zip entry";
    return VolumeArchive::RESULT_FAIL;
  }

  int32_t return_value = mz_zip_goto_next_entry(zip_file_.get());
  if (return_value == MZ_END_OF_LIST) {
    return VolumeArchive::RESULT_EOF;
  }
  if (return_value == MZ_OK)
    return VolumeArchive::RESULT_SUCCESS;

  set_error_message(kArchiveNextHeaderError);
  return VolumeArchive::RESULT_FAIL;
}

bool VolumeArchiveMinizip::SeekHeader(const std::string& path_name) {
  if (!CloseZipEntry()) {
    LOG(ERROR) << "Error closing current zip entry";
    return false;
  }

  // Reset to 0 for new VolumeArchive::ReadData operation.
  last_read_data_offset_ = 0;
  decompressed_data_size_ = 0;

  auto it = file_offset_map_.find(path_name);
  if (it != file_offset_map_.end()) {
    if (mz_zip_goto_entry(zip_file_.get(), it->second) != MZ_OK) {
      set_error_message(kArchiveNextHeaderError);
      return false;
    }
  } else {
    // Setting nullptr to filename_compare_func falls back to strcmp, i.e. case
    // sensitive.
    if (mz_zip_locate_entry(zip_file_.get(), path_name.c_str(), 0) != MZ_OK) {
      set_error_message(kArchiveNextHeaderError);
      return false;
    }
  }

  mz_zip_file* file_info = nullptr;
  if (mz_zip_entry_get_info(zip_file_.get(), &file_info) != MZ_OK) {
    set_error_message(kArchiveNextHeaderError);
    return false;
  }

  if (mz_zip_entry_is_dir(zip_file_.get()) == MZ_OK) {
    return true;
  }

  bool is_encrypted = ((file_info->flag & MZ_ZIP_FLAG_ENCRYPTED) != 0);
  int32_t open_result = MZ_OK;
  do {
    if (is_encrypted && !password_cache_) {
      // Save passphrase for upcoming file requests.
      password_cache_ = reader()->Passphrase();
      // check if |password_cache_| is nullptr in case when user clicks Cancel
      if (!password_cache_) {
        return false;
      }
    }

    open_result = mz_zip_entry_read_open(
        zip_file_.get(), 0,
        is_encrypted ? password_cache_.value().c_str() : nullptr);

    // If password is incorrect then password cache ought to be reseted.
    if (open_result == MZ_PASSWORD_ERROR)
      password_cache_.reset();
  } while (is_encrypted && open_result == MZ_PASSWORD_ERROR);

  if (open_result != MZ_OK) {
    set_error_message(kArchiveNextHeaderError);
    return false;
  }

  return true;
}

void VolumeArchiveMinizip::DecompressData(int64_t offset, int64_t length) {
  // TODO(cmihail): As an optimization consider using archive_read_data_block
  // which avoids extra copying in case offset != last_read_data_offset_.
  // The logic will be more complicated because archive_read_data_block offset
  // will not be aligned with the offset of the read request from JavaScript.

  // Requests with offset smaller than last read offset are not supported.
  if (offset < last_read_data_offset_) {
    set_error_message(std::string(kArchiveReadDataError));
    decompressed_error_ = true;
    return;
  }

  // Request with offset greater than last read offset. Skip not needed bytes.
  // Because files are compressed, seeking is not possible, so all of the bytes
  // until the requested position must be unpacked.
  ssize_t size = -1;
  while (offset > last_read_data_offset_) {
    // ReadData will call CustomArchiveRead when calling archive_read_data. Read
    // should not request more bytes than possibly needed, so we request either
    // offset - last_read_data_offset_, kMaximumDataChunkSize in case the former
    // is too big or kMinimumDataChunkSize in case its too small and we might
    // end up with too many IPCs.
    reader_data_size_ = std::max(
        std::min(offset - last_read_data_offset_, kMaximumDataChunkSize),
        kMinimumDataChunkSize);

    // No need for an offset in dummy_buffer as it will be ignored anyway.
    // archive_read_data receives size_t as length parameter, but we limit it to
    // kDummyBufferSize which is positive and less
    // than size_t maximum. So conversion from int64_t to size_t is safe here.
    size = mz_zip_entry_read(
        zip_file_.get(), dummy_buffer_.get(),
        std::min(offset - last_read_data_offset_, kDummyBufferSize));
    DCHECK_NE(size, 0);    // The actual read is done below. We shouldn't get to
                           // end of file here.
    if (size < 0) {        // Error.
      set_error_message(kArchiveReadDataError);
      decompressed_error_ = true;
      return;
    }
    last_read_data_offset_ += size;
  }

  // Do not decompress more bytes than we can store internally. The
  // kDecompressBufferSize limit is used to avoid huge memory usage.
  int64_t left_length = std::min(length, kDecompressBufferSize);

  // ReadData will call CustomArchiveRead when calling archive_read_data. The
  // read should be done with a value similar to length, which is the requested
  // number of bytes, or kMaximumDataChunkSize / kMinimumDataChunkSize
  // in case length is too big or too small.
  reader_data_size_ = std::max(
      std::min(static_cast<int64_t>(left_length), kMaximumDataChunkSize),
      kMinimumDataChunkSize);

  // Perform the actual copy.
  int64_t bytes_read = 0;
  do {
    // archive_read_data receives size_t as length parameter, but we limit it to
    // kMinimumDataChunkSize (see left_length
    // initialization), which is positive and less than size_t maximum.
    // So conversion from int64_t to size_t is safe here.
    size = mz_zip_entry_read(zip_file_.get(),
                             decompressed_data_buffer_.get() + bytes_read,
                             left_length);
    if (size < 0) {  // Error.
      set_error_message(kArchiveReadDataError);
      decompressed_error_ = true;
      return;
    }
    bytes_read += size;
    left_length -= size;
  } while (left_length > 0 && size != 0);  // There is still data to read.

  // VolumeArchiveMinizip::DecompressData always stores the data from
  // beginning of the buffer. VolumeArchiveMinizip::ConsumeData is used
  // to preserve the bytes that are decompressed but not required by
  // VolumeArchiveMinizip::ReadData.
  decompressed_data_ = decompressed_data_buffer_.get();
  decompressed_data_size_ = bytes_read;
}

int64_t VolumeArchiveMinizip::ReadData(int64_t offset,
                                       int64_t length,
                                       const char** buffer) {
  DCHECK_GT(length, 0);  // Length must be at least 1.
  // In case of first read or no more available data in the internal buffer or
  // offset is different from the last_read_data_offset_, then force
  // VolumeArchiveMinizip::DecompressData as the decompressed data is
  // invalid.
  if (!decompressed_data_ || last_read_data_offset_ != offset ||
      decompressed_data_size_ == 0)
    DecompressData(offset, length);

  // Decompressed failed.
  if (decompressed_error_) {
    set_error_message(kArchiveReadDataError);
    return -1 /* Error */;
  }

  last_read_data_length_ = length;  // Used for decompress ahead.

  // Assign the output *buffer parameter to the internal buffer.
  *buffer = decompressed_data_;

  // Advance internal buffer for next ReadData call.
  int64_t read_bytes = std::min(decompressed_data_size_, length);
  decompressed_data_ = decompressed_data_ + read_bytes;
  decompressed_data_size_ -= read_bytes;
  last_read_data_offset_ += read_bytes;

  DCHECK(decompressed_data_ + decompressed_data_size_ <=
         decompressed_data_buffer_.get() + kDecompressBufferSize);

  return read_bytes;
}

void VolumeArchiveMinizip::MaybeDecompressAhead() {
  if (decompressed_data_size_ == 0)
    DecompressData(last_read_data_offset_, last_read_data_length_);
}

bool VolumeArchiveMinizip::CloseZipEntry() {
  if (mz_zip_entry_is_open(zip_file_.get()) != MZ_OK)
    return true;

  const int32_t error = mz_zip_entry_close(zip_file_.get());
  // If the zip entry was not read in full, then closing the entry may cause a
  // CRC error, because the whole file may not have been decompressed and
  // checksummed.
  const bool ok = (error == MZ_OK || error == MZ_CRC_ERROR);
  if (!ok) {
    set_error_message(base::StringPrintf("mz_zip_entry_close err = %d", error));
  }
  return ok;
}
