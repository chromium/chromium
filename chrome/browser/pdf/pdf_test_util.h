// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_TEST_UTIL_H_
#define CHROME_BROWSER_PDF_PDF_TEST_UTIL_H_

#include <memory>

namespace extensions {
class StreamContainer;
}  // namespace extensions

namespace pdf_test_util {

// Generates a sample `extensions::StreamContainer` to be used in unit tests.
// `container_number` is used as the tab ID and is appended to the extension ID
// and all URLs in the stream container.
std::unique_ptr<extensions::StreamContainer> GenerateSampleStreamContainer(
    int container_number);

}  // namespace pdf_test_util

#endif  // CHROME_BROWSER_PDF_PDF_TEST_UTIL_H_
