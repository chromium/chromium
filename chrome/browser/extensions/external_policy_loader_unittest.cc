// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>

#include "base/values.h"
#include "base/version.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

using content::BrowserThread;
using extensions::mojom::ManifestLocation;

namespace extensions {

class ExternalPolicyLoaderTest : public testing::Test {
 public:
  ExternalPolicyLoaderTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  ~ExternalPolicyLoaderTest() override {}

 private:
  // Needed to satisfy BrowserThread::CurrentlyOn(BrowserThread::UI) checks in
  // ExternalProviderImpl.
  content::BrowserTaskEnvironment task_environment_;
};

class MockExternalPolicyProviderVisitor
    : public ExternalProviderInterface::VisitorInterface {
 public:
  MockExternalPolicyProviderVisitor() {
  }

  MockExternalPolicyProviderVisitor(const MockExternalPolicyProviderVisitor&) =
      delete;
  MockExternalPolicyProviderVisitor& operator=(
      const MockExternalPolicyProviderVisitor&) = delete;

  // Initialize a provider with |policy_forcelist|, and check that it installs
  // exactly the extensions specified in |expected_extensions|.
  void Visit(const base::Value::Dict& policy_forcelist,
             const std::set<std::string>& expected_extensions) {
    profile_ = std::make_unique<TestingProfile>();
    profile_->GetTestingPrefService()->SetManagedPref(
        pref_names::kInstallForceList, base::Value(policy_forcelist.Clone()));
    provider_ = std::make_unique<ExternalProviderImpl>(
        this,
        new ExternalPolicyLoader(
            profile_.get(),
            ExtensionManagementFactory::GetForBrowserContext(profile_.get()),
            ExternalPolicyLoader::FORCED),
        profile_.get(), ManifestLocation::kInvalidLocation,
        ManifestLocation::kExternalPolicyDownload, Extension::NO_FLAGS);

    // Extensions will be removed from this list as they visited,
    // so it should be emptied by the end.
    expected_extensions_ = expected_extensions;
    provider_->VisitRegisteredExtension();
    EXPECT_TRUE(expected_extensions_.empty());
  }

  bool OnExternalExtensionFileFound(
      const extensions::ExternalInstallInfoFile& info) override {
    ADD_FAILURE() << "There should be no external extensions from files.";
    return false;
  }

  bool OnExternalExtensionUpdateUrlFound(
      const extensions::ExternalInstallInfoUpdateUrl& info,
      bool force_update) override {
    // Extension has the correct location.
    EXPECT_EQ(ManifestLocation::kExternalPolicyDownload,
              info.download_location);

    // Provider returns the correct location when asked.
    ManifestLocation location1;
    std::unique_ptr<base::Version> version1;
    provider_->GetExtensionDetails(info.extension_id, &location1, &version1);
    EXPECT_EQ(ManifestLocation::kExternalPolicyDownload, location1);
    EXPECT_FALSE(version1.get());

    // Remove the extension from our list.
    EXPECT_EQ(1U, expected_extensions_.erase(info.extension_id));
    return true;
  }

  void OnExternalProviderReady(
      const ExternalProviderInterface* provider) override {
    EXPECT_EQ(provider, provider_.get());
    EXPECT_TRUE(provider->IsReady());
  }

  void OnExternalProviderUpdateComplete(
      const ExternalProviderInterface* provider,
      const std::vector<ExternalInstallInfoUpdateUrl>& update_url_extensions,
      const std::vector<ExternalInstallInfoFile>& file_extensions,
      const std::set<std::string>& removed_extensions) override {
    ADD_FAILURE() << "Only win registry provider is expected to call this.";
  }

 private:
  std::set<std::string> expected_extensions_;

  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<ExternalProviderImpl> provider_;
};

TEST_F(ExternalPolicyLoaderTest, PolicyIsParsed) {
  base::Value::Dict forced_extensions;
  std::set<std::string> expected_extensions;
  extensions::ExternalPolicyLoader::AddExtension(
      forced_extensions, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      "http://www.example.com/crx?a=5;b=6");
  expected_extensions.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  extensions::ExternalPolicyLoader::AddExtension(
      forced_extensions, "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
      "https://clients2.google.com/service/update2/crx");
  expected_extensions.insert("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

  MockExternalPolicyProviderVisitor mv;
  mv.Visit(forced_extensions, expected_extensions);
}

TEST_F(ExternalPolicyLoaderTest, InvalidEntriesIgnored) {
  base::Value::Dict forced_extensions;
  std::set<std::string> expected_extensions;

  extensions::ExternalPolicyLoader::AddExtension(
      forced_extensions, "cccccccccccccccccccccccccccccccc",
      "http://www.example.com/crx");
  expected_extensions.insert("cccccccccccccccccccccccccccccccc");

  // Add invalid entries.
  forced_extensions.Set("invalid", "http://www.example.com/crx");
  forced_extensions.Set("dddddddddddddddddddddddddddddddd", std::string());
  forced_extensions.Set("invalid", "bad");

  MockExternalPolicyProviderVisitor mv;
  mv.Visit(forced_extensions, expected_extensions);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ExternalPolicyLoaderAshTest : public ExternalPolicyLoaderTest {
 public:
  void SetUp() override {
    ExternalPolicyLoaderTest::SetUp();
    // This setup is required to set the primary profile, which in turn is
    // required to enabled Lacros.
    fake_user_manager_ = new ash::FakeChromeUserManager;
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_.get()));

    AccountId account_id = AccountId::FromUserEmail("test@gmail.com");
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
  }

 private:
  raw_ptr<ash::FakeChromeUserManager, DanglingUntriaged | ExperimentalAsh>
      fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(ExternalPolicyLoaderAshTest, BlockNonOSExtensionsIfAshBrowserDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(ash::standalone_browser::GetFeatureRefs(), {});
  ASSERT_FALSE(crosapi::browser_util::IsAshWebBrowserEnabled());

  base::Value::Dict forced_extensions;
  std::set<std::string> expected_extensions;

  // Add an arbitrary extension.
  ExternalPolicyLoader::AddExtension(forced_extensions,
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                     "http://www.example.com/crx");
  // Add an extension in keep list. Check `ExtensionRunsInOS()` for details.
  ExternalPolicyLoader::AddExtension(
      forced_extensions, extension_misc::kAccessibilityCommonExtensionId,
      "http://www.access.com/crx");
  // Add an extension app in keep list. Check `ExtensionAppRunsInOS()` for
  // details.
  ExternalPolicyLoader::AddExtension(forced_extensions,
                                     extension_misc::kGnubbyAppId,
                                     "http://www.gnubby.com/crx");

  // Only extensions that are allowed to run in Ash should be added i.e. an
  // arbitrary non-OS extension should not be installed.
  expected_extensions.insert(extension_misc::kAccessibilityCommonExtensionId);
  expected_extensions.insert(extension_misc::kGnubbyAppId);
  MockExternalPolicyProviderVisitor mv;
  mv.Visit(forced_extensions, expected_extensions);
}

TEST_F(ExternalPolicyLoaderAshTest, AllowNonOSExtensionsIfAshBrowserEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, ash::standalone_browser::GetFeatureRefs());
  ASSERT_TRUE(crosapi::browser_util::IsAshWebBrowserEnabled());

  base::Value::Dict forced_extensions;
  std::set<std::string> expected_extensions;

  ExternalPolicyLoader::AddExtension(forced_extensions,
                                     "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                     "http://www.example.com/crx");
  ExternalPolicyLoader::AddExtension(
      forced_extensions, extension_misc::kAccessibilityCommonExtensionId,
      "http://www.access.com/crx");
  ExternalPolicyLoader::AddExtension(forced_extensions,
                                     extension_misc::kGnubbyAppId,
                                     "http://www.gnubby.com/crx");

  // If Ash is running as a web browser, all extensions should be added.
  expected_extensions.insert("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  expected_extensions.insert(extension_misc::kAccessibilityCommonExtensionId);
  expected_extensions.insert(extension_misc::kGnubbyAppId);
  MockExternalPolicyProviderVisitor mv;
  mv.Visit(forced_extensions, expected_extensions);
}
#endif

}  // namespace extensions
