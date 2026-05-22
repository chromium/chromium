// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_util.h"

#include <string>
#include <vector>

#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace windows_util {

class WindowsUtilUnitTest : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeExtensionService(ExtensionServiceInitParams());
  }
};

TEST_F(WindowsUtilUnitTest, ShouldOpenIncognitoWindowIncognitoDisabled) {
  // Incognito disabled.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kDisabled);

  std::string error;
  std::vector<GURL> urls;
  urls.emplace_back("https://google.com");
  EXPECT_EQ(
      IncognitoResult::kError,
      ShouldOpenIncognitoWindow(profile(), /*incognito=*/true, &urls, &error));
  EXPECT_EQ("Incognito mode is disabled.", error);
}

TEST_F(WindowsUtilUnitTest, ShouldOpenIncognitoWindowIncognitoForced) {
  // Incognito forced.
  IncognitoModePrefs::SetAvailability(
      profile()->GetPrefs(), policy::IncognitoModeAvailability::kForced);

  std::string error;
  std::vector<GURL> urls;
  urls.emplace_back("https://google.com");
  EXPECT_EQ(
      IncognitoResult::kError,
      ShouldOpenIncognitoWindow(profile(), /*incognito=*/false, &urls, &error));
  EXPECT_EQ("Incognito mode is forced. Cannot open normal windows.", error);
}

TEST_F(WindowsUtilUnitTest, ShouldOpenIncognitoWindowIncompatibleURL) {
  std::string error;
  std::vector<GURL> urls;
  urls.emplace_back("chrome://history");
  EXPECT_EQ(
      IncognitoResult::kError,
      ShouldOpenIncognitoWindow(profile(), /*incognito=*/true, &urls, &error));
  EXPECT_EQ("Cannot open URL \"chrome://history/\" in an incognito window.",
            error);
  EXPECT_TRUE(urls.empty());
}

TEST_F(WindowsUtilUnitTest,
       ShouldOpenIncognitoWindowIncompatibleURLWithSomeLeft) {
  std::string error;
  std::vector<GURL> urls;
  urls.emplace_back("chrome://history");
  urls.emplace_back("https://google.com");
  EXPECT_EQ(
      IncognitoResult::kIncognito,
      ShouldOpenIncognitoWindow(profile(), /*incognito=*/true, &urls, &error));

  size_t expected_size = 1;
  EXPECT_EQ(expected_size, urls.size());
}

}  // namespace windows_util
