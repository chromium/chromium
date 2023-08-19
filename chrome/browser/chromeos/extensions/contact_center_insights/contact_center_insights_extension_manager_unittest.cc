// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/contact_center_insights/contact_center_insights_extension_manager.h"

#include <memory>

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

constexpr char kAffiliatedUserId[] = "123";
constexpr char kUnaffiliatedUserId[] = "456";

// Test delegate for `ContactCenterInsightsExtensionManager` that stubs out the
// component extension installs/uninstalls and profile affiliation.
class TestDelegate : public ContactCenterInsightsExtensionManager::Delegate {
 public:
  TestDelegate() { extension_installed_.store(false); }

  ~TestDelegate() override = default;

  void InstallExtension(ComponentLoader* component_loader) override {
    extension_installed_.store(true);
  }

  void UninstallExtension(ComponentLoader* component_loader) override {
    extension_installed_.store(false);
  }

  bool IsProfileAffiliated(Profile* profile) const override {
    return profile->GetProfileUserName() == kAffiliatedUserId;
  }

  bool IsExtensionInstalled(ComponentLoader* component_loader) const override {
    return extension_installed_.load();
  }

 private:
  std::atomic<bool> extension_installed_;
};

}  // namespace

class ContactCenterInsightsExtensionManagerTest : public ::testing::Test {
 protected:
  ContactCenterInsightsExtensionManagerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    affiliated_user_profile_ =
        profile_manager_.CreateTestingProfile(kAffiliatedUserId);
    unaffiliated_user_profile_ =
        profile_manager_.CreateTestingProfile(kUnaffiliatedUserId);
  }

  void TearDown() override { profile_manager_.DeleteAllTestingProfiles(); }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;

  raw_ptr<TestingProfile, DanglingUntriaged> affiliated_user_profile_;
  raw_ptr<TestingProfile, DanglingUntriaged> unaffiliated_user_profile_;
};

TEST_F(ContactCenterInsightsExtensionManagerTest,
       ExtensionNotInstalledOnInitWhenPrefNotSet) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  ContactCenterInsightsExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(ContactCenterInsightsExtensionManagerTest,
       EnableExtensionOnInitWhenPrefSet) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  // Set pref before initializing extension manager.
  affiliated_user_profile_->GetPrefs()->SetBoolean(
      ::prefs::kInsightsExtensionEnabled, true);

  ContactCenterInsightsExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(extension_manager.CanInstallExtension());
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(ContactCenterInsightsExtensionManagerTest,
       DisableExtensionOnInitWhenPrefNotSet) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;

  // Install extension initially.
  delegate_raw_ptr->InstallExtension(component_loader);
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  ContactCenterInsightsExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(ContactCenterInsightsExtensionManagerTest, EnableExtensionWhenPrefSet) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  ContactCenterInsightsExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  // Set pref.
  affiliated_user_profile_->GetPrefs()->SetBoolean(
      ::prefs::kInsightsExtensionEnabled, true);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(extension_manager.CanInstallExtension());
  EXPECT_TRUE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(ContactCenterInsightsExtensionManagerTest,
       DisableExtensionWhenPrefUnset) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  ContactCenterInsightsExtensionManager extension_manager(
      component_loader, affiliated_user_profile_, std::move(delegate));

  // Install extension initially.
  delegate_raw_ptr->InstallExtension(component_loader);

  // Unset pref and ensure the manager uninstalls the extension.
  affiliated_user_profile_->GetPrefs()->SetBoolean(
      ::prefs::kInsightsExtensionEnabled, false);
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

TEST_F(ContactCenterInsightsExtensionManagerTest,
       DisableExtensionForUnaffiliatedUser) {
  auto delegate = std::make_unique<TestDelegate>();
  auto* delegate_raw_ptr = delegate.get();
  ComponentLoader* component_loader = nullptr;
  ASSERT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));

  unaffiliated_user_profile_->GetPrefs()->SetBoolean(
      ::prefs::kInsightsExtensionEnabled, true);

  ContactCenterInsightsExtensionManager extension_manager(
      component_loader, unaffiliated_user_profile_, std::move(delegate));
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(extension_manager.CanInstallExtension());
  EXPECT_FALSE(delegate_raw_ptr->IsExtensionInstalled(component_loader));
}

}  // namespace chromeos
