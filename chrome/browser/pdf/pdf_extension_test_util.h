// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_
#define CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_

namespace content {
class WebContents;
}  // namespace content

namespace pdf_extension_test_util {

// Ensures that a PDF has finished loading inside the given |web_contents|.
// Returns true if it loads successfully or false if it fails. If it doesn't
// finish loading the test will hang.
bool EnsurePDFHasLoaded(content::WebContents* web_contents);

}  // namespace pdf_extension_test_util

#endif  // CHROME_BROWSER_PDF_PDF_EXTENSION_TEST_UTIL_H_
