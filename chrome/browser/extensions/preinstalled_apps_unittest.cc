// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/preinstalled_apps.h"

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

using extensions::mojom::ManifestLocation;
using preinstalled_apps::Provider;

namespace extensions {

class MockExternalLoader : public ExternalLoader {
 public:
  MockExternalLoader() {}

  void StartLoading() override {}

 private:
  ~MockExternalLoader() override {}
};

class PreinstalledAppsTest : public testing::Test {
 public:
  PreinstalledAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~PreinstalledAppsTest() override {}

 private:
  content::BrowserTaskEnvironment task_environment_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Chrome OS has different way of installing pre-installed apps.
// Android does not currently support installing apps via Chrome.
TEST_F(PreinstalledAppsTest, Install) {
  TestingProfile profile;
  scoped_refptr<ExternalLoader> loader =
      base::MakeRefCounted<MockExternalLoader>();

  auto get_install_state = [](Profile* profile) {
    return profile->GetPrefs()->GetInteger(
        prefs::kPreinstalledAppsInstallState);
  };
  auto set_install_state = [](Profile* profile,
                              preinstalled_apps::InstallState state) {
    return profile->GetPrefs()->SetInteger(prefs::kPreinstalledAppsInstallState,
                                           state);
  };

  EXPECT_EQ(preinstalled_apps::kUnknown, get_install_state(&profile));

  {
    // The pre-installed apps should be installed if
    // kPreinstalledAppsInstallState is unknown.
    Provider provider(&profile, nullptr, loader, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_apps_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_TRUE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_apps::kAlreadyInstalledPreinstalledApps,
              get_install_state(&profile));
  }

  {
    // The pre-installed apps should only be installed once.
    Provider provider(&profile, nullptr, loader, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_apps_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_apps::kAlreadyInstalledPreinstalledApps,
              get_install_state(&profile));
  }

  {
    // The pre-installed apps should not be installed if the state is
    // kNeverProvidePreinstalledApps
    set_install_state(&profile,
                      preinstalled_apps::kNeverInstallPreinstalledApps);
    Provider provider(&profile, nullptr, loader, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_apps_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_apps::kNeverInstallPreinstalledApps,
              get_install_state(&profile));
  }

  {
    // The old pre-installed apps with kAlwaysInstallPreinstalledApps should be
    // migrated.
    set_install_state(&profile,
                      preinstalled_apps::kProvideLegacyPreinstalledApps);
    Provider provider(&profile, nullptr, loader, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_apps_enabled());
    EXPECT_TRUE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_apps::kAlreadyInstalledPreinstalledApps,
              get_install_state(&profile));
  }

  class DefaultTestingProfile : public TestingProfile {
    bool WasCreatedByVersionOrLater(const std::string& version) override {
      return false;
    }
  };
  {
    DefaultTestingProfile default_testing_profile;
    // The old pre-installed apps with kProvideLegacyPreinstalledApps should be
    // migrated even if the profile version is older than Chrome version.
    set_install_state(&default_testing_profile,
                      preinstalled_apps::kProvideLegacyPreinstalledApps);
    Provider provider(&default_testing_profile, nullptr, loader,
                      ManifestLocation::kInternal, ManifestLocation::kInternal,
                      Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_apps_enabled());
    EXPECT_TRUE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_apps::kAlreadyInstalledPreinstalledApps,
              get_install_state(&default_testing_profile));
  }
}
#endif

}  // namespace extensions
