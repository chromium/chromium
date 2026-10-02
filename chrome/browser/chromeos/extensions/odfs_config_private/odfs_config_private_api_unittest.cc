// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/odfs_config_private/odfs_config_private_api.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/check_deref.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace extensions {

namespace {

base::ListValue ToList(const std::vector<std::string>& values) {
  base::ListValue list;
  for (const auto& value : values) {
    list.Append(value);
  }
  return list;
}

}  // namespace

class OfdsConfigPrivateApiUnittest : public ExtensionApiUnittest {
 public:
  OfdsConfigPrivateApiUnittest() = default;
  OfdsConfigPrivateApiUnittest(const OfdsConfigPrivateApiUnittest&) = delete;
  OfdsConfigPrivateApiUnittest& operator=(const OfdsConfigPrivateApiUnittest&) =
      delete;
  ~OfdsConfigPrivateApiUnittest() override = default;

 protected:
  void SetOneDriveMount(Profile* profile, const std::string& mount) {
    ASSERT_TRUE(profile);
    profile->GetPrefs()->SetString(ash::prefs::kMicrosoftOneDriveMount, mount);
  }

  void SetOneDriveAccountRestrictions(
      Profile* profile,
      const std::vector<std::string>& restrictions) {
    ASSERT_TRUE(profile);
    profile->GetPrefs()->SetList(
        ash::prefs::kMicrosoftOneDriveAccountRestrictions,
        ToList(restrictions));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OfdsConfigPrivateApiUnittest, GetMountSuccessful) {
  struct {
    std::string policy_value;
    extensions::api::odfs_config_private::Mount expected_mode;
  } test_cases[] = {
      {"allowed", extensions::api::odfs_config_private::Mount::kAllowed},
      {"disallowed", extensions::api::odfs_config_private::Mount::kDisallowed},
      {"automated", extensions::api::odfs_config_private::Mount::kAutomated},
  };

  for (const auto& test_case : test_cases) {
    SetOneDriveMount(profile(), test_case.policy_value);
    auto function =
        base::MakeRefCounted<extensions::OdfsConfigPrivateGetMountFunction>();
    auto returned_mount_info_value =
        RunFunctionAndReturnValue(function.get(), /*args=*/"[]");

    ASSERT_TRUE(returned_mount_info_value);
    std::optional<extensions::api::odfs_config_private::MountInfo>
        returned_mount_info =
            extensions::api::odfs_config_private::MountInfo::FromValue(
                *returned_mount_info_value);

    ASSERT_TRUE(returned_mount_info.has_value());
    extensions::api::odfs_config_private::Mount returned_mode =
        returned_mount_info->mode;
    EXPECT_EQ(returned_mode, test_case.expected_mode);
  }
}

TEST_F(OfdsConfigPrivateApiUnittest, GetAccountRestrictionsSuccessful) {
  struct {
    std::vector<std::string> restrictions;
  } test_cases[] = {
      {{"common"}},
      {{"organizations"}},
      {{"https://www.google.com", "abcd1234-1234-1234-1234-1234abcd1234"}},
  };

  for (const auto& test_case : test_cases) {
    SetOneDriveAccountRestrictions(profile(), test_case.restrictions);
    auto function = base::MakeRefCounted<
        extensions::OdfsConfigPrivateGetAccountRestrictionsFunction>();
    auto returned_restrictions_value =
        RunFunctionAndReturnValue(function.get(), /*args=*/"[]");

    ASSERT_TRUE(returned_restrictions_value);
    std::optional<extensions::api::odfs_config_private::AccountRestrictionsInfo>
        returned_account_restrictions = extensions::api::odfs_config_private::
            AccountRestrictionsInfo::FromValue(*returned_restrictions_value);

    ASSERT_TRUE(returned_account_restrictions.has_value());
    std::vector<std::string> returned_restrictions =
        returned_account_restrictions->restrictions;
    EXPECT_THAT(returned_restrictions,
                testing::ElementsAreArray(test_case.restrictions));
  }
}

class OfdsConfigPrivateNotificationApiUnittest
    : public OfdsConfigPrivateApiUnittest {
 public:
  void SetUp() override {
    OfdsConfigPrivateApiUnittest::SetUp();
    message_center::MessageCenter::Initialize();

    // The notification is scoped to the user who owns the profile, so log in
    // a user and associate it with the profile.
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId("user@example.com", GaiaId("fakegaia"));
    auto* user_manager = user_manager::UserManager::Get();
    ASSERT_TRUE(
        user_manager::TestHelper(user_manager).AddRegularUser(account_id));
    user_manager->UserLoggedIn(
        account_id,
        ash::BrowserContextHelper::GetUserIdHashFromBrowserContext(profile()));
    ash::AnnotatedAccountId::Set(profile(), account_id);
  }

  void TearDown() override {
    message_center::MessageCenter::Shutdown();
    OfdsConfigPrivateApiUnittest::TearDown();
  }
};

TEST_F(OfdsConfigPrivateNotificationApiUnittest,
       ShowAutomatedMountErrorNotificationIsShown) {
  auto function = base::MakeRefCounted<
      extensions::OdfsConfigPrivateShowAutomatedMountErrorFunction>();
  RunFunction(function.get(), /*args=*/"[]");
  const user_manager::User& user = CHECK_DEREF(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile()));
  const message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          ash::CreateUserScopedNotificationId(
              "automated_mount_error_notification_id", user.username_hash()));
  ASSERT_TRUE(notification);
  EXPECT_EQ(user.GetAccountId().GetUserEmail(),
            notification->notifier_id().profile_id);
  EXPECT_EQ(u"OneDrive setup failed", notification->title());
  EXPECT_EQ(
      u"Your administrator configured your account to be connected to "
      u"Microsoft OneDrive automatically, but something went wrong.",
      notification->message());
}

TEST_F(OfdsConfigPrivateApiUnittest, IsCloudFileSystemEnabled_Enabled) {
  scoped_feature_list_.InitAndEnableFeature(
      chromeos::features::kFileSystemProviderCloudFileSystem);
  auto function = base::MakeRefCounted<
      extensions::OdfsConfigPrivateIsCloudFileSystemEnabledFunction>();
  auto returned_is_file_system_provider_cloud_file_system_enabled_value =
      RunFunctionAndReturnValue(function.get(), /*args=*/"[]");
  ASSERT_TRUE(returned_is_file_system_provider_cloud_file_system_enabled_value
                  .has_value());

  ASSERT_TRUE(returned_is_file_system_provider_cloud_file_system_enabled_value
                  ->GetBool());
}

TEST_F(OfdsConfigPrivateApiUnittest, IsCloudFileSystemEnabled_Disabled) {
  scoped_feature_list_.InitAndDisableFeature(
      chromeos::features::kFileSystemProviderCloudFileSystem);
  auto function = base::MakeRefCounted<
      extensions::OdfsConfigPrivateIsCloudFileSystemEnabledFunction>();
  auto returned_is_file_system_provider_cloud_file_system_enabled_value =
      RunFunctionAndReturnValue(function.get(), /*args=*/"[]");
  ASSERT_TRUE(returned_is_file_system_provider_cloud_file_system_enabled_value
                  .has_value());

  ASSERT_FALSE(returned_is_file_system_provider_cloud_file_system_enabled_value
                   ->GetBool());
}

TEST_F(OfdsConfigPrivateApiUnittest, IsContentCacheEnabled_Enabled) {
  scoped_feature_list_.InitWithFeatures(
      {chromeos::features::kFileSystemProviderCloudFileSystem,
       chromeos::features::kFileSystemProviderContentCache},
      {});
  auto function = base::MakeRefCounted<
      extensions::OdfsConfigPrivateIsCloudFileSystemEnabledFunction>();
  auto returned_is_file_system_provider_cloud_file_system_enabled_value =
      RunFunctionAndReturnValue(function.get(), /*args=*/"[]");
  ASSERT_TRUE(returned_is_file_system_provider_cloud_file_system_enabled_value
                  .has_value());

  EXPECT_TRUE(returned_is_file_system_provider_cloud_file_system_enabled_value
                  ->GetBool());
}

TEST_F(OfdsConfigPrivateApiUnittest, IsContentCacheEnabled_Disabled) {
  scoped_feature_list_.InitWithFeatures(
      {}, {chromeos::features::kFileSystemProviderCloudFileSystem,
           chromeos::features::kFileSystemProviderContentCache});
  auto function = base::MakeRefCounted<
      extensions::OdfsConfigPrivateIsCloudFileSystemEnabledFunction>();
  auto returned_is_file_system_provider_content_cache_enabled_value =
      RunFunctionAndReturnValue(function.get(), /*args=*/"[]");
  ASSERT_TRUE(
      returned_is_file_system_provider_content_cache_enabled_value.has_value());

  ASSERT_FALSE(
      returned_is_file_system_provider_content_cache_enabled_value->GetBool());
}

}  // namespace extensions
