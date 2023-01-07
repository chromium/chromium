// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_EXTRACTION_PROPERTIES_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_EXTRACTION_PROPERTIES_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace extensions {
namespace image_writer {

struct ExtractionProperties {
  ExtractionProperties();
  ExtractionProperties(ExtractionProperties&&);
  ~ExtractionProperties();

  using OpenCallback = base::OnceCallback<void(const base::FilePath&)>;
  using CompleteCallback = base::OnceClosure;
  using FailureCallback = base::OnceCallback<void(const std::string&)>;
  using ProgressCallback = base::RepeatingCallback<void(int64_t, int64_t)>;

  base::FilePath image_path;
  base::FilePath temp_dir_path;

  OpenCallback open_callback;
  CompleteCallback complete_callback;
  FailureCallback failure_callback;
  ProgressCallback progress_callback;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_EXTRACTION_PROPERTIES_H_
