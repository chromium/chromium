// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/persistent_forced_extension_keep_alive.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
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
  PersistentForcedExtensionKeepAliveTest() {
    scoped_feature_list_.InitWithFeatures(
        ash::standalone_browser::GetFeatureRefs(), {});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kEnableLacrosForTesting);
  }

  PersistentForcedExtensionKeepAliveTest(
      const PersistentForcedExtensionKeepAliveTest&) = delete;
  PersistentForcedExtensionKeepAliveTest& operator=(
      const PersistentForcedExtensionKeepAliveTest&) = delete;

  ~PersistentForcedExtensionKeepAliveTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    browser_manager_ = std::make_unique<FakeBrowserManager>();

    // Login a user. The "email" must match the TestingProfile's
    // GetProfileUserName() so that profile() will be the primary profile.
    const AccountId account_id = AccountId::FromUserEmail(kUserEmail);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    CreateTestingProfile();

    ASSERT_TRUE(browser_util::IsLacrosEnabled());

    // Unset KeepAlive temporarily at the end of SetUp(), so that any KeepAlive
    // instantiated during the set up will be disabled.
    scoped_unset_all_keep_alive_ =
        std::make_unique<BrowserManager::ScopedUnsetAllKeepAliveForTesting>(
            BrowserManager::Get());
  }

  void TearDown() override {
    scoped_unset_all_keep_alive_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    browser_manager_.reset();
    profile_manager_.reset();
    fake_user_manager_.Reset();
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
  base::test::ScopedCommandLine scoped_command_line_;

  content::BrowserTaskEnvironment task_environment_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<FakeBrowserManager> browser_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  std::unique_ptr<BrowserManager::ScopedUnsetAllKeepAliveForTesting>
      scoped_unset_all_keep_alive_;

  raw_ptr<TestingProfile> profile_;
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
