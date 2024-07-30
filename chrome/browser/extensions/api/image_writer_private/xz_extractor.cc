// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/xz_extractor.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/file_util_service.h"

// XzExtractor extracts a .tar.xz file. Xz extraction uses third party
// libraries, so actual .tar.xz extraction is performed in a utility process.

namespace extensions {
namespace image_writer {

namespace {

constexpr base::FilePath::StringPieceType kExtractedBinFileName =
    FILE_PATH_LITERAL("extracted.bin");

// https://tukaani.org/xz/xz-file-format-1.0.4.txt
constexpr uint8_t kExpectedMagic[6] = {0xfd, '7', 'z', 'X', 'Z', 0x00};

}  // namespace

bool XzExtractor::IsXzFile(const base::FilePath& image_path) {
  base::File src_file(image_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                      base::File::FLAG_WIN_SHARE_DELETE);
  if (!src_file.IsValid()) {
    return false;
  }

  constexpr size_t kExpectedSize = sizeof(kExpectedMagic);
  uint8_t actual_magic[kExpectedSize] = {};
  return src_file.ReadAtCurrentPosAndCheck(actual_magic) &&
         base::ranges::equal(kExpectedMagic, actual_magic);
}

// static
void XzExtractor::Extract(ExtractionProperties properties) {
  // XzExtractor manages its own lifetime, and will delete itself when it
  // completes.
  XzExtractor* extractor = new XzExtractor(std::move(properties));
  extractor->ExtractImpl();
}

XzExtractor::XzExtractor(ExtractionProperties properties)
    : properties_(std::move(properties)) {}

XzExtractor::~XzExtractor() = default;

void XzExtractor::OnProgress(uint64_t total_bytes, uint64_t progress_bytes) {
  properties_.progress_callback.Run(total_bytes, progress_bytes);
}

void XzExtractor::ExtractImpl() {
  // TODO(b/254591810): Run on a pooled worker thread to avoid blocking
  // operation on the main UI thread.
  base::File src_file(properties_.image_path,
                      base::File::FLAG_OPEN | base::File::FLAG_READ |
                          // Do not allow others to write to the file.
                          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                          base::File::FLAG_WIN_SHARE_DELETE);
  if (!src_file.IsValid()) {
    RunFailureCallbackAndDeleteThis(error::kUnzipGenericError);
    return;
  }

  base::FilePath out_image_path =
      properties_.temp_dir_path.Append(kExtractedBinFileName);
  // TODO(b/254591810): Run on a pooled worker thread to avoid blocking
  // operation on the main UI thread.
  base::File dst_file(out_image_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
                          // Do not allow others to read the file.
                          base::File::FLAG_WIN_EXCLUSIVE_READ |
                          // Do not allow others to write to the file.
                          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                          base::File::FLAG_WIN_SHARE_DELETE);
  if (!dst_file.IsValid()) {
    RunFailureCallbackAndDeleteThis(error::kTempFileError);
    return;
  }
  std::move(properties_.open_callback).Run(out_image_path);

  service_.Bind(LaunchFileUtilService());
  service_->BindSingleFileTarXzFileExtractor(
      remote_single_file_extractor_.BindNewPipeAndPassReceiver());

  remote_single_file_extractor_->Extract(
      std::move(src_file), std::move(dst_file),
      listener_.BindNewPipeAndPassRemote(),
      base::BindOnce(&XzExtractor::OnRemoteFinished, base::Unretained(this)));
}

void XzExtractor::OnRemoteFinished(
    chrome::file_util::mojom::ExtractionResult result) {
  switch (result) {
    case chrome::file_util::mojom::ExtractionResult::kSuccess: {
      auto complete_callback = std::move(properties_.complete_callback);
      delete this;
      std::move(complete_callback).Run();
      return;
    }
    case chrome::file_util::mojom::ExtractionResult::kGenericError: {
      RunFailureCallbackAndDeleteThis(error::kUnzipGenericError);
      return;
    }
    case chrome::file_util::mojom::ExtractionResult::kInvalidSrcFile: {
      RunFailureCallbackAndDeleteThis(error::kUnzipInvalidArchive);
      return;
    }
    case chrome::file_util::mojom::ExtractionResult::kDstFileError: {
      RunFailureCallbackAndDeleteThis(error::kTempFileError);
      return;
    }
  }
}

void XzExtractor::RunFailureCallbackAndDeleteThis(const std::string& error_id) {
  auto failure_callback = std::move(properties_.failure_callback);
  delete this;
  std::move(failure_callback).Run(error_id);
}

}  // namespace image_writer
}  // namespace extensions
