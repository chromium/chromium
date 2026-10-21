// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/zstd_extractor.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "third_party/zstd/src/lib/zstd.h"

namespace extensions::image_writer {

namespace {

constexpr base::FilePath::CharType kExtractedBinFileName[] =
    FILE_PATH_LITERAL("extracted.bin");

// Zstandard frame magic number (0xFD2FB528 in little-endian byte order, per
// RFC 8878 Section 3.1.1).
constexpr std::array<uint8_t, 4> kExpectedMagic = {0x28, 0xb5, 0x2f, 0xfd};

// Custom deleter for libzstd's decompression context (`ZSTD_DCtx`).
struct ZstdDecompressionContextDeleter {
  void operator()(ZSTD_DCtx* dctx) const { ZSTD_freeDCtx(dctx); }
};

using ScopedZstdDecompressionContext =
    std::unique_ptr<ZSTD_DCtx, ZstdDecompressionContextDeleter>;

}  // namespace

// static
bool ZstdExtractor::IsZstdFile(const base::FilePath& image_path) {
  // `image_writer_private` is cross-platform (Windows, macOS, Linux, ChromeOS,
  // and Desktop Android). On Windows, disallow other processes from modifying
  // the archive while it is open (matching ZipExtractor, TarExtractor, and
  // XzExtractor); on POSIX, the `FLAG_WIN_*` flags are no-ops.
  base::File src_file(image_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                      base::File::FLAG_WIN_SHARE_DELETE);
  if (!src_file.IsValid()) {
    return false;
  }

  std::array<uint8_t, kExpectedMagic.size()> actual_magic = {};
  return src_file.ReadAtCurrentPosAndCheck(actual_magic) &&
         actual_magic == kExpectedMagic;
}

// static
void ZstdExtractor::Extract(ExtractionProperties properties) {
  ZstdExtractor extractor(std::move(properties));
  extractor.ExtractImpl();
}

ZstdExtractor::ZstdExtractor(ExtractionProperties properties)
    : properties_(std::move(properties)) {}

ZstdExtractor::~ZstdExtractor() = default;

void ZstdExtractor::ExtractImpl() {
  base::File src_file(properties_.image_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ |
                          // Do not allow others to write to the file.
                          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                          base::File::FLAG_WIN_SHARE_DELETE);
  if (!src_file.IsValid()) {
    std::move(properties_.failure_callback).Run(error::kUnzipGenericError);
    return;
  }

  const int64_t total_bytes = src_file.GetLength();
  if (total_bytes <= 0) {
    std::move(properties_.failure_callback).Run(error::kUnzipGenericError);
    return;
  }

  base::FilePath out_image_path =
      properties_.temp_dir_path.Append(kExtractedBinFileName);
  base::File dst_file(out_image_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                          // Do not allow others to read the file.
                          base::File::FLAG_WIN_EXCLUSIVE_READ |
                          // Do not allow others to write to the file.
                          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                          base::File::FLAG_WIN_SHARE_DELETE);
  if (!dst_file.IsValid()) {
    std::move(properties_.failure_callback).Run(error::kTempFileError);
    return;
  }

  ScopedZstdDecompressionContext dctx(ZSTD_createDCtx());
  if (!dctx || ZSTD_isError(ZSTD_initDStream(dctx.get()))) {
    std::move(properties_.failure_callback).Run(error::kUnzipGenericError);
    return;
  }

  std::move(properties_.open_callback).Run(out_image_path);
  properties_.progress_callback.Run(total_bytes, 0);

  std::vector<uint8_t> in_buffer(ZSTD_DStreamInSize());
  std::vector<uint8_t> out_buffer(ZSTD_DStreamOutSize());

  int64_t processed_bytes = 0;
  size_t last_ret = 1;
  bool decoded_any_input = false;

  while (true) {
    const std::optional<size_t> bytes_read =
        src_file.ReadAtCurrentPos(in_buffer);
    if (!bytes_read.has_value()) {
      std::move(properties_.failure_callback).Run(error::kUnzipGenericError);
      return;
    }
    if (*bytes_read == 0) {
      break;
    }

    decoded_any_input = true;
    processed_bytes += static_cast<int64_t>(*bytes_read);

    ZSTD_inBuffer input = {in_buffer.data(), *bytes_read, 0};
    bool wrote_output = false;
    while (input.pos < input.size) {
      ZSTD_outBuffer output = {out_buffer.data(), out_buffer.size(), 0};
      const size_t ret = ZSTD_decompressStream(dctx.get(), &output, &input);
      if (ZSTD_isError(ret)) {
        std::move(properties_.failure_callback).Run(error::kUnzipGenericError);
        return;
      }
      last_ret = ret;

      if (output.pos > 0) {
        if (!dst_file.WriteAtCurrentPosAndCheck(
                base::span(out_buffer).first(output.pos))) {
          std::move(properties_.failure_callback).Run(error::kTempFileError);
          return;
        }
        wrote_output = true;
      }
    }

    if (wrote_output || last_ret == 0) {
      properties_.progress_callback.Run(total_bytes, processed_bytes);
    }
  }

  if (!decoded_any_input || last_ret != 0) {
    std::move(properties_.failure_callback).Run(error::kUnzipGenericError);
    return;
  }

  src_file.Close();
  dst_file.Close();
  std::move(properties_.complete_callback).Run();
}

}  // namespace extensions::image_writer
