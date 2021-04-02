// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using user_manager::User;

namespace crosapi {

namespace {
class BrowserManagerFake : public BrowserManager {
 public:
  BrowserManagerFake() : BrowserManager(nullptr) {}
  ~BrowserManagerFake() override = default;
  void Start(mojom::InitialBrowserAction initial_browser_action) override {
    ++start_count_;
    SetState(State::STARTING);
  }
  int start_count() { return start_count_; }
  void SetStatePublic(State state) { SetState(state); }

  // Make the State enum publicly available.
  using BrowserManager::State;

 private:
  int start_count_ = 0;
};
}  // namespace

class BrowserManagerTest : public testing::Test {
 public:
  BrowserManagerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~BrowserManagerTest() override = default;

  void SetUp() override {
    fake_user_manager_ = new ash::FakeChromeUserManager;
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));
    fake_browser_manager_ = std::make_unique<BrowserManagerFake>();
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, &testing_profile_);
  }

  // The order of these members is relevant for both construction and
  // destruction timing.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<BrowserManagerFake> fake_browser_manager_;

  ScopedTestingLocalState local_state_;
};

TEST_F(BrowserManagerTest, LacrosKeepAlive) {
  // Enable Lacros by creating a regular user, and setting the appropriate
  // flag.
  AddRegularUser("user@test.com");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kLacrosSupport);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_TRUE(browser_util::IsLacrosAllowedToLaunch());

  using State = BrowserManagerFake::State;
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  fake_browser_manager_->SetStatePublic(State::UNAVAILABLE);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Creating a ScopedKeepAlive does not start Lacros.
  std::unique_ptr<BrowserManager::ScopedKeepAlive> keep_alive =
      fake_browser_manager_->KeepAlive(BrowserManager::Feature::kTestOnly);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Once the state becomes STOPPED, then Lacros should start.
  fake_browser_manager_->SetStatePublic(State::STOPPED);
  EXPECT_EQ(fake_browser_manager_->start_count(), 1);

  // Repeating the process starts Lacros again.
  fake_browser_manager_->SetStatePublic(State::STOPPED);
  EXPECT_EQ(fake_browser_manager_->start_count(), 2);

  // Once the ScopedKeepAlive is destroyed, this should no longer happen.
  keep_alive.reset();
  fake_browser_manager_->SetStatePublic(State::STOPPED);
  EXPECT_EQ(fake_browser_manager_->start_count(), 2);
}

}  // namespace crosapi
