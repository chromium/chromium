// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZSTD_EXTRACTOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZSTD_EXTRACTOR_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/services/file_util/public/mojom/constants.mojom.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/services/file_util/public/mojom/single_file_extractor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions::image_writer {

// Single-stream Zstandard (.zst / .bin.zst) extractor for recovery images.
// Zstandard decompression uses a third-party library, so actual .zst
// extraction is performed in a utility process. Should be called from a
// SequencedTaskRunner context.
class ZstdExtractor : public chrome::mojom::SingleFileExtractorListener {
 public:
  static bool IsZstdFile(const base::FilePath& image_path);

  // Starts extracting the Zstandard archive at `properties.image_path` to
  // `properties.temp_dir_path`. Returns a `ZstdExtractor` instance managing the
  // extraction; destroying the returned instance terminates any in-progress
  // extraction and the utility process without invoking completion or failure
  // callbacks. A fixed file name is used for an extracted image, so a new
  // temporary directory has to be used for every `Extract()` call.
  [[nodiscard]] static std::unique_ptr<ZstdExtractor> Extract(
      ExtractionProperties properties);

  static std::unique_ptr<ZstdExtractor> CreateForTesting(
      ExtractionProperties properties);

  ZstdExtractor(const ZstdExtractor&) = delete;
  ZstdExtractor& operator=(const ZstdExtractor&) = delete;

  ~ZstdExtractor() override;

 private:
  explicit ZstdExtractor(ExtractionProperties properties);

  // chrome::mojom::SingleFileExtractorListener implementation.
  void OnProgress(uint64_t total_bytes, uint64_t progress_bytes) override;

  void ExtractImpl();
  void OnRemoteFinished(chrome::file_util::mojom::ExtractionResult result);
  void ResetMojoState();
  void ResetAndRunFailureCallback(const std::string& error_id);

  // `service_` is a class member so that the utility process where the actual
  // .zst file extraction is performed is kept alive while extraction is in
  // progress.
  mojo::Remote<chrome::mojom::FileUtilService> service_;

  mojo::Remote<chrome::mojom::SingleFileExtractor>
      remote_single_file_extractor_;

  // Listener receiver. This class listens for .zst extraction progress reports
  // from the utility process.
  mojo::Receiver<chrome::mojom::SingleFileExtractorListener> listener_{this};

  ExtractionProperties properties_;
};

}  // namespace extensions::image_writer

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZSTD_EXTRACTOR_H_
