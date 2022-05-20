// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_UTIL_H_
#define ASH_TEST_ASH_TEST_UTIL_H_

namespace base {
class FilePath;
}

namespace ash::test {

// Takes a screenshot of the primary display and saves the screenshot picture to
// the location specified by `file_path`. Returns true if the screenshot is
// taken and saved successfully. Useful for debugging ash unit tests. When using
// this function on an ash unit test, the test code should be executed with
// --enable-pixel-output-in-tests flag.
// NOTE: `file_path` must end with the extension '.png'. If there is an existing
// file matching `file_path`, the existing file will be overwritten.
bool TakePrimaryDisplayScreenshotAndSave(const base::FilePath& file_path);

}  // namespace ash::test

#endif
