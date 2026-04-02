// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/devtools_device_discovery.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(DevToolsDeviceDiscoveryTest, GetFrontendURLFromValue) {
  struct TestCase {
    const char* input_url;
    const char* browser_version;
    const char* expected_url;
  } test_cases[] = {
      // Basic case: no ws parameter.
      {"https://chrome-devtools-frontend.appspot.com/serve_rev/@123/"
       "inspector.html",
       "1.2.3.4",
       "https://chrome-devtools-frontend.appspot.com/serve_rev/@123/"
       "inspector.html?remoteVersion=1.2.3.4"},
      // ws parameter at the end.
      {"https://chrome-devtools-frontend.appspot.com/serve_rev/@123/"
       "inspector.html?ws=127.0.0.1:9222/devtools/page/1",
       "1.2.3.4",
       "https://chrome-devtools-frontend.appspot.com/serve_rev/@123/"
       "inspector.html?remoteVersion=1.2.3.4"},
      // ws parameter in the middle.
      {"https://chrome-devtools-frontend.appspot.com/serve_rev/@123/"
       "inspector.html?a=b&ws=127.0.0.1:9222/devtools/page/1&c=d",
       "1.2.3.4",
       "https://chrome-devtools-frontend.appspot.com/serve_rev/@123/"
       "inspector.html?a=b&c=d&remoteVersion=1.2.3.4"},
      // http to https conversion.
      {"http://chrome-devtools-frontend.appspot.com/serve_rev/@123/"
       "inspector.html",
       "1.2.3.4",
       "https://chrome-devtools-frontend.appspot.com/serve_rev/@123/"
       "inspector.html?remoteVersion=1.2.3.4"},
      // Relative URL with ws.
      {"/devtools/inspector.html?ws=127.0.0.1:9222/devtools/page/1", "1.2.3.4",
       "/devtools/inspector.html?remoteVersion=1.2.3.4"},
      // Relative URL with other params.
      {"/devtools/inspector.html?a=b&ws=127.0.0.1:9222/devtools/page/1&c=d",
       "1.2.3.4", "/devtools/inspector.html?a=b&c=d&remoteVersion=1.2.3.4"},
      // Multiple ws parameters.
      {"https://example.com/inspector.html?ws=1&ws=2&other=3", "1.2.3.4",
       "https://example.com/inspector.html?other=3&remoteVersion=1.2.3.4"},
      // Empty browser version.
      {"https://example.com/inspector.html?ws=1", "",
       "https://example.com/inspector.html"},
      // No query part.
      {"https://example.com/inspector.html", "",
       "https://example.com/inspector.html"},
      // Ref preservation.
      {"https://example.com/inspector.html?ws=1#ref", "1.2.3.4",
       "https://example.com/inspector.html?remoteVersion=1.2.3.4#ref"},
      // Relative URL with ref.
      {"/inspector.html?ws=1#ref", "1.2.3.4",
       "/inspector.html?remoteVersion=1.2.3.4#ref"},
      // Invalid URL (not relative, no scheme).
      {"invalid_url?ws=1", "1.2.3.4", "invalid_url?ws=1"},
  };

  for (const auto& test_case : test_cases) {
    base::DictValue value;
    value.Set("devtoolsFrontendUrl", test_case.input_url);
    std::string result = DevToolsDeviceDiscovery::GetFrontendURLFromValue(
        value, test_case.browser_version);
    EXPECT_EQ(test_case.expected_url, result)
        << "For input: " << test_case.input_url;
  }
}
