// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_manager.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::component_updater::FakeCrOSComponentManager;
using ::component_updater::MockComponentUpdateService;
using testing::_;
using update_client::UpdateClient;
using user_manager::User;

namespace crosapi {

namespace {

constexpr char kSampleLacrosPath[] =
    "/run/imageloader-lacros-dogfood-dev/97.0.4676/";

class BrowserManagerFake : public BrowserManager {
 public:
  BrowserManagerFake(std::unique_ptr<BrowserLoader> browser_loader,
                     component_updater::ComponentUpdateService* update_service)
      : BrowserManager(std::move(browser_loader), update_service) {}
  ~BrowserManagerFake() override = default;

  // BrowserManager:
  void Start(
      browser_util::InitialBrowserAction initial_browser_action) override {
    ++start_count_;
    initial_browser_action_ = initial_browser_action.action;
    SetState(State::STARTING);
  }

  int start_count() const { return start_count_; }

  mojom::InitialBrowserAction initial_browser_action() const {
    return initial_browser_action_;
  }

  void SetStatePublic(State state) { SetState(state); }

  // Make the State enum publicly available.
  using BrowserManager::State;

 private:
  int start_count_ = 0;
  mojom::InitialBrowserAction initial_browser_action_;
};

}  // namespace

class MockBrowserLoader : public BrowserLoader {
 public:
  explicit MockBrowserLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager)
      : BrowserLoader(manager) {}
  MockBrowserLoader(const MockBrowserLoader&) = delete;
  MockBrowserLoader& operator=(const MockBrowserLoader&) = delete;
  ~MockBrowserLoader() override = default;

  MOCK_METHOD1(Load, void(LoadCompletionCallback));
  MOCK_METHOD0(Unload, void());
};

class BrowserManagerTest : public testing::Test {
 public:
  BrowserManagerTest() : local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~BrowserManagerTest() override = default;

  void SetUp() override {
    // Enable Lacros by setting the appropriate flag.
    feature_list_.InitAndEnableFeature(chromeos::features::kLacrosSupport);

    fake_user_manager_ = new ash::FakeChromeUserManager;
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));

    auto fake_cros_component_manager =
        base::MakeRefCounted<FakeCrOSComponentManager>();
    std::unique_ptr<MockBrowserLoader> browser_loader =
        std::make_unique<testing::StrictMock<MockBrowserLoader>>(
            fake_cros_component_manager);
    browser_loader_ = browser_loader.get();
    component_update_service_ =
        std::make_unique<testing::NiceMock<MockComponentUpdateService>>();
    fake_browser_manager_ = std::make_unique<BrowserManagerFake>(
        std::move(browser_loader), component_update_service_.get());
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, &testing_profile_);
  }

 protected:
  // The order of these members is relevant for both construction and
  // destruction timing.
  content::BrowserTaskEnvironment task_environment_;
  session_manager::SessionManager session_manager_;
  TestingProfile testing_profile_;
  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  MockBrowserLoader* browser_loader_ = nullptr;
  std::unique_ptr<MockComponentUpdateService> component_update_service_;
  std::unique_ptr<BrowserManagerFake> fake_browser_manager_;
  ScopedTestingLocalState local_state_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BrowserManagerTest, LacrosKeepAlive) {
  AddRegularUser("user@test.com");
  browser_util::SetProfileMigrationCompletedForUser(
      local_state_.Get(), ash::ProfileHelper::Get()
                              ->GetUserByProfile(&testing_profile_)
                              ->username_hash());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_TRUE(browser_util::IsLacrosAllowedToLaunch());

  using State = BrowserManagerFake::State;
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Attempt to mount the Lacros image. Will not start as it does not meet the
  // automatic start criteria.
  EXPECT_CALL(*browser_loader_, Load(_))
      .WillOnce([](BrowserLoader::LoadCompletionCallback callback) {
        std::move(callback).Run(base::FilePath("/run/lacros"),
                                browser_util::LacrosSelection::kRootfs);
      });
  fake_browser_manager_->InitializeAndStart();
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

TEST_F(BrowserManagerTest, LacrosKeepAliveReloadsWhenUpdateAvailable) {
  AddRegularUser("user@test.com");
  browser_util::SetProfileMigrationCompletedForUser(
      local_state_.Get(), ash::ProfileHelper::Get()
                              ->GetUserByProfile(&testing_profile_)
                              ->username_hash());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_TRUE(browser_util::IsLacrosAllowedToLaunch());

  EXPECT_CALL(*browser_loader_, Load(_))
      .WillOnce([](BrowserLoader::LoadCompletionCallback callback) {
        std::move(callback).Run(base::FilePath("/run/lacros"),
                                browser_util::LacrosSelection::kRootfs);
      });
  fake_browser_manager_->InitializeAndStart();

  using State = BrowserManagerFake::State;
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  fake_browser_manager_->SetStatePublic(State::UNAVAILABLE);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Simulate an update event by the component update service.
  const std::string lacros_component_id =
      browser_util::kLacrosDogfoodDevInfo.crx_id;
  static_cast<component_updater::ComponentUpdateService::Observer*>(
      fake_browser_manager_.get())
      ->OnEvent(UpdateClient::Observer::Events::COMPONENT_UPDATED,
                lacros_component_id);

  std::unique_ptr<BrowserManager::ScopedKeepAlive> keep_alive =
      fake_browser_manager_->KeepAlive(BrowserManager::Feature::kTestOnly);

  EXPECT_CALL(*browser_loader_, Load(_))
      .WillOnce([](BrowserLoader::LoadCompletionCallback callback) {
        std::move(callback).Run(base::FilePath(kSampleLacrosPath),
                                browser_util::LacrosSelection::kStateful);
      });

  // Once the state becomes STOPPED, then Lacros should start. Since there is
  // an update, it should first load the updated image.
  fake_browser_manager_->SetStatePublic(State::STOPPED);
  EXPECT_EQ(fake_browser_manager_->start_count(), 1);
}

TEST_F(BrowserManagerTest, NewWindowReloadsWhenUpdateAvailable) {
  AddRegularUser("user@test.com");
  browser_util::SetProfileMigrationCompletedForUser(
      local_state_.Get(), ash::ProfileHelper::Get()
                              ->GetUserByProfile(&testing_profile_)
                              ->username_hash());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_TRUE(browser_util::IsLacrosAllowedToLaunch());

  EXPECT_CALL(*browser_loader_, Load(_))
      .WillOnce([](BrowserLoader::LoadCompletionCallback callback) {
        std::move(callback).Run(base::FilePath("/run/lacros"),
                                browser_util::LacrosSelection::kRootfs);
      });
  fake_browser_manager_->InitializeAndStart();

  // Set the state of the browser manager as stopped, which would match the
  // state after the browser mounted an image, ran, and was terminated.
  using State = BrowserManagerFake::State;
  fake_browser_manager_->SetStatePublic(State::STOPPED);

  const std::string lacros_component_id =
      browser_util::kLacrosDogfoodDevInfo.crx_id;
  static_cast<component_updater::ComponentUpdateService::Observer*>(
      fake_browser_manager_.get())
      ->OnEvent(UpdateClient::Observer::Events::COMPONENT_UPDATED,
                lacros_component_id);

  EXPECT_CALL(*browser_loader_, Load(_))
      .WillOnce([](BrowserLoader::LoadCompletionCallback callback) {
        std::move(callback).Run(base::FilePath(kSampleLacrosPath),
                                browser_util::LacrosSelection::kStateful);
      });
  fake_browser_manager_->NewWindow(/*incongnito=*/false,
                                   /*should_trigger_session_restore=*/false);
}

TEST_F(BrowserManagerTest, LacrosKeepAliveDoesNotBlockRestart) {
  AddRegularUser("user@test.com");
  browser_util::SetProfileMigrationCompletedForUser(
      local_state_.Get(), ash::ProfileHelper::Get()
                              ->GetUserByProfile(&testing_profile_)
                              ->username_hash());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_TRUE(browser_util::IsLacrosAllowedToLaunch());

  using State = BrowserManagerFake::State;
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Attempt to mount the Lacros image. Will not start as it does not meet the
  // automatic start criteria.
  EXPECT_CALL(*browser_loader_, Load(_))
      .WillOnce([](BrowserLoader::LoadCompletionCallback callback) {
        std::move(callback).Run(base::FilePath("/run/lacros"),
                                browser_util::LacrosSelection::kRootfs);
      });
  fake_browser_manager_->InitializeAndStart();
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  fake_browser_manager_->SetStatePublic(State::UNAVAILABLE);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Creating a ScopedKeepAlive does not start Lacros.
  std::unique_ptr<BrowserManager::ScopedKeepAlive> keep_alive =
      fake_browser_manager_->KeepAlive(BrowserManager::Feature::kTestOnly);
  EXPECT_EQ(fake_browser_manager_->start_count(), 0);

  // Simulate a Lacros termination, keep alive should launch Lacros in a
  // windowless state.
  auto simulate_lacros_termination = [&]() {
    fake_browser_manager_->SetStatePublic(State::TERMINATING);
    fake_browser_manager_->OnLacrosChromeTerminated();
  };
  simulate_lacros_termination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 1);
  EXPECT_EQ(fake_browser_manager_->initial_browser_action(),
            mojom::InitialBrowserAction::kDoNotOpenWindow);

  // Terminating again causes keep alive to again start Lacros in a windowless
  // state.
  simulate_lacros_termination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 2);
  EXPECT_EQ(fake_browser_manager_->initial_browser_action(),
            mojom::InitialBrowserAction::kDoNotOpenWindow);

  // Request a relaunch. Keep alive should not start Lacros in a windowless
  // state but Lacros should instead start with the kRestoreLastSession action.
  fake_browser_manager_->set_relaunch_requested_for_testing(true);
  simulate_lacros_termination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 3);
  EXPECT_EQ(fake_browser_manager_->initial_browser_action(),
            mojom::InitialBrowserAction::kRestoreLastSession);

  // Resetting the relaunch requested bit should cause keep alive to start
  // Lacros in a windowless state.
  fake_browser_manager_->set_relaunch_requested_for_testing(false);
  simulate_lacros_termination();
  EXPECT_EQ(fake_browser_manager_->start_count(), 4);
  EXPECT_EQ(fake_browser_manager_->initial_browser_action(),
            mojom::InitialBrowserAction::kDoNotOpenWindow);
}

}  // namespace crosapi
