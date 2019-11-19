// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/default_apps.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using default_apps::Provider;

namespace extensions {

class MockExternalLoader : public ExternalLoader {
 public:
  MockExternalLoader() {}

  void StartLoading() override {}

 private:
  ~MockExternalLoader() override {}
};

class DefaultAppsTest : public testing::Test {
 public:
  DefaultAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~DefaultAppsTest() override {}

 private:
  content::BrowserTaskEnvironment task_environment_;
};

#if !defined(OS_CHROMEOS)
// Chrome OS has different way of installing default apps.
// Android does not currently support installing apps via Chrome.
TEST_F(DefaultAppsTest, Install) {
  std::unique_ptr<TestingProfile> profile(new TestingProfile());
  ExternalLoader* loader = new MockExternalLoader();

  Provider provider(profile.get(), NULL, loader, Manifest::INTERNAL,
                    Manifest::INTERNAL, Extension::NO_FLAGS);

  // The default apps should be installed if kDefaultAppsInstallState
  // is unknown.
  EXPECT_TRUE(provider.ShouldInstallInProfile());
  int state = profile->GetPrefs()->GetInteger(prefs::kDefaultAppsInstallState);
  EXPECT_TRUE(state == default_apps::kAlreadyInstalledDefaultApps);

  // The default apps should only be installed once.
  EXPECT_FALSE(provider.ShouldInstallInProfile());
  state = profile->GetPrefs()->GetInteger(prefs::kDefaultAppsInstallState);
  EXPECT_TRUE(state == default_apps::kAlreadyInstalledDefaultApps);

  // The default apps should not be installed if the state is
  // kNeverProvideDefaultApps
  profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
      default_apps::kNeverInstallDefaultApps);
  EXPECT_FALSE(provider.ShouldInstallInProfile());
  state = profile->GetPrefs()->GetInteger(prefs::kDefaultAppsInstallState);
  EXPECT_TRUE(state == default_apps::kNeverInstallDefaultApps);

  // The old default apps with kAlwaysInstallDefaultAppss should be migrated.
  profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
      default_apps::kProvideLegacyDefaultApps);
  EXPECT_TRUE(provider.ShouldInstallInProfile());
  state = profile->GetPrefs()->GetInteger(prefs::kDefaultAppsInstallState);
  EXPECT_TRUE(state == default_apps::kAlreadyInstalledDefaultApps);

  class DefaultTestingProfile : public TestingProfile {
    bool WasCreatedByVersionOrLater(const std::string& version) override {
      return false;
    }
  };
  profile.reset(new DefaultTestingProfile);
  Provider provider2(profile.get(), NULL, loader, Manifest::INTERNAL,
                     Manifest::INTERNAL, Extension::NO_FLAGS);
  // The old default apps with kProvideLegacyDefaultApps should be migrated
  // even if the profile version is older than Chrome version.
  profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
      default_apps::kProvideLegacyDefaultApps);
  EXPECT_TRUE(provider2.ShouldInstallInProfile());
  state = profile->GetPrefs()->GetInteger(prefs::kDefaultAppsInstallState);
  EXPECT_TRUE(state == default_apps::kAlreadyInstalledDefaultApps);
}
#endif

}  // namespace extensions
