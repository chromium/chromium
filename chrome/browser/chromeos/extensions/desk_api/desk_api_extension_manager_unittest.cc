// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/desk_api/desk_api_extension_manager.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::extensions::ComponentLoader;

namespace chromeos {
namespace {

inline constexpr char kAffiliatedUserId[] = "affiliated_user";
inline constexpr char kUnaffiliatedUserId[] = "unaffiliated_user";
inline constexpr char kDummyManifest[] = "";
inline constexpr char kExternallyConnectableKey[] = "externally_connectable";
inline constexpr char kMatchesKey[] = "matches";

std::unique_ptr<base::Value> ParseManifest(std::string manifest) {
  std::string error_message;
  int error_code;
  JSONStringValueDeserializer deserializer(manifest);
  return deserializer.Deserialize(&error_code, &error_message);
}

const base::Value::List* GetMatchesListFromManifest(
    const base::Value* manifest_value) {
  const base::Value::Dict* manifest_dict = manifest_value->GetIfDict();

  if (!manifest_dict) {
    ADD_FAILURE() << "No manifest dict";
    return nullptr;
  }

  const base::Value::Dict* externally_connectable_dict =
      manifest_dict->FindDict(kExternallyConnectableKey);

  if (!externally_connectable_dict) {
    ADD_FAILURE() << "No externally_connectable dict";
    return nullptr;
  }

  return externally_connectable_dict->FindList(kMatchesKey);
}

// Test delegate for `DeskApiExtensionManager` that stubs out the
// component extension installs/uninstalls and profile affiliation.
class TestDelegate : public DeskApiExtensionManager::Delegate {
 public:
  TestDelegate() { extension_installed_.store(false); }

  ~TestDelegate() override = default;

  void InstallExtension(ComponentLoader* component_loader,
                        const std::string& manifest_content) override {
    extension_installed_.store(true);
    manifest_value_ = ParseManifest(manifest_content);
  }

  void UninstallExtension(ComponentLoader* component_loader) override {
    extension_installed_.store(false);
    manifest_value_.reset();
  }

  bool IsProfileAffiliated(Profile* profile) const override {
    return profile->GetProfileUserName() == kAffiliatedUserId;
  }

  bool IsExtensionInstalled(ComponentLoader* component_loader) const override {
    return extension_installed_.load();
  }

  const base::Value* GetInstalledManifest() const {
    return manifest_value_.get();
  }

 private:
  std::atomic<bool> extension_installed_;
  std::unique_ptr<base::Value> manifest_value_;
};

void SetDeskAPIPolicies(PrefService* pref_service,
                        bool enabled,
                        const base::Value::List& allowlist) {
  pref_service->SetBoolean(::prefs::kDeskAPIThirdPartyAccessEnabled, enabled);
  pref_service->SetList(::prefs::kDeskAPIThirdPartyAllowlist,
                        allowlist.Clone());
}

void EnableDeskAPI(PrefService* pref_service) {
  // Create an arbitrary allowlist.
  auto allowlist = base::Value::List().Append("http://*.domain1.com/*");
  SetDeskAPIPolicies(pref_service, true, allowlist);
}

void DisableDeskAPI(PrefService* pref_service) {
  base::Value::List allowlist;
  SetDeskAPIPolicies(pref_service, false, allowlist);
}

}  // namespace

class DeskApiExtensionManagerTest : public ::testing::Test {
 protected:
  DeskApiExtensionManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    affiliated_user_profile_ =
        profile_manager_.CreateTestingProfile(kAffiliatedUserId);
    incognito_profile_ =
        TestingProfile::Builder().BuildIncognito(affiliated_user_profile_);

    unaffiliated_user_profile_ =
        profile_manager_.CreateTestingProfile(kUnaffiliatedUserId);
  }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;

  raw_ptr<TestingProfile, DanglingUntriaged> affiliated_user_profile_;
  raw_ptr<TestingProfile, DanglingUntriaged> incognito_profile_;
  raw_ptr<TestingProfile, DanglingUntriaged> unaffiliated_user_profile_;
};

TEST_F(DeskApiExtensionManagerTest, ExtensionNotInstalledOnInitWhenPrefNotSet) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  DeskApiExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(DeskApiExtensionManagerTest, EnableExtensionOnInitWhenPrefSet) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  // Set pref before initializing extension manager.
  EnableDeskAPI(affiliated_user_profile_->GetPrefs());

  DeskApiExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(extension_manager.CanInstallExtension());
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(DeskApiExtensionManagerTest, DisableExtensionForIncognitoProfile) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  // Set pref before initializing extension manager.
  EnableDeskAPI(affiliated_user_profile_->GetPrefs());
  task_environment_.RunUntilIdle();

  DeskApiExtensionManager extension_manager(
      component_loader, incognito_profile_, std::move(delegate));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(DeskApiExtensionManagerTest, DisableExtensionOnInitWhenPrefNotSet) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;

  // Install extension initially.
  delegate_raw_ptr->InstallExtension(component_loader, kDummyManifest);
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  DeskApiExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(DeskApiExtensionManagerTest, EnableExtensionWhenPrefSet) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  DeskApiExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  // Set pref.
  EnableDeskAPI(affiliated_user_profile_->GetPrefs());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(extension_manager.CanInstallExtension());
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(DeskApiExtensionManagerTest, DisableExtensionWhenPrefUnset) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  DeskApiExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  // Install extension initially.
  delegate_raw_ptr->InstallExtension(component_loader, kDummyManifest);
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  // Unset pref and ensure the manager uninstalls the extension.
  DisableDeskAPI(affiliated_user_profile_->GetPrefs());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(DeskApiExtensionManagerTest, DoNotInstallExtensionWithEmptyAllowlist) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  DeskApiExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  base::Value::List empty_allowlist;
  SetDeskAPIPolicies(affiliated_user_profile_->GetPrefs(), true,
                     empty_allowlist);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(DeskApiExtensionManagerTest, DisableExtensionForUnaffiliatedUser) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  EnableDeskAPI(unaffiliated_user_profile_->GetPrefs());

  DeskApiExtensionManager extension_manager(
      component_loader, unaffiliated_user_profile_, std::move(delegate));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(DeskApiExtensionManagerTest, GenerateManifestFromPolicyAllowlist) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  DeskApiExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  constexpr char test_domain1[] = "http://*.domain1.com/*";
  constexpr char test_domain2[] = "http://*.domain2.com/*";

  auto domain_allowlist =
      base::Value::List().Append(test_domain1).Append(test_domain2);

  SetDeskAPIPolicies(affiliated_user_profile_->GetPrefs(), true,
                     domain_allowlist);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(extension_manager.CanInstallExtension());
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  const base::Value* installed_manifest_value =
      delegate_raw_ptr->GetInstalledManifest();
  ASSERT_TRUE(installed_manifest_value);
  const base::Value::List* installed_matches_list =
      GetMatchesListFromManifest(installed_manifest_value);
  ASSERT_TRUE(installed_matches_list);
  EXPECT_EQ(domain_allowlist, *installed_matches_list);
}

TEST_F(DeskApiExtensionManagerTest, GenerateManifestIgnoresInvalidURLPattern) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  DeskApiExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  constexpr char test_domain1[] = "http://*.domain1.com/*";
  constexpr char test_domain2[] = "\"Invalid URL Pattern\"";

  auto domain_allowlist =
      base::Value::List().Append(test_domain1).Append(test_domain2);

  SetDeskAPIPolicies(affiliated_user_profile_->GetPrefs(), true,
                     domain_allowlist);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(extension_manager.CanInstallExtension());
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  const base::Value* installed_manifest_value =
      delegate_raw_ptr->GetInstalledManifest();
  ASSERT_TRUE(installed_manifest_value);
  const base::Value::List* installed_matches_list =
      GetMatchesListFromManifest(installed_manifest_value);
  ASSERT_TRUE(installed_matches_list);
  EXPECT_EQ(1ul, installed_matches_list->size());
  EXPECT_EQ(test_domain1, (*installed_matches_list)[0].GetString());
}

}  // namespace chromeos
