// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZSTD_EXTRACTOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZSTD_EXTRACTOR_H_

#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions::image_writer {

// Single-stream Zstandard (.zst / .bin.zst) extractor for recovery images.
class ZstdExtractor {
 public:
  static bool IsZstdFile(const base::FilePath& image_path);

  // Starts extracting the Zstandard archive at `properties.image_path` to
  // `properties.temp_dir_path`.
  static void Extract(ExtractionProperties properties);

  ZstdExtractor(const ZstdExtractor&) = delete;
  ZstdExtractor& operator=(const ZstdExtractor&) = delete;

 private:
  explicit ZstdExtractor(ExtractionProperties properties);
  ~ZstdExtractor();

  void ExtractImpl();

  ExtractionProperties properties_;
};

}  // namespace extensions::image_writer

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZSTD_EXTRACTOR_H_
