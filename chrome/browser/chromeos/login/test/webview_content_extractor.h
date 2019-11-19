// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_

#include <string>

namespace chromeos {
namespace test {

// Returns the contents of the <webview> identified by |element_ids|.
std::string GetWebViewContents(
    std::initializer_list<base::StringPiece> element_ids);

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_
