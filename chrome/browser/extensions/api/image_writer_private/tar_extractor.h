// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TAR_EXTRACTOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TAR_EXTRACTOR_H_

#include "base/files/file_util.h"
#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/browser/extensions/api/image_writer_private/single_file_tar_reader.h"

namespace base {
class File;
}  // namespace base

namespace extensions {
namespace image_writer {

class TarExtractor : public SingleFileTarReader::Delegate {
 public:
  static bool IsTarFile(const base::FilePath& image_path);

  // Start extracting the archive at |image_path| to |temp_dir_path| in
  // |properties|.
  static void Extract(ExtractionProperties properties);

  TarExtractor(const TarExtractor&) = delete;
  TarExtractor& operator=(const TarExtractor&) = delete;

 private:
  // This class manages its own lifetime.
  explicit TarExtractor(ExtractionProperties properties);
  ~TarExtractor() override;

  void ExtractImpl();
  void ExtractChunk();

  // SingleFileTarReader:
  int ReadTarFile(char* data, int size, std::string* error_id) override;
  bool WriteContents(const char* data,
                     int size,
                     std::string* error_id) override;

  SingleFileTarReader tar_reader_;

  base::File infile_;
  base::File outfile_;

  ExtractionProperties properties_;

  base::WeakPtrFactory<TarExtractor> weak_ptr_factory_{this};
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_TAR_EXTRACTOR_H_
