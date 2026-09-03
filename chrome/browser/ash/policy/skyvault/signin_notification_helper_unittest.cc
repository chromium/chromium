// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"

#include <memory>

#include "ash/public/cpp/notification_utils.h"
#include "base/check_deref.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

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
    message_center::MessageCenter::Initialize();
    user_manager::User* user =
        fake_user_manager_->AddUser(user_manager::StubAccountId());
    fake_user_manager_->LoginUser(user->GetAccountId());
    ash::AnnotatedAccountId::Set(&profile_, user->GetAccountId());
  }

  void TearDown() override { message_center::MessageCenter::Shutdown(); }

  Profile* profile() { return &profile_; }

  const message_center::Notification* GetNotification(
      const std::string& notification_id) {
    const user_manager::User& user = CHECK_DEREF(
        ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile()));
    return message_center::MessageCenter::Get()->FindNotificationById(
        ash::CreateUserScopedNotificationId(notification_id,
                                            user.username_hash()));
  }

  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  TestingProfile profile_;
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
  ShowSignInNotification(profile(), kId, file_type,
                         base::FilePath("dummy_name.jpg"), mock_cb.Get());
  ASSERT_TRUE(GetNotification(notification_id));

  EXPECT_CALL(mock_cb, Run(base::File::Error::FILE_ERROR_FAILED));
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      GetNotification(notification_id)->id(),
      NotificationButtonIndex::kCancelButton);

  EXPECT_FALSE(GetNotification(notification_id));
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
  ShowSignInNotification(profile(), kId, file_type,
                         base::FilePath("dummy_name.jpg"), mock_cb.Get(),
                         thumbnail);
  ASSERT_TRUE(GetNotification(notification_id));
  EXPECT_EQ(GetNotification(notification_id)->image().IsEmpty(), !with_image);

  EXPECT_CALL(mock_cb, Run(base::File::Error::FILE_ERROR_FAILED));
  message_center::MessageCenter::Get()->RemoveNotification(
      GetNotification(notification_id)->id(), /*by_user=*/true);

  EXPECT_FALSE(GetNotification(notification_id));
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
      profile(), kId, local_user_files::UploadTrigger::kCamera,
      base::FilePath("dummy_name." + extension), base::DoNothing());
  ASSERT_TRUE(GetNotification(notification_id));
  auto file_type = base::UTF8ToUTF16(ExtensionToFileType(extension));
  EXPECT_NE(GetNotification(notification_id)->title().find(file_type),
            std::u16string::npos);
  EXPECT_NE(GetNotification(notification_id)->message().find(file_type),
            std::u16string::npos);
}

INSTANTIATE_TEST_CASE_P(SkyVault,
                        SignInNotificationHelperCameraTest,
                        ::testing::Values("jpg", "gif", "mp4", "pdf"),
                        SignInNotificationHelperCameraTest::ParamToName);

}  // namespace policy::skyvault_ui_utils
