// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace policy::skyvault_ui_utils {

namespace {

const gfx::Image CreateTestThumbnail() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

}  // namespace

constexpr int kId = 123;

class SignInNotificationHelperTestBase : public testing::Test {
 public:
  SignInNotificationHelperTestBase() = default;

  SignInNotificationHelperTestBase(const SignInNotificationHelperTestBase&) =
      delete;
  SignInNotificationHelperTestBase& operator=(
      const SignInNotificationHelperTestBase&) = delete;

  ~SignInNotificationHelperTestBase() override = default;

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

class SignInNotificationHelperTest
    : public SignInNotificationHelperTestBase,
      public ::testing::WithParamInterface<
          std::tuple<policy::local_user_files::UploadTrigger,
                     /*notification_id*/ std::string>> {
 public:
  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    auto [file_type, id] = info.param;
    switch (file_type) {
      case policy::local_user_files::UploadTrigger::kDownload:
        return "download";
      case policy::local_user_files::UploadTrigger::kScreenCapture:
        return "screen_capture";
      case policy::local_user_files::UploadTrigger::kMigration:
        return "migration";
      case policy::local_user_files::UploadTrigger::kCamera:
        return "camera";
    }
  }

  SignInNotificationHelperTest() = default;

  SignInNotificationHelperTest(const SignInNotificationHelperTest&) = delete;
  SignInNotificationHelperTest& operator=(const SignInNotificationHelperTest&) =
      delete;

  ~SignInNotificationHelperTest() override = default;
};

class SignInNotificationHelperCameraTest
    : public SignInNotificationHelperTestBase,
      public ::testing::WithParamInterface</*extension*/ std::string> {
 public:
  static std::string ExtensionToFileType(std::string_view extension) {
    if (extension == "jpg") {
      return "photo";
    } else if (extension == "gif") {
      return "video";
    } else if (extension == "mp4") {
      return "video";
    } else if (extension == "pdf") {
      return "scan";
    }
    return "photo";
  }

  static std::string ParamToName(const testing::TestParamInfo<ParamType> info) {
    return info.param;
  }

  SignInNotificationHelperCameraTest() = default;

  SignInNotificationHelperCameraTest(
      const SignInNotificationHelperCameraTest&) = delete;
  SignInNotificationHelperCameraTest& operator=(
      const SignInNotificationHelperCameraTest&) = delete;

  ~SignInNotificationHelperCameraTest() override = default;
};

// Tests that when the user clicks on cancel, the sign-in callback will be run
// with error.
TEST_P(SignInNotificationHelperTest, ClickOnCancel) {
  auto [file_type, notification_id] = GetParam();

  base::MockCallback<base::RepeatingCallback<void(base::File::Error)>> mock_cb;
  ShowSignInNotification(profile_.get(), kId, file_type,
                         base::FilePath("dummy_name.jpg"), mock_cb.Get());
  EXPECT_TRUE(display_service_->GetNotification(notification_id).has_value());

  EXPECT_CALL(mock_cb, Run(base::File::Error::FILE_ERROR_FAILED));
  display_service_->SimulateClick(
      NotificationHandler::Type::TRANSIENT, notification_id,
      NotificationButtonIndex::kCancelButton, /*reply=*/std::nullopt);

  EXPECT_FALSE(display_service_->GetNotification(notification_id).has_value());
}

// Tests that when the user closes the notification, the sign-in callback will
// be run with error.
TEST_P(SignInNotificationHelperTest, CloseNotification) {
  auto [file_type, notification_id] = GetParam();
  const bool with_image =
      file_type == policy::local_user_files::UploadTrigger::kScreenCapture;

  base::MockCallback<base::RepeatingCallback<void(base::File::Error)>> mock_cb;
  std::optional<const gfx::Image> thumbnail =
      with_image ? std::optional<const gfx::Image>(CreateTestThumbnail())
                 : std::nullopt;
  ShowSignInNotification(profile_.get(), kId, file_type,
                         base::FilePath("dummy_name.jpg"), mock_cb.Get(),
                         thumbnail);
  EXPECT_TRUE(display_service_->GetNotification(notification_id).has_value());
  EXPECT_EQ(
      display_service_->GetNotification(notification_id)->image().IsEmpty(),
      !with_image);

  EXPECT_CALL(mock_cb, Run(base::File::Error::FILE_ERROR_FAILED));
  display_service_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                       notification_id,
                                       /*by_user=*/true,
                                       /*silent=*/false);

  EXPECT_FALSE(display_service_->GetNotification(notification_id).has_value());
}

INSTANTIATE_TEST_SUITE_P(
    SkyVault,
    SignInNotificationHelperTest,
    ::testing::Values(
        std::make_tuple(policy::local_user_files::UploadTrigger::kCamera,
                        base::StrCat({kCameraSignInNotificationIdPrefix,
                                      base::NumberToString(kId)})),
        std::make_tuple(policy::local_user_files::UploadTrigger::kDownload,
                        base::StrCat({kDownloadSignInNotificationPrefix,
                                      base::NumberToString(kId)})),
        std::make_tuple(policy::local_user_files::UploadTrigger::kMigration,
                        kMigrationSignInNotification),
        std::make_tuple(policy::local_user_files::UploadTrigger::kScreenCapture,
                        base::StrCat({kScreenCaptureSignInNotificationIdPrefix,
                                      base::NumberToString(kId)}))),

    SignInNotificationHelperTest::ParamToName);

TEST_P(SignInNotificationHelperCameraTest, CheckNotificationData) {
  auto extension = GetParam();
  std::string notification_id = base::StrCat(
      {kCameraSignInNotificationIdPrefix, base::NumberToString(kId)});
  ShowSignInNotification(
      profile_.get(), kId, local_user_files::UploadTrigger::kCamera,
      base::FilePath("dummy_name." + extension), base::DoNothing());
  EXPECT_TRUE(display_service_->GetNotification(notification_id).has_value());
  auto file_type = base::UTF8ToUTF16(ExtensionToFileType(extension));
  EXPECT_NE(display_service_->GetNotification(notification_id)
                ->title()
                .find(file_type),
            std::u16string::npos);
  EXPECT_NE(display_service_->GetNotification(notification_id)
                ->message()
                .find(file_type),
            std::u16string::npos);
}

INSTANTIATE_TEST_CASE_P(SkyVault,
                        SignInNotificationHelperCameraTest,
                        ::testing::Values("jpg", "gif", "mp4", "pdf"),
                        SignInNotificationHelperCameraTest::ParamToName);

}  // namespace policy::skyvault_ui_utils
