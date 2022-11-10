// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_XZ_EXTRACTOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_XZ_EXTRACTOR_H_

#include <string>

#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/services/file_util/public/mojom/single_file_tar_xz_file_extractor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
namespace image_writer {

// .tar.xz archive extractor. Should be called from a SequencedTaskRunner
// context.
class XzExtractor : public chrome::mojom::SingleFileTarXzFileExtractorListener {
 public:
  static bool IsXzFile(const base::FilePath& image_path);

  // Start extracting the archive at |properties.image_path| to
  // |properties.temp_dir_path|. A fixed file name is used for an extracted
  // image, so a new temporary directory has to be used for every Extract()
  // call.
  static void Extract(ExtractionProperties properties);

  XzExtractor(const XzExtractor&) = delete;
  XzExtractor& operator=(const XzExtractor&) = delete;

 private:
  // This class manages its own lifetime.
  explicit XzExtractor(ExtractionProperties properties);
  ~XzExtractor() override;

  // chrome::mojom::SingleFileTarXzFileExtractorListener implementation.
  void OnProgress(uint64_t total_bytes, uint64_t progress_bytes) override;

  void ExtractImpl();
  void OnRemoteFinished(chrome::file_util::mojom::ExtractionResult result);
  // |error_id| might be a member variable, so it cannot be a reference.
  void RunFailureCallbackAndDeleteThis(std::string error_id);

  mojo::Remote<chrome::mojom::FileUtilService> service_;
  mojo::Remote<chrome::mojom::SingleFileTarXzFileExtractor>
      remote_single_file_tar_xz_file_extractor_;

  // Listener receiver.
  // This class listens for .tar.xz extraction progress reports from the utility
  // process.
  mojo::Receiver<chrome::mojom::SingleFileTarXzFileExtractorListener> listener_{
      this};

  ExtractionProperties properties_;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_XZ_EXTRACTOR_H_
