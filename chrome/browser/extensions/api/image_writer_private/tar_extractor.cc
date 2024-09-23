// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/extensions/api/image_writer_private/tar_extractor.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/file_util_service.h"

// TarExtractor extracts a .tar file. The actual .tar file extraction is
// performed in a utility process.

namespace extensions {
namespace image_writer {

namespace {
constexpr base::FilePath::CharType kExtractedBinFileName[] =
    FILE_PATH_LITERAL("extracted.bin");

// https://www.gnu.org/software/tar/manual/html_node/Standard.html
constexpr char kExpectedMagic[5] = {'u', 's', 't', 'a', 'r'};
constexpr int kMagicOffset = 257;

}  // namespace

bool TarExtractor::IsTarFile(const base::FilePath& image_path) {
  base::File src_file(image_path, base::File::FLAG_OPEN |
                                      base::File::FLAG_READ |
                                      base::File::FLAG_WIN_EXCLUSIVE_WRITE |
                                      base::File::FLAG_WIN_SHARE_DELETE);
  if (!src_file.IsValid()) {
    return false;
  }

  // Tar header record is always 512 bytes, so if the file is shorter than that,
  // it's not tar.
  char header[512] = {};
  if (src_file.ReadAtCurrentPos(header, sizeof(header)) != sizeof(header)) {
    return false;
  }

  return std::equal(kExpectedMagic, kExpectedMagic + sizeof(kExpectedMagic),
                    header + kMagicOffset);
}

// static
void TarExtractor::Extract(ExtractionProperties properties) {
  // TarExtractor manages its own lifetime, and will delete itself when it
  // completes.
  TarExtractor* extractor = new TarExtractor(std::move(properties));
  extractor->ExtractImpl();
}

TarExtractor::TarExtractor(ExtractionProperties properties)
    : properties_(std::move(properties)) {}

TarExtractor::~TarExtractor() = default;

void TarExtractor::OnProgress(uint64_t total_bytes, uint64_t progress_bytes) {
  properties_.progress_callback.Run(total_bytes, progress_bytes);
}

void TarExtractor::ExtractImpl() {
  service_.Bind(LaunchFileUtilService());
  service_->BindSingleFileTarFileExtractor(
      remote_single_file_extractor_.BindNewPipeAndPassReceiver());

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

  // base::Unretained(this) is safe here because callback won't be called once
  // `remote_single_file_extractor_` is destroyed.
  remote_single_file_extractor_->Extract(
      std::move(src_file), std::move(dst_file),
      listener_.BindNewPipeAndPassRemote(),
      base::BindOnce(&TarExtractor::OnRemoteFinished, base::Unretained(this)));
}

void TarExtractor::OnRemoteFinished(
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

void TarExtractor::RunFailureCallbackAndDeleteThis(
    const std::string& error_id) {
  auto failure_callback = std::move(properties_.failure_callback);
  delete this;
  std::move(failure_callback).Run(error_id);
}

}  // namespace image_writer
}  // namespace extensions
