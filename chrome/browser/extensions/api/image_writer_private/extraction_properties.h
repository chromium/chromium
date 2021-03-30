// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_EXTRACTION_PROPERTIES_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_EXTRACTION_PROPERTIES_H_

#include "base/callback.h"
#include "base/files/file_path.h"

namespace extensions {
namespace image_writer {

struct ExtractionProperties {
  ExtractionProperties();
  ExtractionProperties(ExtractionProperties&&);
  ~ExtractionProperties();

  base::FilePath image_path;
  base::FilePath temp_dir_path;

  base::OnceCallback<void(const base::FilePath&)> open_callback;
  base::OnceClosure complete_callback;
  base::OnceCallback<void(const std::string&)> failure_callback;
  base::RepeatingCallback<void(int64_t, int64_t)> progress_callback;
};

}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_EXTRACTION_PROPERTIES_H_
