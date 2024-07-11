// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::skyvault_ui_utils {

class SignInNotificationHelperTest : public testing::Test {
 public:
  SignInNotificationHelperTest() = default;

  SignInNotificationHelperTest(const SignInNotificationHelperTest&) = delete;
  SignInNotificationHelperTest& operator=(const SignInNotificationHelperTest&) =
      delete;

  ~SignInNotificationHelperTest() override = default;

 protected:
  void SetUp() override {
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile_.get());
  }

  void TearDown() override { display_service_.reset(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
};

// Tests that when the user clicks on cancel, the sign-in callback will be run
// with error.
TEST_F(SignInNotificationHelperTest, Download_ClickOnCancel) {
  base::MockCallback<base::RepeatingCallback<void(base::File::Error)>> mock_cb;
  ShowSignInNotification(
      profile_.get(), /*id=*/123,
      ash::cloud_upload::OdfsSkyvaultUploader::FileType::kDownload,
      "dummy_name.txt", mock_cb.Get());
  auto notification_id = base::StrCat(
      {kDownloadSignInNotificationPrefix, base::NumberToString(123)});
  EXPECT_TRUE(display_service_->GetNotification(notification_id).has_value());

  EXPECT_CALL(mock_cb, Run(base::File::Error::FILE_ERROR_FAILED));
  display_service_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification_id,
      NotificationButtonIndex::kCancelButton, /*reply=*/std::nullopt);

  EXPECT_FALSE(display_service_->GetNotification(notification_id).has_value());
}

// Tests that when the user closes the notification, the sign-in callback will
// be run with error.
TEST_F(SignInNotificationHelperTest, Download_CloseNotification) {
  base::MockCallback<base::RepeatingCallback<void(base::File::Error)>> mock_cb;
  ShowSignInNotification(
      profile_.get(), /*id=*/123,
      ash::cloud_upload::OdfsSkyvaultUploader::FileType::kDownload,
      "dummy_name.txt", mock_cb.Get());
  auto notification_id = base::StrCat(
      {kDownloadSignInNotificationPrefix, base::NumberToString(123)});
  EXPECT_TRUE(display_service_->GetNotification(notification_id).has_value());

  EXPECT_CALL(mock_cb, Run(base::File::Error::FILE_ERROR_FAILED));
  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       notification_id,
                                       /*by_user=*/true,
                                       /*silent=*/false);

  EXPECT_FALSE(display_service_->GetNotification(notification_id).has_value());
}

}  // namespace policy::skyvault_ui_utils
