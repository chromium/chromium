// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/companion_app/companion_app_broker_impl.h"

#include <map>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/fake_quick_pair_browser_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const std::string kUserEmail = "test@test.test";
constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kValidModelId[] = "6EDAF7";
constexpr char kInvalidModelId[] = "000000";
constexpr char kFeatureParamModelId[] = "AAAAAA";
constexpr char kValidCompanionBrowserUri[] = "https://photos.google.com/";
constexpr char kEmptyCompanionBrowserUri[] = "";
constexpr char kCompanionAppId[] = "ncmjhecbjeaamljdfahankockkkdmedg";
constexpr char kValidCompanionPlayStoreUri[] =
    "https://play.google.com/store/apps/"
    "details?id=com.google.android.apps.photos";
constexpr char kEmptyCompanionPlayStoreUri[] = "";
constexpr char kFeatureParamDeviceIds[] = "111111,AAAAAA,BBBBBB,CCCCCC,DDDDDD";

}  // namespace

namespace ash {
namespace quick_pair {

class CompanionAppBrokerImplUnitTest : public AshTestBase,
                                       public CompanionAppBroker::Observer {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    companion_app_broker_ = std::make_unique<CompanionAppBrokerImpl>();
    companion_app_broker_->AddObserver(this);

    test_device_ = base::MakeRefCounted<Device>(
        kValidModelId, kTestDeviceAddress, Protocol::kFastPairInitial);

    identity_test_environment_ =
        std::make_unique<signin::IdentityTestEnvironment>();
    identity_manager_ = identity_test_environment_->identity_manager();
  }

  void TearDown() override {
    ClearLogin();
    AshTestBase::TearDown();
  }

  void Login(user_manager::UserType user_type) {
    SimulateUserLogin(kUserEmail, user_type);
  }

  void SetIdentityManager(signin::IdentityManager* identity_manager) {
    FakeQuickPairBrowserDelegate* delegate =
        FakeQuickPairBrowserDelegate::Get();
    delegate->SetIdentityManager(identity_manager);
  }

  void SetCompanionAppInstalled(const std::string& app_id, bool installed) {
    FakeQuickPairBrowserDelegate* delegate =
        FakeQuickPairBrowserDelegate::Get();
    delegate->SetCompanionAppInstalled(app_id, installed);
  }

  // CompanionAppBroker::Observer
  void ShowInstallCompanionApp(scoped_refptr<Device> device) override {
    install_companion_app_notification_shown_ = true;
  }

  void ShowLaunchCompanionApp(scoped_refptr<Device> device) override {
    launch_companion_app_notification_shown_ = true;
  }

  void OnCompanionAppInstalled(scoped_refptr<Device> device) override {}

 protected:
  std::unique_ptr<CompanionAppBrokerImpl> companion_app_broker_;
  scoped_refptr<Device> test_device_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_environment_;
  raw_ptr<signin::IdentityManager> identity_manager_;

  bool install_companion_app_notification_shown_ = false;
  bool launch_companion_app_notification_shown_ = false;
};

TEST_F(CompanionAppBrokerImplUnitTest, MaybeShowCompanionAppActions_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  EXPECT_DEATH_IF_SUPPORTED(
      { companion_app_broker_->MaybeShowCompanionAppActions(test_device_); },
      "");
}

// The app is not installed and there is a Play store link, so regular
// logged-in users would be directed to the Play store via "install"
// notification. However, guests are only allowed to access the app through
// the browser link.
TEST_F(CompanionAppBrokerImplUnitTest,
       ShowLaunchCompanionApp_Guest_BrowserOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kValidCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kValidCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId}});

  SetIdentityManager(identity_manager_);
  SetCompanionAppInstalled(kCompanionAppId, false);
  Login(user_manager::UserType::kGuest);

  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
  companion_app_broker_->MaybeShowCompanionAppActions(test_device_);
  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_TRUE(launch_companion_app_notification_shown_);
}

// The companion app is installed, but no browser link is supplied and guests
// can only access browser link.
TEST_F(CompanionAppBrokerImplUnitTest, NoCompanionAppNotification_Guest) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kEmptyCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kValidCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId}});

  SetIdentityManager(identity_manager_);
  SetCompanionAppInstalled(kCompanionAppId, true);
  Login(user_manager::UserType::kGuest);

  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
  companion_app_broker_->MaybeShowCompanionAppActions(test_device_);
  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
}

// If the app is installed, the user should be pointed directly toward it via
// the "Launch" notification
TEST_F(CompanionAppBrokerImplUnitTest, ShowLaunchCompanionApp_Installed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kValidCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kValidCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId}});

  SetIdentityManager(identity_manager_);
  SetCompanionAppInstalled(kCompanionAppId, true);
  Login(user_manager::UserType::kRegular);

  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
  companion_app_broker_->MaybeShowCompanionAppActions(test_device_);
  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_TRUE(launch_companion_app_notification_shown_);
}

// When the app is not yet installed and there is no Play store link, users
// should be directed to the browser via "Launch" notification.
TEST_F(CompanionAppBrokerImplUnitTest, ShowLaunchCompanionApp_NoPlayStoreLink) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kValidCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kEmptyCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId}});

  SetIdentityManager(identity_manager_);
  SetCompanionAppInstalled(kCompanionAppId, false);
  Login(user_manager::UserType::kRegular);

  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
  companion_app_broker_->MaybeShowCompanionAppActions(test_device_);
  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_TRUE(launch_companion_app_notification_shown_);
}

// If no companion app information is provided for this device, the user cannot
// be directed to the app, so no notification will be shown.
TEST_F(CompanionAppBrokerImplUnitTest, NoCompanionAppNotification_NoAppInfo) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kEmptyCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kEmptyCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId}});

  SetIdentityManager(identity_manager_);
  SetCompanionAppInstalled(kCompanionAppId, false);
  Login(user_manager::UserType::kRegular);

  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
  companion_app_broker_->MaybeShowCompanionAppActions(test_device_);
  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
}

// If the app is not yet installed, the Play store link takes precedence over
// the browser link.
TEST_F(CompanionAppBrokerImplUnitTest, ShowInstallCompanionApp_PlayStoreLink) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kValidCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kValidCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId}});

  SetIdentityManager(identity_manager_);
  SetCompanionAppInstalled(kCompanionAppId, false);
  Login(user_manager::UserType::kRegular);

  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
  companion_app_broker_->MaybeShowCompanionAppActions(test_device_);
  EXPECT_TRUE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
}

TEST_F(CompanionAppBrokerImplUnitTest, InstallCompanionApp_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  EXPECT_DEATH_IF_SUPPORTED(
      { companion_app_broker_->InstallCompanionApp(test_device_); }, "");
}

// TODO(b/290816916): Update with new logic to check device metadata.
// Ensures calling InstallCompanionApp with feature enabled does not crash.
TEST_F(CompanionAppBrokerImplUnitTest, InstallCompanionApp_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kValidCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kValidCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId}});

  companion_app_broker_->InstallCompanionApp(test_device_);
}

TEST_F(CompanionAppBrokerImplUnitTest, LaunchCompanionApp_Disabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{ash::features::kFastPairPwaCompanion});

  EXPECT_DEATH_IF_SUPPORTED(
      { companion_app_broker_->LaunchCompanionApp(test_device_); }, "");
}

// TODO(b/290816916): Update with new logic to check device metadata.
// Ensures calling LaunchCompanionApp with feature enabled does not crash.
TEST_F(CompanionAppBrokerImplUnitTest, LaunchCompanionApp_Enabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kValidCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kValidCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId}});

  companion_app_broker_->LaunchCompanionApp(test_device_);
}

// If the app is not yet installed, the install playstore app prompt will be
// shown
TEST_F(CompanionAppBrokerImplUnitTest,
       ShowsInstallCompanionApp_UsingFeatureParamDeviceId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kValidCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kValidCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId},
       {ash::features::kFastPairPwaCompanionDeviceIds.name,
        kFeatureParamDeviceIds}});
  test_device_ = base::MakeRefCounted<Device>(
      kFeatureParamModelId, kTestDeviceAddress, Protocol::kFastPairInitial);

  SetIdentityManager(identity_manager_);
  SetCompanionAppInstalled(kCompanionAppId, false);
  Login(user_manager::UserType::kRegular);

  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
  companion_app_broker_->MaybeShowCompanionAppActions(test_device_);
  EXPECT_TRUE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
}

// If the device ID isn't supported the install app prompt will not be shown
TEST_F(CompanionAppBrokerImplUnitTest,
       SkipsInstallCompanionApp_UsingInvalidDeviceId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      ash::features::kFastPairPwaCompanion,
      {{ash::features::kFastPairPwaCompanionInstallUri.name,
        kValidCompanionBrowserUri},
       {ash::features::kFastPairPwaCompanionPlayStoreUri.name,
        kValidCompanionPlayStoreUri},
       {ash::features::kFastPairPwaCompanionAppId.name, kCompanionAppId},
       {ash::features::kFastPairPwaCompanionDeviceIds.name,
        kFeatureParamDeviceIds}});
  test_device_ = base::MakeRefCounted<Device>(
      kInvalidModelId, kTestDeviceAddress, Protocol::kFastPairInitial);

  SetIdentityManager(identity_manager_);
  SetCompanionAppInstalled(kCompanionAppId, false);
  Login(user_manager::UserType::kRegular);

  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
  companion_app_broker_->MaybeShowCompanionAppActions(test_device_);
  EXPECT_FALSE(install_companion_app_notification_shown_);
  EXPECT_FALSE(launch_companion_app_notification_shown_);
}

}  // namespace quick_pair
}  // namespace ash
