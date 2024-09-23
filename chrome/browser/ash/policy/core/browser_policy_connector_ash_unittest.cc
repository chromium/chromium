// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"

#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(BrowserPolicyConnectorAshTest, UserManager) {
  ScopedTestingLocalState testing_local_state{
      TestingBrowserProcess::GetGlobal()};
  content::BrowserTaskEnvironment task_environment;
  const AccountId kAccountId =
      AccountId::FromUserEmailGaiaId("test@example.com", "1234567890");

  ash::ScopedCrosSettingsTestHelper settings_helper_;
  settings_helper_.ReplaceDeviceSettingsProviderWithStub();

  TestWallpaperController test_wallpaper_controller;
  WallpaperControllerClientImpl wallpaper_controller_client{
      std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>()};
  wallpaper_controller_client.InitForTesting(&test_wallpaper_controller);

  BrowserPolicyConnectorAsh browser_policy_connector;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager(std::make_unique<user_manager::FakeUserManager>(
          testing_local_state.Get()));
  ash::UserImageManagerRegistry user_image_manager_registry(
      fake_user_manager.Get());

  fake_user_manager->AddUser(AccountId::FromUserEmail("owner@example/com"));
  fake_user_manager->AddUser(kAccountId);

  browser_policy_connector.OnUserManagerCreated(fake_user_manager.Get());

  EXPECT_EQ(2u, fake_user_manager->GetUsers().size());
  EXPECT_EQ(0, test_wallpaper_controller.remove_user_wallpaper_count());

  fake_user_manager->RemoveUser(kAccountId,
                                user_manager::UserRemovalReason::UNKNOWN);

  EXPECT_EQ(1u, fake_user_manager->GetUsers().size());
  EXPECT_EQ(1, test_wallpaper_controller.remove_user_wallpaper_count());

  browser_policy_connector.OnUserManagerShutdown();
}

}  // namespace policy
