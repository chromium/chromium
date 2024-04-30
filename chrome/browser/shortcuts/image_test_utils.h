// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHORTCUTS_IMAGE_TEST_UTILS_H_
#define CHROME_BROWSER_SHORTCUTS_IMAGE_TEST_UTILS_H_

#include <string>

#include "base/types/expected.h"

class SkBitmap;

namespace base {
class FilePath;
}

namespace shortcuts {

// Loads an image from a test file relative to `../chrome/data/test/`. File
// reading errors are passed to the caller via a `base::unexpected` error, and
// needs to be handled at the callsite.
base::expected<SkBitmap, std::string> LoadImageFromTestFile(
    const base::FilePath& relative_path_from_chrome_data);

}  // namespace shortcuts

#endif  // CHROME_BROWSER_SHORTCUTS_IMAGE_TEST_UTILS_H_
