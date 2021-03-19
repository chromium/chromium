// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/default_apps.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using default_apps::Provider;
using extensions::mojom::ManifestLocation;

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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Chrome OS has different way of installing default apps.
// Android does not currently support installing apps via Chrome.
TEST_F(DefaultAppsTest, Install) {
  TestingProfile profile;
  scoped_refptr<ExternalLoader> loader =
      base::MakeRefCounted<MockExternalLoader>();

  auto get_install_state = [](Profile* profile) {
    return profile->GetPrefs()->GetInteger(prefs::kDefaultAppsInstallState);
  };
  auto set_install_state = [](Profile* profile,
                              default_apps::InstallState state) {
    return profile->GetPrefs()->SetInteger(prefs::kDefaultAppsInstallState,
                                           state);
  };

  EXPECT_EQ(default_apps::kUnknown, get_install_state(&profile));

  {
    // The default apps should be installed if kDefaultAppsInstallState
    // is unknown.
    Provider provider(&profile, nullptr, loader, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.default_apps_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_TRUE(provider.perform_new_installation());
    EXPECT_EQ(default_apps::kAlreadyInstalledDefaultApps,
              get_install_state(&profile));
  }

  {
    // The default apps should only be installed once.
    Provider provider(&profile, nullptr, loader, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.default_apps_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(default_apps::kAlreadyInstalledDefaultApps,
              get_install_state(&profile));
  }

  {
    // The default apps should not be installed if the state is
    // kNeverProvideDefaultApps
    set_install_state(&profile, default_apps::kNeverInstallDefaultApps);
    Provider provider(&profile, nullptr, loader, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.default_apps_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(default_apps::kNeverInstallDefaultApps,
              get_install_state(&profile));
  }

  {
    // The old default apps with kAlwaysInstallDefaultApps should be migrated.
    set_install_state(&profile, default_apps::kProvideLegacyDefaultApps);
    Provider provider(&profile, nullptr, loader, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.default_apps_enabled());
    EXPECT_TRUE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(default_apps::kAlreadyInstalledDefaultApps,
              get_install_state(&profile));
  }

  class DefaultTestingProfile : public TestingProfile {
    bool WasCreatedByVersionOrLater(const std::string& version) override {
      return false;
    }
  };
  {
    DefaultTestingProfile default_testing_profile;
    // The old default apps with kProvideLegacyDefaultApps should be migrated
    // even if the profile version is older than Chrome version.
    set_install_state(&default_testing_profile,
                      default_apps::kProvideLegacyDefaultApps);
    Provider provider(&default_testing_profile, nullptr, loader,
                      ManifestLocation::kInternal, ManifestLocation::kInternal,
                      Extension::NO_FLAGS);
    EXPECT_TRUE(provider.default_apps_enabled());
    EXPECT_TRUE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(default_apps::kAlreadyInstalledDefaultApps,
              get_install_state(&default_testing_profile));
  }
}
#endif

}  // namespace extensions
