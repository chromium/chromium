// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_SCAN_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_SCANNING_SCAN_TEST_UTIL_H_

#include <string>

namespace ash {

// Returns a manually generated JPEG image. `alpha` is used to make them unique.
std::string CreateJpeg(const int alpha = 255);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SCANNING_SCAN_TEST_UTIL_H_
