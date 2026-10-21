// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/zstd_extractor.h"

#include <stdint.h>

#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/file_util_service.h"

namespace extensions::image_writer {

namespace {

constexpr base::FilePath::StringViewType kExtractedBinFileName =
    FILE_PATH_LITERAL("extracted.bin");

// Zstandard frame magic number (0xFD2FB528 in little-endian byte order, per
// RFC 8878 Section 3.1.1).
constexpr std::array<uint8_t, 4> kExpectedMagic = {0x28, 0xb5, 0x2f, 0xfd};

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
std::unique_ptr<ZstdExtractor> ZstdExtractor::Extract(
    ExtractionProperties properties) {
  auto extractor = base::WrapUnique(new ZstdExtractor(std::move(properties)));
  extractor->ExtractImpl();
  if (!extractor->remote_single_file_extractor_.is_bound()) {
    return nullptr;
  }
  return extractor;
}

// static
std::unique_ptr<ZstdExtractor> ZstdExtractor::CreateForTesting(
    ExtractionProperties properties) {
  return base::WrapUnique(new ZstdExtractor(std::move(properties)));
}

ZstdExtractor::ZstdExtractor(ExtractionProperties properties)
    : properties_(std::move(properties)) {}

ZstdExtractor::~ZstdExtractor() = default;

void ZstdExtractor::OnProgress(uint64_t total_bytes, uint64_t progress_bytes) {
  // Avoid division by zero in the progress callback handler by not reporting
  // progress for 0-byte files.
  if (total_bytes == 0) {
    return;
  }
  // `ExtractionProperties::ProgressCallback` (`Operation::OnExtractProgress`)
  // takes signed `int64_t` byte counts (see crbug.com/489798713), so reject
  // values that would overflow `int64_t` when converted from `uint64_t`.
  if (total_bytes > std::numeric_limits<int64_t>::max() ||
      progress_bytes > total_bytes) {
    listener_.ReportBadMessage("invalid extraction progress values");
    return;
  }
  properties_.progress_callback.Run(total_bytes, progress_bytes);
}

void ZstdExtractor::ExtractImpl() {
  base::File src_file(properties_.image_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ |
                          // Do not allow others to write to the file.
                          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                          base::File::FLAG_WIN_SHARE_DELETE);
  if (!src_file.IsValid()) {
    ResetAndRunFailureCallback(error::kUnzipGenericError);
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
    ResetAndRunFailureCallback(error::kTempFileError);
    return;
  }
  std::move(properties_.open_callback).Run(out_image_path);

  service_.Bind(LaunchFileUtilService());
  service_->BindSingleFileZstdFileExtractor(
      remote_single_file_extractor_.BindNewPipeAndPassReceiver());
  // `base::Unretained(this)` is safe here because `this` owns
  // `remote_single_file_extractor_`, so the disconnect handler will not be
  // invoked after `this` is destroyed.
  remote_single_file_extractor_.set_disconnect_handler(
      base::BindOnce(&ZstdExtractor::ResetAndRunFailureCallback,
                     base::Unretained(this), error::kUnzipGenericError));

  remote_single_file_extractor_->Extract(
      std::move(src_file), std::move(dst_file),
      listener_.BindNewPipeAndPassRemote(),
      base::BindOnce(&ZstdExtractor::OnRemoteFinished, base::Unretained(this)));
}

void ZstdExtractor::OnRemoteFinished(
    chrome::file_util::mojom::ExtractionResult result) {
  switch (result) {
    case chrome::file_util::mojom::ExtractionResult::kSuccess: {
      auto complete_callback = std::move(properties_.complete_callback);
      ResetMojoState();
      std::move(complete_callback).Run();
      return;
    }
    case chrome::file_util::mojom::ExtractionResult::kGenericError: {
      ResetAndRunFailureCallback(error::kUnzipGenericError);
      return;
    }
    case chrome::file_util::mojom::ExtractionResult::kInvalidSrcFile: {
      ResetAndRunFailureCallback(error::kUnzipInvalidArchive);
      return;
    }
    case chrome::file_util::mojom::ExtractionResult::kDstFileError: {
      ResetAndRunFailureCallback(error::kTempFileError);
      return;
    }
  }
}

void ZstdExtractor::ResetMojoState() {
  listener_.reset();
  remote_single_file_extractor_.reset();
  service_.reset();
  properties_.open_callback.Reset();
  properties_.complete_callback.Reset();
  properties_.failure_callback.Reset();
  properties_.progress_callback.Reset();
}

void ZstdExtractor::ResetAndRunFailureCallback(const std::string& error_id) {
  auto failure_callback = std::move(properties_.failure_callback);
  ResetMojoState();
  std::move(failure_callback).Run(error_id);
}

}  // namespace extensions::image_writer
