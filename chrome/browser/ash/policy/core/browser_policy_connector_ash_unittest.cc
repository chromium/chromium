// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"

#include "base/check_deref.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/ui/ash/wallpaper/test_wallpaper_controller.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

TEST(BrowserPolicyConnectorAshTest, UserManager) {
  content::BrowserTaskEnvironment task_environment;
  constexpr auto kAccountId = AccountId::Literal::FromUserEmailGaiaId(
      "test@example.com", GaiaId::Literal("1234567890"));

  ash::ScopedCrosSettingsTestHelper settings_helper_;
  settings_helper_.ReplaceDeviceSettingsProviderWithStub();

  TestWallpaperController test_wallpaper_controller;
  WallpaperControllerClientImpl wallpaper_controller_client{
      CHECK_DEREF(TestingBrowserProcess::GetGlobal()->local_state()),
      std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>()};
  wallpaper_controller_client.InitForTesting(&test_wallpaper_controller);

  BrowserPolicyConnectorAsh browser_policy_connector;
  user_manager::ScopedUserManager user_manager(
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<user_manager::FakeUserManagerDelegate>(),
          TestingBrowserProcess::GetGlobal()->local_state()));
  ash::UserImageManagerRegistry user_image_manager_registry(user_manager.Get());

  {
    user_manager::TestHelper test_helper(user_manager.Get());
    constexpr auto kOwnerAccountId = AccountId::Literal::FromUserEmailGaiaId(
        "owner@example.com", GaiaId::Literal("ownergaia"));
    ASSERT_TRUE(test_helper.AddRegularUser(kOwnerAccountId));
    ASSERT_TRUE(test_helper.AddRegularUser(kAccountId));

    user_manager->SetOwnerId(kOwnerAccountId);
  }

  browser_policy_connector.OnUserManagerCreated(user_manager.Get());

  EXPECT_EQ(2u, user_manager->GetPersistedUsers().size());
  EXPECT_EQ(0, test_wallpaper_controller.remove_user_wallpaper_count());

  user_manager->RemoveUser(kAccountId,
                           user_manager::UserRemovalReason::UNKNOWN);

  EXPECT_EQ(1u, user_manager->GetPersistedUsers().size());
  EXPECT_EQ(1, test_wallpaper_controller.remove_user_wallpaper_count());

  browser_policy_connector.OnUserManagerShutdown();
}

}  // namespace policy
