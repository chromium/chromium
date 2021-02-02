// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/single_file_tar_reader.h"

#include <string>

#include "base/check.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"

namespace extensions {
namespace image_writer {

namespace {
constexpr int kTarBufferSize = 8192;
}  // namespace

SingleFileTarReader::SingleFileTarReader(Delegate* delegate)
    : delegate_(delegate), buffer_(kTarBufferSize) {}

SingleFileTarReader::~SingleFileTarReader() = default;

bool SingleFileTarReader::ExtractChunk() {
  int bytes_read =
      delegate_->ReadTarFile(buffer_.data(), buffer_.size(), &error_id_);
  if (bytes_read < 0) {
    return false;
  }

  int offset = 0;

  // We haven't read the header.
  if (total_bytes_ == 0) {
    if (bytes_read < 512) {
      error_id_ = error::kUnzipInvalidArchive;
      return false;
    }

    // TODO(tetsui): check the file header checksum

    // Read the actual file size.
    total_bytes_ = ReadOctalNumber(buffer_.data() + 124, 12);

    // Skip the rest of the header.
    offset += 512;
  }

  // A tar file always has a padding at the end of the file. As they should not
  // be included in the output, we should take the minimum of the actual
  // remaining bytes versus the bytes read.
  uint64_t bytes_written =
      std::min<uint64_t>(total_bytes_ - curr_bytes_, bytes_read - offset);
  if (!delegate_->WriteContents(buffer_.data() + offset, bytes_written,
                                &error_id_)) {
    return false;
  }
  curr_bytes_ += bytes_written;

  // TODO(tetsui): check it's the end of the file

  return true;
}

bool SingleFileTarReader::IsComplete() const {
  return total_bytes_ > 0 && total_bytes_ == curr_bytes_;
}

// static
uint64_t SingleFileTarReader::ReadOctalNumber(const char* buffer,
                                              size_t length) {
  DCHECK(length > 8);

  uint64_t num = 0;

  // In GNU tar extension, when the number starts with an invalid ASCII
  // character 0x80, then non-leading 8 bytes of the field should be interpreted
  // as a big-endian integer.
  // https://www.gnu.org/software/tar/manual/html_node/Extensions.html
  if (static_cast<unsigned char>(buffer[0]) == 0x80) {
    for (size_t i = length - 8; i < length; ++i) {
      num <<= 8;
      num += static_cast<unsigned char>(buffer[i]);
    }
    return num;
  }

  for (size_t i = 0; i < length; ++i) {
    if (buffer[i] == '\0')
      break;
    num *= 8;
    num += buffer[i] - '0';
  }
  return num;
}

}  // namespace image_writer
}  // namespace extensions
