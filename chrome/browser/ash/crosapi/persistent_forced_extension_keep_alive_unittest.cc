// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/persistent_forced_extension_keep_alive.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAllowedExtensionId[] = "baobpecgllpajfeojepgedjdlnlfffde";
const char kNotAllowedExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
constexpr char kUserEmail[] = "user@email.com";

}  // namespace

namespace crosapi {

class PersistentForcedExtensionKeepAliveTest : public testing::Test {
 public:
  PersistentForcedExtensionKeepAliveTest()
      : browser_manager_(std::make_unique<FakeBrowserManager>()) {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
         ash::features::kLacrosOnly,
         ash::features::kLacrosProfileMigrationForceOff},
        {});
  }

  PersistentForcedExtensionKeepAliveTest(
      const PersistentForcedExtensionKeepAliveTest&) = delete;
  PersistentForcedExtensionKeepAliveTest& operator=(
      const PersistentForcedExtensionKeepAliveTest&) = delete;

  ~PersistentForcedExtensionKeepAliveTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());

    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    auto* fake_user_manager = user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));

    ASSERT_TRUE(profile_manager_->SetUp());

    // Login a user. The "email" must match the TestingProfile's
    // GetProfileUserName() so that profile() will be the primary profile.
    const AccountId account_id = AccountId::FromUserEmail(kUserEmail);
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);

    CreateTestingProfile();

    ASSERT_TRUE(browser_util::IsLacrosEnabled());

    // Unset KeepAlive temporarily at the end of SetUp(), so that any KeepAlive
    // instantiated during the set up will be disabled.
    scoped_unset_all_keep_alive_ =
        std::make_unique<BrowserManager::ScopedUnsetAllKeepAliveForTesting>(
            BrowserManager::Get());
  }

  void TearDown() override {
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    scoped_user_manager_.reset();
  }

  void CreateTestingProfile() {
    profile_ = profile_manager_->CreateTestingProfile(kUserEmail);
  }

  void SetInstallForceList(const std::string& extension_id) {
    profile_->GetPrefs()->Set(extensions::pref_names::kInstallForceList,
                              base::Value(base::Value::Dict().Set(
                                  extension_id, base::Value::Dict())));
  }

  FakeBrowserManager& browser_manager() { return *browser_manager_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<FakeBrowserManager> browser_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::unique_ptr<BrowserManager::ScopedUnsetAllKeepAliveForTesting>
      scoped_unset_all_keep_alive_;

  raw_ptr<TestingProfile, ExperimentalAsh> profile_;
};

// Test that KeepAlive is registered on session start if an extension that
// requires Lacros to be running is force-installed.
TEST_F(PersistentForcedExtensionKeepAliveTest, StartKeepAliveIfAllowlisted) {
  SetInstallForceList(kAllowedExtensionId);
  EXPECT_TRUE(browser_manager().IsKeepAliveEnabled());
}

// Test that KeepAlive is not registered if no forced extension requires Lacros
// to be kept alive.
TEST_F(PersistentForcedExtensionKeepAliveTest,
       DontStartKeepAliveIfExtensionNotAllowlisted) {
  SetInstallForceList(kNotAllowedExtensionId);
  EXPECT_FALSE(browser_manager().IsKeepAliveEnabled());
}

// Test that KeepAlive is not registered if there are no force-installed
// extensions.
TEST_F(PersistentForcedExtensionKeepAliveTest, DontStartKeepAliveIfUnset) {
  EXPECT_FALSE(browser_manager().IsKeepAliveEnabled());
}

// Test that KeepAlive reacts to pref value changes.
TEST_F(PersistentForcedExtensionKeepAliveTest,
       UpdateKeepAliveOnPrefValueChanges) {
  SetInstallForceList(kAllowedExtensionId);
  EXPECT_TRUE(browser_manager().IsKeepAliveEnabled());

  SetInstallForceList(kNotAllowedExtensionId);
  EXPECT_FALSE(browser_manager().IsKeepAliveEnabled());
}

}  // namespace crosapi
