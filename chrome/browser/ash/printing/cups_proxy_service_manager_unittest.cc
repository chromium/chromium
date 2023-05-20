// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/cups_proxy_service_manager.h"

#include <memory>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/services/cups_proxy/cups_proxy_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/cups_proxy/cups_proxy_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kProfileName[] = "user@example.com";

}  // namespace

class CupsProxyServiceManagerTest : public testing::Test {
 protected:
  CupsProxyServiceManagerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  CupsProxyServiceManagerTest(const CupsProxyServiceManagerTest&) = delete;
  CupsProxyServiceManagerTest& operator=(const CupsProxyServiceManagerTest&) =
      delete;
  ~CupsProxyServiceManagerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager_ = fake_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    CupsProxyClient::InitializeFake();
  }

  void TearDown() override { CupsProxyClient::Shutdown(); }

  void CreatePrimaryProfile() {
    AccountId account_id = AccountId::FromUserEmail(kProfileName);
    fake_user_manager_->AddUser(account_id);
    user_manager::UserManager::Get()->UserLoggedIn(
        account_id,
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id),
        /*browser_restart=*/false,
        /*is_child=*/false);
    testing_profile_manager_.CreateTestingProfile(kProfileName,
                                                  /*is_main_profile=*/true);
  }

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

  base::test::ScopedFeatureList* scoped_feature_list() {
    return &scoped_feature_list_;
  }

  user_manager::FakeUserManager* fake_user_manager() {
    return fake_user_manager_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager testing_profile_manager_;
  // Owned by `scoped_user_manager_`.
  user_manager::FakeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(CupsProxyServiceManagerTest, FeatureNotEnabled) {
  scoped_feature_list()->InitAndDisableFeature(features::kPluginVm);

  CupsProxyServiceManager manager;

  EXPECT_EQ(nullptr, cups_proxy::CupsProxyService::GetInstance());
}

TEST_F(CupsProxyServiceManagerTest, PrimaryProfileAlreadyCreated) {
  scoped_feature_list()->InitAndEnableFeature(features::kPluginVm);
  CreatePrimaryProfile();

  CupsProxyServiceManager manager;

  task_environment()->RunUntilIdle();

  EXPECT_NE(nullptr, cups_proxy::CupsProxyService::GetInstance());
}

TEST_F(CupsProxyServiceManagerTest, PrimaryProfileCreatedLater) {
  scoped_feature_list()->InitAndEnableFeature(features::kPluginVm);

  // Before the primary profile has been created, we don't expect
  // CupsProxyService to have been spawned.
  CupsProxyServiceManager manager;

  task_environment()->RunUntilIdle();

  EXPECT_EQ(nullptr, cups_proxy::CupsProxyService::GetInstance());

  CreatePrimaryProfile();

  task_environment()->RunUntilIdle();

  EXPECT_NE(nullptr, cups_proxy::CupsProxyService::GetInstance());
}

}  // namespace ash
