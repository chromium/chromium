// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_package_notification.h"

#include <memory>
#include <string>

#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

namespace {

constexpr char kDefaultAppFileId[] = "default_file_id";
constexpr char kSecondAppFileId[] = "default_file_id2";
constexpr char kNotificationId[] = "notification_id";

class CrostiniPackageNotificationTest : public testing::Test {
 public:
  CrostiniPackageNotificationTest() = default;

  void SetUp() override {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();
    task_environment_ = std::make_unique<content::BrowserTaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::UI,
        base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
        content::BrowserTaskEnvironment::REAL_IO_THREAD);

    profile_ = std::make_unique<TestingProfile>();
    crostini_test_helper_ =
        std::make_unique<CrostiniTestHelper>(profile_.get());
    service_ = std::make_unique<CrostiniPackageService>(profile_.get());
  }

  void TearDown() override {
    service_.reset();
    crostini_test_helper_.reset();
    profile_.reset();
    task_environment_.reset();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

 protected:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<CrostiniTestHelper> crostini_test_helper_;
  std::unique_ptr<CrostiniPackageService> service_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(CrostiniPackageNotificationTest, InstallWithNoIcons) {
  CrostiniPackageNotification notification(
      profile_.get(),
      CrostiniPackageNotification::NotificationType::PACKAGE_INSTALL,
      PackageOperationStatus::RUNNING, DefaultContainerId(), std::u16string(),
      kNotificationId, service_.get());

  notification.UpdateProgress(PackageOperationStatus::SUCCEEDED, 100);
  EXPECT_EQ(notification.GetButtonCountForTesting(), 0);
}

TEST_F(CrostiniPackageNotificationTest, InstallWithOneIcon) {
  CrostiniPackageNotification notification(
      profile_.get(),
      CrostiniPackageNotification::NotificationType::PACKAGE_INSTALL,
      PackageOperationStatus::RUNNING, DefaultContainerId(), std::u16string(),
      kNotificationId, service_.get());

  auto app = CrostiniTestHelper::BasicApp(kDefaultAppFileId);
  crostini_test_helper_->AddApp(app);

  notification.UpdateProgress(PackageOperationStatus::SUCCEEDED, 100);
  EXPECT_EQ(notification.GetButtonCountForTesting(), 1);
}

TEST_F(CrostiniPackageNotificationTest, InstallWithTwoIcons) {
  CrostiniPackageNotification notification(
      profile_.get(),
      CrostiniPackageNotification::NotificationType::PACKAGE_INSTALL,
      PackageOperationStatus::RUNNING, DefaultContainerId(), std::u16string(),
      kNotificationId, service_.get());

  auto app = CrostiniTestHelper::BasicApp(kDefaultAppFileId);
  crostini_test_helper_->AddApp(app);

  app = CrostiniTestHelper::BasicApp(kSecondAppFileId);
  crostini_test_helper_->AddApp(app);

  notification.UpdateProgress(PackageOperationStatus::SUCCEEDED, 100);
  EXPECT_EQ(notification.GetButtonCountForTesting(), 0);
}

TEST_F(CrostiniPackageNotificationTest, InstallIgnorePreviousIcons) {
  auto app = CrostiniTestHelper::BasicApp(kDefaultAppFileId);
  crostini_test_helper_->AddApp(app);

  CrostiniPackageNotification notification(
      profile_.get(),
      CrostiniPackageNotification::NotificationType::PACKAGE_INSTALL,
      PackageOperationStatus::RUNNING, DefaultContainerId(), std::u16string(),
      kNotificationId, service_.get());

  app = CrostiniTestHelper::BasicApp(kSecondAppFileId);
  crostini_test_helper_->AddApp(app);

  notification.UpdateProgress(PackageOperationStatus::SUCCEEDED, 100);
  EXPECT_EQ(notification.GetButtonCountForTesting(), 1);
}

TEST_F(CrostiniPackageNotificationTest, FailureErrorMessage) {
  CrostiniPackageNotification notification(
      profile_.get(),
      CrostiniPackageNotification::NotificationType::PACKAGE_INSTALL,
      PackageOperationStatus::RUNNING, DefaultContainerId(), std::u16string(),
      kNotificationId, service_.get());

  // Initially, the error message is blank.
  EXPECT_EQ(notification.GetErrorMessageForTesting(), "");

  // Non-failure statuses do not update the error_message.
  notification.UpdateProgress(PackageOperationStatus::RUNNING, 50,
                              "error_message_1");
  EXPECT_EQ(notification.GetErrorMessageForTesting(), "");

  // Failure statuses change the error message.
  notification.UpdateProgress(PackageOperationStatus::FAILED, 50,
                              "error_message_2");
  EXPECT_EQ(notification.GetErrorMessageForTesting(), "error_message_2");
}

}  // namespace

}  // namespace crostini
