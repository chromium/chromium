// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_
#define CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class WebContents;
}  // namespace content

namespace pdf_extension_test_util {

// Ensures, inside the given `web_contents`, that a PDF has either finished
// loading or prompted a password. The result indicates success if the PDF loads
// successfully, otherwise it indicates failure. If it doesn't finish loading,
// the test will hang.
testing::AssertionResult EnsurePDFHasLoaded(content::WebContents* web_contents)
    WARN_UNUSED_RESULT;

}  // namespace pdf_extension_test_util

#endif  // CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_
