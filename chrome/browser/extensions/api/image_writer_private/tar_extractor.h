// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TAR_EXTRACTOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TAR_EXTRACTOR_H_

#include <string>

#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/services/file_util/public/mojom/single_file_extractor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {
namespace image_writer {

class TarExtractor : public chrome::mojom::SingleFileExtractorListener {
 public:
  static bool IsTarFile(const base::FilePath& image_path);

  // Start extracting the archive at `image_path` to `temp_dir_path` in
  // `properties`.
  static void Extract(ExtractionProperties properties);

  TarExtractor(const TarExtractor&) = delete;
  TarExtractor& operator=(const TarExtractor&) = delete;

 private:
  // This class manages its own lifetime.
  explicit TarExtractor(ExtractionProperties properties);
  ~TarExtractor() override;

  // chrome::mojom::SingleFileExtractorListener implementation.
  void OnProgress(uint64_t total_bytes, uint64_t progress_bytes) override;

  void ExtractImpl();
  void OnRemoteFinished(chrome::file_util::mojom::ExtractionResult result);
  void RunFailureCallbackAndDeleteThis(const std::string& error_id);

  // `service_` is a class member so that the utility process where the actual
  // .tar file extraction is performed is kept alive while extraction is in
  // progress.
  mojo::Remote<chrome::mojom::FileUtilService> service_;

  mojo::Remote<chrome::mojom::SingleFileExtractor>
      remote_single_file_extractor_;

  // Listener receiver.
  // This class listens for tar extraction progress reports from the utility
  // process.
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener_{this};

  ExtractionProperties properties_;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TAR_EXTRACTOR_H_
