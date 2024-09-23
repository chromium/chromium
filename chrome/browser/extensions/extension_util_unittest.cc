// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/test_extension_dir.h"
#include "url/gurl.h"

namespace extensions {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kExtensionUpdateUrl[] =
    "https://clients2.google.com/service/update2/crx";  // URL of Chrome Web
                                                        // Store backend.
#endif

}  // namespace

class ExtensionUtilUnittest : public ExtensionServiceTestBase {
 public:
  void SetUp() override { InitializeEmptyExtensionService(); }
};

TEST_F(ExtensionUtilUnittest, SetAllowFileAccess) {
  constexpr char kManifest[] =
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 2,
           "permissions": ["<all_urls>"]
         })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);

  ChromeTestExtensionLoader loader(profile());
  // An unpacked extension would get file access by default, so disabled it on
  // the loader.
  loader.set_allow_file_access(false);

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(dir.UnpackedPath());
  const std::string extension_id = extension->id();

  GURL file_url("file://etc");
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents.get()).id();

  // Initially the file access pref will be false and the extension will not be
  // able to capture a file URL page.
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Calling SetAllowFileAccess should reload the extension with file access.
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), true);
    extension = observer.WaitForExtensionInstalled();
  }

  EXPECT_TRUE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_TRUE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Removing the file access should reload the extension again back to not
  // having file access.
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), false);
    extension = observer.WaitForExtensionInstalled();
  }

  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));
}

TEST_F(ExtensionUtilUnittest, SetAllowFileAccessWhileDisabled) {
  constexpr char kManifest[] =
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 2,
           "permissions": ["<all_urls>"]
         })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);

  ChromeTestExtensionLoader loader(profile());
  // An unpacked extension would get file access by default, so disabled it on
  // the loader.
  loader.set_allow_file_access(false);

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(dir.UnpackedPath());
  const std::string extension_id = extension->id();

  GURL file_url("file://etc");
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents.get()).id();

  // Initially the file access pref will be false and the extension will not be
  // able to capture a file URL page.
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Disabling the extension then calling SetAllowFileAccess should reload the
  // extension with file access.
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), true);
    extension = observer.WaitForExtensionInstalled();
  }
  // The extension should still be disabled.
  EXPECT_FALSE(service()->IsExtensionEnabled(extension_id));

  service()->EnableExtension(extension_id);
  EXPECT_TRUE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_TRUE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Disabling the extension and then removing the file access should reload it
  // again back to not having file access. Regression test for
  // crbug.com/1385343.
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), false);
    extension = observer.WaitForExtensionInstalled();
  }
  // The extension should still be disabled.
  EXPECT_FALSE(service()->IsExtensionEnabled(extension_id));

  service()->EnableExtension(extension_id);
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));
}

TEST_F(ExtensionUtilUnittest, HasIsolatedStorage) {
  // Platform apps should have isolated storage.
  scoped_refptr<const Extension> app =
      ExtensionBuilder("foo_app", ExtensionBuilder::Type::PLATFORM_APP).Build();
  EXPECT_TRUE(app->is_platform_app());
  EXPECT_TRUE(util::HasIsolatedStorage(*app.get(), profile()));

  // Extensions should not have isolated storage.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo_ext").Build();
  EXPECT_FALSE(extension->is_platform_app());
  EXPECT_FALSE(util::HasIsolatedStorage(*extension.get(), profile()));
}

TEST_F(ExtensionUtilUnittest, FixupLongExtensionName) {
  const std::string long_extension_name =
      "A very long extension name etc A very long extension name etc A very "
      "long extension name etc A very long extension name etc";
  std::u16string expected_fixup_extension_name =
      u"A very long extension name etc A very long extension name etc A very "
      u"long\u2026";

  std::u16string fixup_extension_name =
      util::GetFixupExtensionNameForUIDisplay(long_extension_name);
  EXPECT_EQ(fixup_extension_name, expected_fixup_extension_name);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ExtensionUtilWithSigninProfileUnittest : public ExtensionUtilUnittest {
 public:
  void SetUp() override {
    ExtensionUtilUnittest::SetUp();

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal(), &testing_local_state_);
    ASSERT_TRUE(testing_profile_manager_->SetUp());
    auto policy_service = std::make_unique<policy::PolicyServiceImpl>(
        std::vector<
            raw_ptr<policy::ConfigurationPolicyProvider, VectorExperimental>>{
            policy_provider()});
    signin_profile_ = testing_profile_manager_->CreateTestingProfile(
        chrome::kInitialProfile, /*prefs=*/nullptr,
        base::UTF8ToUTF16(chrome::kInitialProfile), 0,
        TestingProfile::TestingFactories(),
        /*is_supervised_profile=*/false, /*is_new_profile=*/std::nullopt,
        std::move(policy_service));
    signin_profile_prefs_ = signin_profile_->GetTestingPrefService();
  }

  void TearDown() override {
    signin_profile_ = nullptr;
    signin_profile_prefs_ = nullptr;
    testing_profile_manager_->DeleteAllTestingProfiles();
    ExtensionUtilUnittest::TearDown();
  }

  scoped_refptr<const Extension> BuildPolicyInstalledExtension() {
    return ExtensionBuilder("foo_ext")
        .SetLocation(mojom::ManifestLocation::kExternalPolicyDownload)
        .Build();
  }

  void SetupForceList(const ExtensionIdList& extension_ids) {
    base::Value::Dict dict = base::Value::Dict();
    for (const auto& extension_id : extension_ids) {
      dict.Set(extension_id,
               base::Value::Dict().Set(ExternalProviderImpl::kExternalUpdateUrl,
                                       kExtensionUpdateUrl));
    }
    signin_profile_prefs_->SetManagedPref(pref_names::kInstallForceList,
                                          std::move(dict));
  }

 protected:
  raw_ptr<TestingProfile> signin_profile_;

 private:
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  raw_ptr<sync_preferences::TestingPrefServiceSyncable> signin_profile_prefs_;
};

// HasIsolatedStorage() will be called when an extension is disabled, more
// precisely when its service worker is unregistered. At that moment the
// extension is already added to the disabled list of the extension registry.
// The method needs to still be able to correctly specify if the extension's
// storage is isolated or not, even if the extension is disabled.
// Regression test for b/279763783.
TEST_F(ExtensionUtilWithSigninProfileUnittest,
       HasIsolatedStorageOnDisabledExtension) {
  scoped_refptr<const Extension> policy_extension =
      BuildPolicyInstalledExtension();
  const std::string& policy_extension_id = policy_extension->id();
  EXPECT_FALSE(policy_extension->is_platform_app());

  // Extension enabled.
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(signin_profile_);
  extension_registry->AddEnabled(policy_extension);
  EXPECT_TRUE(util::HasIsolatedStorage(policy_extension_id, signin_profile_));

  // Extension disabled.
  extension_registry->RemoveEnabled(policy_extension_id);
  extension_registry->AddDisabled(policy_extension);
  EXPECT_TRUE(util::HasIsolatedStorage(policy_extension_id, signin_profile_));

  // Extension neither enabled, nor disabled.
  extension_registry->RemoveDisabled(policy_extension_id);
  EXPECT_FALSE(util::HasIsolatedStorage(policy_extension_id, signin_profile_));
}

TEST_F(ExtensionUtilWithSigninProfileUnittest,
       HasIsolatedStorageOnTerminatedOrBlockedExtension) {
  scoped_refptr<const Extension> policy_extension =
      BuildPolicyInstalledExtension();
  const std::string& policy_extension_id = policy_extension->id();
  EXPECT_FALSE(policy_extension->is_platform_app());

  // Extension enabled.
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(signin_profile_);
  extension_registry->AddEnabled(policy_extension);
  EXPECT_TRUE(util::HasIsolatedStorage(policy_extension_id, signin_profile_));

  // Extension terminated.
  extension_registry->RemoveEnabled(policy_extension_id);
  extension_registry->AddTerminated(policy_extension);
  EXPECT_TRUE(util::HasIsolatedStorage(policy_extension_id, signin_profile_));

  // Extension blockedlisted.
  extension_registry->RemoveTerminated(policy_extension_id);
  extension_registry->AddBlocklisted(policy_extension);
  EXPECT_TRUE(util::HasIsolatedStorage(policy_extension_id, signin_profile_));

  // Extension blocked.
  extension_registry->RemoveBlocklisted(policy_extension_id);
  extension_registry->AddBlocked(policy_extension);
  EXPECT_TRUE(util::HasIsolatedStorage(policy_extension_id, signin_profile_));

  // Extension not found.
  extension_registry->RemoveBlocked(policy_extension_id);
  EXPECT_FALSE(util::HasIsolatedStorage(policy_extension_id, signin_profile_));
}

// Verifies that the force-installed extension policy is checked in case it
// was not found in the extension registry. When an extension is unloaded, we
// clean up state from the extension. For service worker-based extensions,
// this includes unregistering the worker, which requires access to the
// storage partition. At this point, since the extension is unloaded, it won't
// be present in the registry, but we still need to determine if the extension
// has isolated storage to pinpoint the correct storage partition.
// Regression test for b/287924795.
TEST_F(ExtensionUtilWithSigninProfileUnittest,
       HasIsolatedStorageForForceInstalledExtensions) {
  scoped_refptr<const Extension> extension1 = BuildPolicyInstalledExtension();
  scoped_refptr<const Extension> extension2 = BuildPolicyInstalledExtension();
  ExtensionRegistry* extension_registry =
      ExtensionRegistry::Get(signin_profile_);
  extension_registry->AddEnabled(extension1);
  extension_registry->AddEnabled(extension2);

  // Extensions are found in the registry, are policy-installed and run on the
  // sign-in screen.
  EXPECT_TRUE(util::HasIsolatedStorage(extension1->id(), signin_profile_));
  EXPECT_TRUE(util::HasIsolatedStorage(extension2->id(), signin_profile_));

  extension_registry->RemoveEnabled(extension1->id());
  extension_registry->RemoveEnabled(extension2->id());

  // Extensions are not found in the registry and are not force-installed.
  EXPECT_FALSE(util::HasIsolatedStorage(extension1->id(), signin_profile_));
  EXPECT_FALSE(util::HasIsolatedStorage(extension2->id(), signin_profile_));

  ExtensionIdList extension_ids;
  extension_ids.push_back(extension1->id());
  extension_ids.push_back(extension2->id());
  SetupForceList(extension_ids);

  // Extensions are not found in the registry, but are force-installed and run
  // on the sign-in screen.
  EXPECT_TRUE(util::HasIsolatedStorage(extension1->id(), signin_profile_));
  EXPECT_TRUE(util::HasIsolatedStorage(extension2->id(), signin_profile_));
}

#endif

}  // namespace extensions
