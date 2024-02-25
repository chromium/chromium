// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_

#include <string>
#include <string_view>

namespace ash {
namespace test {

// Returns the contents of the <webview> identified by `element_ids`.
std::string GetWebViewContents(
    std::initializer_list<std::string_view> element_ids);

// Returns the contents of the <webview> identified by `element_id`.
std::string GetWebViewContentsById(const std::string& element_id);

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_WEBVIEW_CONTENT_EXTRACTOR_H_
