// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_content_browser_client_chromeos_part.h"

#include <string>

#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

bool UseDefaultFontSize(const std::string& url) {
  return ChromeContentBrowserClientChromeOsPart::UseDefaultFontSizeForTest(
      GURL(url));
}

std::string GetExtensionURL(const std::string& extension_id) {
  std::string url = extensions::kExtensionScheme;
  url += "://";
  url += extension_id;
  return url;
}

TEST(ChromeContentBrowserClientChromeOsPartTest, FontSizeForChromeUI) {
  struct TestCase {
    std::string url;
    bool is_system_ui;
  };
  // Just check some common examples, not an exhaustive list.
  TestCase test_cases[] = {
      {"https://google.com/", false}, {"about:blank", false},
      {"chrome://history", false},    {"chrome://settings", false},
      {"chrome://os-settings", true}, {"chrome://cellular-setup", true},
  };
  for (const TestCase& test_case : test_cases) {
    const std::string& url = test_case.url;
    EXPECT_EQ(test_case.is_system_ui, UseDefaultFontSize(url)) << url;
  }
}

TEST(ChromeContentBrowserClientChromeOsPartTest, FontSizeForApps) {
  struct TestCase {
    std::string extension_id;
    bool is_system_ui;
  };
  // Just check some common examples, not an exhaustive list.
  TestCase test_cases[] = {
      {extensions::kWebStoreAppId, false},
      {extension_misc::kPdfExtensionId, false},
      {extension_misc::kFilesManagerAppId, true},
      {extension_misc::kScreensaverAppId, true},
  };
  for (const TestCase& test_case : test_cases) {
    std::string url = GetExtensionURL(test_case.extension_id);
    EXPECT_EQ(test_case.is_system_ui, UseDefaultFontSize(url)) << url;
  }
}

}  // namespace
