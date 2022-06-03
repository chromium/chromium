// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_XZ_EXTRACTOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_XZ_EXTRACTOR_H_

#include <string>

#include "base/files/file.h"
#include "chrome/browser/extensions/api/image_writer_private/extraction_properties.h"
#include "chrome/browser/extensions/api/image_writer_private/single_file_tar_reader.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "chrome/services/file_util/public/mojom/xz_file_extractor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
}  // namespace base

namespace extensions {
namespace image_writer {

// .tar.xz archive extractor. Should be called from a SequencedTaskRunner
// context.
class XzExtractor : public SingleFileTarReader::Delegate {
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

  void ExtractImpl();
  void OnXzWritable(MojoResult result);
  void OnTarReadable(MojoResult result);
  void OnRemoteFinished(bool success);

  // |error_id| might be a member variable, so it cannot be a reference.
  void RunFailureCallbackAndDeleteThis(std::string error_id);

  // SingleFileTarReader::Delegate:
  SingleFileTarReader::Result ReadTarFile(char* data,
                                          uint32_t* size,
                                          std::string* error_id) override;
  bool WriteContents(const char* data,
                     int size,
                     std::string* error_id) override;

  mojo::Remote<chrome::mojom::FileUtilService> service_;
  mojo::Remote<chrome::mojom::XzFileExtractor> remote_xz_file_extractor_;

  SingleFileTarReader tar_reader_;

  base::File infile_;
  base::File outfile_;

  mojo::ScopedDataPipeProducerHandle xz_producer_;
  mojo::ScopedDataPipeConsumerHandle tar_consumer_;

  mojo::SimpleWatcher xz_producer_watcher_;
  mojo::SimpleWatcher tar_consumer_watcher_;

  ExtractionProperties properties_;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_XZ_EXTRACTOR_H_
