// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZIP_EXTRACTOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZIP_EXTRACTOR_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "third_party/zlib/google/zip_reader.h"

namespace base {
class FilePath;
}

namespace extensions {
namespace image_writer {

// An extractor that supports extraction of zip-archived OS images.
class ZipExtractor {
 public:
  static bool IsZipFile(const base::FilePath& image_path);

  // Start extracting the archive at |image_path| to |temp_dir_path| in
  // |properties|.
  static void Extract(ExtractionProperties properties);

  ZipExtractor(const ZipExtractor&) = delete;
  ZipExtractor& operator=(const ZipExtractor&) = delete;

 private:
  // This class manages its own lifetime.
  explicit ZipExtractor(ExtractionProperties properties);
  ~ZipExtractor();

  void ExtractImpl();

  void OnError(const std::string& error);
  void OnComplete();

  zip::ZipReader zip_reader_;

  ExtractionProperties properties_;

  base::WeakPtrFactory<ZipExtractor> weak_ptr_factory_{this};
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ZIP_EXTRACTOR_H_
