// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar_features.h"

#include "base/test/scoped_feature_list.h"
#include "components/infobars/core/infobar_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace infobars {

namespace {

// Verifies the compiled-in defaults, i.e. the behavior without a Finch seed.
class InfoBarFeaturesDefaultsTest : public testing::Test {
 public:
  InfoBarFeaturesDefaultsTest() {
    // Ignore command-line and field-trial overrides.
    feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InfoBarFeaturesDefaultsTest, FrameworkEnabledByDefault) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(kCentralizedInfoBarFramework));
  EXPECT_FALSE(kEnableAll.Get());
}

// LINT.IfChange(InfoBarMigrationParams)
// Launched infobars must be migrated by default.
TEST_F(InfoBarFeaturesDefaultsTest, LaunchedInfoBarsAreMigratedByDefault) {
  constexpr InfoBarDelegate::InfoBarIdentifier kLaunched[] = {
      InfoBarDelegate::COLLECTED_COOKIES_INFOBAR_DELEGATE,
      InfoBarDelegate::INSTALLER_DOWNLOADER_INFOBAR_DELEGATE,
      InfoBarDelegate::PAGE_INFO_INFOBAR_DELEGATE,
      InfoBarDelegate::PDF_INFOBAR_DELEGATE,
  };
  for (InfoBarDelegate::InfoBarIdentifier id : kLaunched) {
    EXPECT_TRUE(IsInfoBarMigrated(id)) << "InfoBarIdentifier " << id;
  }
}

// Unlaunched infobars stay on the legacy path, controlled only by Finch.
TEST_F(InfoBarFeaturesDefaultsTest, UnlaunchedInfoBarsAreNotMigratedByDefault) {
  constexpr InfoBarDelegate::InfoBarIdentifier kUnlaunched[] = {
      InfoBarDelegate::ALTERNATE_NAV_INFOBAR_DELEGATE,
      InfoBarDelegate::AUTOMATION_INFOBAR_DELEGATE,
      InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE,
      InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE,
      InfoBarDelegate::DEV_TOOLS_INFOBAR_DELEGATE,
      InfoBarDelegate::DEV_TOOLS_REMOTE_DEBUGGING_INFOBAR_DELEGATE,
      InfoBarDelegate::DEV_TOOLS_SHARED_PROCESS_DELEGATE,
      InfoBarDelegate::FILE_ACCESS_DISABLED_INFOBAR_DELEGATE,
      InfoBarDelegate::GOOGLE_API_KEYS_INFOBAR_DELEGATE,
      InfoBarDelegate::INCOGNITO_CONNECTABILITY_INFOBAR_DELEGATE,
      InfoBarDelegate::JS_OPTIMIZATIONS_INFOBAR_DELEGATE,
      InfoBarDelegate::KEYSTONE_PROMOTION_INFOBAR_DELEGATE_MAC,
      InfoBarDelegate::KNOWN_INTERCEPTION_DISCLOSURE_INFOBAR_DELEGATE,
      InfoBarDelegate::ENABLE_LINK_CAPTURING_INFOBAR_DELEGATE,
      InfoBarDelegate::CHROME_FOR_TESTING_INFOBAR_DELEGATE,
      InfoBarDelegate::OBSOLETE_SYSTEM_INFOBAR_DELEGATE,
      InfoBarDelegate::OSCRYPTASYNC_AVAILABILITY_INFOBAR_DELEGATE,
      InfoBarDelegate::PIN_INFOBAR_DELEGATE,
      InfoBarDelegate::LOCAL_TEST_POLICIES_APPLIED_INFOBAR,
      InfoBarDelegate::THEME_INSTALLED_INFOBAR_DELEGATE,
      InfoBarDelegate::EXTENSION_DEV_TOOLS_INFOBAR_DELEGATE,
      InfoBarDelegate::SESSION_RESTORE_INFOBAR_DELEGATE,
      InfoBarDelegate::STARTUP_LAUNCH_INFOBAR_DELEGATE,
      InfoBarDelegate::TAB_SHARING_INFOBAR_DELEGATE,
      InfoBarDelegate::INSTALLATION_ERROR_INFOBAR_DELEGATE,
      InfoBarDelegate::WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE,
      InfoBarDelegate::EXTENSIONS_WEB_AUTH_FLOW_INFOBAR_DELEGATE,
  };
  for (InfoBarDelegate::InfoBarIdentifier id : kUnlaunched) {
    EXPECT_FALSE(IsInfoBarMigrated(id)) << "InfoBarIdentifier " << id;
  }
}
// LINT.ThenChange(//chrome/browser/infobars/infobar_features.cc:InfoBarMigrationParams)

TEST_F(InfoBarFeaturesDefaultsTest, UnknownInfoBarIsNotMigrated) {
  EXPECT_FALSE(IsInfoBarMigrated(InfoBarDelegate::TEST_INFOBAR));
}

}  // namespace

}  // namespace infobars
