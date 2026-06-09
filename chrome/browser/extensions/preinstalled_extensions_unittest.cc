// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/preinstalled_extensions.h"

#include <memory>

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/external_pref_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

// Android does not support Chrome Apps, but uses
// preinstalled_extensions::Provider to install extensions.
static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

// Chrome OS has different way of installing pre-installed apps.
static_assert(!BUILDFLAG(IS_CHROMEOS));

using extensions::mojom::ManifestLocation;
using preinstalled_extensions::Provider;

namespace extensions {
namespace {

class MockExternalProviderVisitor
    : public ExternalProviderInterface::VisitorInterface {
 public:
  MockExternalProviderVisitor() = default;
  ~MockExternalProviderVisitor() override = default;

  // ExternalProviderInterface::VisitorInterface:
  bool OnExternalExtensionFileFound(
      const ExternalInstallInfoFile& info) override {
    return true;
  }
  bool OnExternalExtensionUpdateUrlFound(
      const ExternalInstallInfoUpdateUrl& info,
      bool force_update) override {
    return true;
  }
  void OnExternalProviderReady(
      const ExternalProviderInterface* provider) override {}
  void OnExternalProviderUpdateComplete(
      const ExternalProviderInterface* provider,
      const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const std::vector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {}
};

int GetInstallState(Profile* profile) {
  return profile->GetPrefs()->GetInteger(
      prefs::kPreinstalledExtensionsInstallState);
}

class PreinstalledExtensionsTest : public testing::Test {
 public:
  PreinstalledExtensionsTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}
  ~PreinstalledExtensionsTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(PreinstalledExtensionsTest, Install) {
  TestingProfile profile;

  auto set_install_state = [](Profile* profile,
                              preinstalled_extensions::InstallState state) {
    return profile->GetPrefs()->SetInteger(
        prefs::kPreinstalledExtensionsInstallState, state);
  };

  EXPECT_EQ(preinstalled_extensions::kUnknown, GetInstallState(&profile));

  {
    // The pre-installed apps should be installed if
    // kPreinstalledExtensionsInstallState is unknown.
    Provider provider(&profile, nullptr, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_extensions_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_TRUE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_extensions::kAlreadyInstalledPreinstalledExtensions,
              GetInstallState(&profile));
  }

  {
    // The pre-installed apps should only be installed once.
    Provider provider(&profile, nullptr, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_extensions_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_extensions::kAlreadyInstalledPreinstalledExtensions,
              GetInstallState(&profile));
  }

  {
    // The pre-installed apps should not be installed if the state is
    // kNeverInstallPreinstalledExtensions
    set_install_state(
        &profile, preinstalled_extensions::kNeverInstallPreinstalledExtensions);
    Provider provider(&profile, nullptr, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_extensions_enabled());
    EXPECT_FALSE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_extensions::kNeverInstallPreinstalledExtensions,
              GetInstallState(&profile));
  }

  {
    // The old pre-installed apps with kProvideLegacyPreinstalledApps should be
    // migrated.
    set_install_state(&profile,
                      preinstalled_extensions::kProvideLegacyPreinstalledApps);
    Provider provider(&profile, nullptr, ManifestLocation::kInternal,
                      ManifestLocation::kInternal, Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_extensions_enabled());
    EXPECT_TRUE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_extensions::kAlreadyInstalledPreinstalledExtensions,
              GetInstallState(&profile));
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
                      preinstalled_extensions::kProvideLegacyPreinstalledApps);
    Provider provider(&default_testing_profile, nullptr,
                      ManifestLocation::kInternal, ManifestLocation::kInternal,
                      Extension::NO_FLAGS);
    EXPECT_TRUE(provider.preinstalled_extensions_enabled());
    EXPECT_TRUE(provider.is_migration());
    EXPECT_FALSE(provider.perform_new_installation());
    EXPECT_EQ(preinstalled_extensions::kAlreadyInstalledPreinstalledExtensions,
              GetInstallState(&default_testing_profile));
  }
}

TEST_F(PreinstalledExtensionsTest, DocsOfflineInstalledForBranded) {
  TestingProfile profile;
  MockExternalProviderVisitor visitor;

  // By default, apps/extensions aren't installed yet.
  ASSERT_EQ(preinstalled_extensions::kUnknown, GetInstallState(&profile));

  // The pre-installed apps/extensions will be installed. The `visitor` must be
  // non-null for prefs to be set.
  Provider provider(&profile, &visitor, ManifestLocation::kInternal,
                    ManifestLocation::kInternal, Extension::NO_FLAGS);
  ASSERT_TRUE(provider.preinstalled_extensions_enabled());

  // Compute the extension install prefs.
  provider.SetPrefs(base::DictValue());
  ASSERT_TRUE(provider.prefs_for_test().has_value());
  const base::DictValue& prefs = *provider.prefs_for_test();

  // Docs Offline is only bundled in branded builds.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  EXPECT_TRUE(prefs.contains(extension_misc::kDocsOfflineExtensionId));
#else
  EXPECT_FALSE(prefs.contains(extension_misc::kDocsOfflineExtensionId));
#endif
}

}  // namespace
}  // namespace extensions
