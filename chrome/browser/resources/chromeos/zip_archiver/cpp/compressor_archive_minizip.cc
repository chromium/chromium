// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_archive_minizip.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>

#include "base/time/time.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_io_javascript_stream.h"
#include "chrome/browser/resources/chromeos/zip_archiver/cpp/compressor_stream.h"
#include "ppapi/cpp/logging.h"

namespace {

const char kCreateArchiveError[] = "Failed to create archive.";
const char kAddToArchiveError[] = "Failed to add entry to archive.";
const char kCloseArchiveError[] = "Failed to close archive.";

// We need at least 256KB for MiniZip.
const int64_t kMaximumDataChunkSize = 512 * 1024;

uint32_t UnixToDosdate(const base::Time datetime) {
  base::Time::Exploded exploded;
  datetime.LocalExplode(&exploded);

  return (exploded.year - 1980) << 25 | exploded.month << 21 |
         exploded.day_of_month << 16 | exploded.hour << 11 |
         exploded.minute << 5 | exploded.second >> 1;
}

void* MinizipOpen(void* compressor, const char* /*filename*/, int /*mode*/) {
  return compressor;
}

uint32_t MinizipRead(void* /*compressor*/,
                     void* /*stream*/,
                     void* /*buffur*/,
                     uint32_t /*size*/) {
  NOTREACHED();
  return 0;
}

int MinizipClose(void* /*compressor*/, void* /*stream*/) {
  return 0;
}

int MinizipError(void* /*compressor*/, void* /*stream*/) {
  return 0;
}

};  // namespace

// Called when data chunk must be written on the archive. It copies data
// from the given buffer processed by minizip to an array buffer and passes
// it to compressor_stream.
uint32_t CompressorArchiveMinizip::MinizipWrite(void* compressor,
                                                void* /*stream*/,
                                                const void* zip_buffer,
                                                uint32_t zip_length) {
  return static_cast<CompressorArchiveMinizip*>(compressor)
      ->StreamWrite(zip_buffer, zip_length);
}

uint32_t CompressorArchiveMinizip::StreamWrite(const void* zip_buffer,
                                               uint32_t zip_length) {
  int64_t written_bytes = compressor_stream()->Write(
      offset_, zip_length, static_cast<const char*>(zip_buffer));

  if (written_bytes != zip_length)
    return 0 /* Error */;

  // Update offset_ and length_.
  offset_ += written_bytes;
  if (offset_ > length_)
    length_ = offset_;
  return static_cast<uint32_t>(written_bytes);
}

// Returns the offset from the beginning of the data.
long CompressorArchiveMinizip::MinizipTell(void* compressor, void* /*stream*/) {
  return static_cast<CompressorArchiveMinizip*>(compressor)->StreamTell();
}

long CompressorArchiveMinizip::StreamTell() {
  return static_cast<long>(offset_);
}

// Moves the current offset to the specified position.
long CompressorArchiveMinizip::MinizipSeek(void* compressor,
                                           void* /*stream*/,
                                           uint32_t offset,
                                           int origin) {
  return static_cast<CompressorArchiveMinizip*>(compressor)
      ->StreamSeek(offset, origin);
}

long CompressorArchiveMinizip::StreamSeek(uint32_t offset, int origin) {
  if (origin == ZLIB_FILEFUNC_SEEK_CUR) {
    offset_ = std::min(offset_ + static_cast<int64_t>(offset), length_);
    return 0 /* Success */;
  }
  if (origin == ZLIB_FILEFUNC_SEEK_END) {
    offset_ = std::max(length_ - static_cast<int64_t>(offset),
                       static_cast<int64_t>(0));
    return 0 /* Success */;
  }
  if (origin == ZLIB_FILEFUNC_SEEK_SET) {
    offset_ = std::min(static_cast<int64_t>(offset), length_);
    return 0 /* Success */;
  }
  return -1 /* Error */;
}

CompressorArchiveMinizip::CompressorArchiveMinizip(
    CompressorStream* compressor_stream)
    : CompressorArchive(compressor_stream),
      compressor_stream_(compressor_stream),
      zip_file_(nullptr),
      destination_buffer_(std::make_unique<char[]>(kMaximumDataChunkSize)),
      offset_(0),
      length_(0) {}

CompressorArchiveMinizip::~CompressorArchiveMinizip() = default;

bool CompressorArchiveMinizip::CreateArchive() {
  // Set up archive object.
  zlib_filefunc_def zip_funcs;
  zip_funcs.zopen_file = MinizipOpen;
  zip_funcs.zread_file = MinizipRead;
  zip_funcs.zwrite_file = MinizipWrite;
  zip_funcs.ztell_file = MinizipTell;
  zip_funcs.zseek_file = MinizipSeek;
  zip_funcs.zclose_file = MinizipClose;
  zip_funcs.zerror_file = MinizipError;
  zip_funcs.opaque = this;

  zip_file_ = zipOpen2(nullptr /* pathname */, APPEND_STATUS_CREATE,
                       nullptr /* globalcomment */, &zip_funcs);
  if (!zip_file_) {
    set_error_message(kCreateArchiveError);
    return false /* Error */;
  }
  return true /* Success */;
}

bool CompressorArchiveMinizip::AddToArchive(const std::string& filename,
                                            int64_t file_size,
                                            base::Time modification_time,
                                            bool is_directory) {
  // Minizip takes filenames that end with '/' as directories.
  std::string normalized_filename = filename;
  if (is_directory)
    normalized_filename += "/";

  // Fill zipfileMetadata with modification_time.
  zip_fileinfo zipfileMetadata;
  zipfileMetadata.dos_date = UnixToDosdate(modification_time);

  // Section 4.4.4 http://www.pkware.com/documents/casestudies/APPNOTE.TXT
  // Setting the Language encoding flag so the file is told to be in utf-8.
  const uLong LANGUAGE_ENCODING_FLAG = 0x1 << 11;

  // Indicates the compatibility of the file attribute information.
  // Attributes of files are not avaiable in the FileSystem API. Therefore
  // we don't store file attributes to an archive. However, other apps may use
  // this field to determine the line record format for text files etc.
  const int HOST_SYSTEM_CODE = 3;  // UNIX

  // PKWARE .ZIP File Format Specification version 6.3.x
  const int ZIP_SPECIFICATION_VERSION_CODE = 63;

  const int VERSION_MADE_BY =
      HOST_SYSTEM_CODE << 8 | ZIP_SPECIFICATION_VERSION_CODE;

  int open_result =
      zipOpenNewFileInZip4(zip_file_,                    // file
                           normalized_filename.c_str(),  // filename
                           &zipfileMetadata,             // zipfi
                           nullptr,                      // extrafield_local
                           0u,                       // size_extrafield_local
                           nullptr,                  // extrafield_global
                           0u,                       // size_extrafield_global
                           nullptr,                  // comment
                           Z_DEFLATED,               // method
                           Z_DEFAULT_COMPRESSION,    // level
                           0,                        // raw
                           -MAX_WBITS,               // windowBits
                           DEF_MEM_LEVEL,            // memLevel
                           Z_DEFAULT_STRATEGY,       // strategy
                           nullptr,                  // password
                           0,                        // crcForCrypting
                           VERSION_MADE_BY,          // versionMadeBy
                           LANGUAGE_ENCODING_FLAG);  // flagBase
  if (open_result != ZIP_OK) {
    CloseArchive(true /* has_error */);
    set_error_message(kAddToArchiveError);
    return false /* Error */;
  }

  bool has_error = false;
  if (!is_directory) {
    int64_t remaining_size = file_size;
    while (remaining_size > 0) {
      int64_t chunk_size = std::min(remaining_size, kMaximumDataChunkSize);
      PP_DCHECK(chunk_size > 0);

      int64_t read_bytes =
          compressor_stream_->Read(chunk_size, destination_buffer_.get());
      // Negative read_bytes indicates an error occurred when reading chunks.
      // 0 just means there is no more data available, but here we need positive
      // length of bytes, so this is also an error here.
      if (read_bytes <= 0) {
        has_error = true;
        break;
      }

      if (canceled_) {
        break;
      }

      if (zipWriteInFileInZip(zip_file_, destination_buffer_.get(),
                              read_bytes) != ZIP_OK) {
        has_error = true;
        break;
      }
      remaining_size -= read_bytes;
    }
  }

  if (!has_error && zipCloseFileInZip(zip_file_) != ZIP_OK)
    has_error = true;

  if (has_error) {
    CloseArchive(true /* has_error */);
    set_error_message(kAddToArchiveError);
    return false /* Error */;
  }

  if (canceled_) {
    CloseArchive(true /* has_error */);
    return false /* Error */;
  }

  return true /* Success */;
}

bool CompressorArchiveMinizip::CloseArchive(bool has_error) {
  if (zipClose(zip_file_, nullptr /* global_comment */) != ZIP_OK) {
    set_error_message(kCloseArchiveError);
    return false /* Error */;
  }
  if (!has_error) {
    if (compressor_stream()->Flush() < 0) {
      set_error_message(kCloseArchiveError);
      return false /* Error */;
    }
  }
  return true /* Success */;
}

void CompressorArchiveMinizip::CancelArchive() {
  canceled_ = true;
}
