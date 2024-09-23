// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/favicon/favicon_util.h"

#include <string_view>

#include "components/favicon_base/favicon_url_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

TEST(FaviconUtilUnittest, Parse) {
  const struct {
    bool parse_should_succeed;
    std::string_view url;
  } test_cases[] = {
      {false, "chrome-extension://id"},
      {false, "chrome-extension://id/"},
      {false, "chrome-extension://id?"},
      {false, "chrome-extension://id/?"},
      {false, "chrome-extension://id/_favicon"},
      {false, "chrome-extension://id/_favicon/"},
      {false, "chrome-extension://id/_favicon/?"},
      {true, "chrome-extension://id/_favicon?pageUrl=https://ok.com"},
      {true, "chrome-extension://id/_favicon/?pageUrl=https://ok.com"},
      {true, "chrome-extension://id/_favicon/?pageUrl=https://ok.com&size=16"},
      {true,
       "chrome-extension://id/_favicon/?pageUrl=https://"
       "ok.com&size=16&scaleFactor=1.0x&server_fallback=1"}};
  for (const auto& test_case : test_cases) {
    GURL url(test_case.url);
    chrome::ParsedFaviconPath parsed;
    EXPECT_EQ(test_case.parse_should_succeed,
              favicon_util::ParseFaviconPath(url, &parsed));
  }
}

}  // namespace extensions
