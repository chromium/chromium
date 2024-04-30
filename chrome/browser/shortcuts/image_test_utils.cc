// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/image_test_utils.h"

#include <string>

#include "base/base_paths.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace shortcuts {

base::expected<SkBitmap, std::string> LoadImageFromTestFile(
    const base::FilePath& relative_path_from_chrome_data) {
  CHECK_IS_TEST();
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Load image data from test directory.
  base::FilePath chrome_src_dir;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &chrome_src_dir)) {
    return base::unexpected("Could not find src directory.");
  }

  base::FilePath image_path =
      chrome_src_dir.Append(FILE_PATH_LITERAL("chrome/test/data"))
          .Append(relative_path_from_chrome_data);
  if (!base::PathExists(image_path)) {
    return base::unexpected(
        base::StrCat({"Path does not exist: ", image_path.AsUTF8Unsafe()}));
  }
  std::string image_data;
  if (!base::ReadFileToString(image_path, &image_data)) {
    return base::unexpected("Could not read file.");
  }

  SkBitmap image;
  if (!gfx::PNGCodec::Decode(
          reinterpret_cast<const uint8_t*>(image_data.data()),
          image_data.size(), &image)) {
    return base::unexpected("Could not decode file.");
  }
  return image;
}

}  // namespace shortcuts
