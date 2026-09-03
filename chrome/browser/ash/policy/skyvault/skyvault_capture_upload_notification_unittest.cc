// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome/browser/ash/policy/skyvault/skyvault_capture_upload_notification.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy::skyvault {

constexpr char kUploadNotificationId[] = "skyvault_capture_upload_notification";

class SkyvaultCaptureUploadNotificationTest : public BrowserWithTestWindowTest {
 public:
  SkyvaultCaptureUploadNotificationTest() = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().AppendASCII("test_capture.png");
    base::WriteFile(file_path_, "test content");
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath file_path_;
};

TEST_F(SkyvaultCaptureUploadNotificationTest, CreationAndDisplay) {
  SkyvaultCaptureUploadNotification notification(file_path_,
                                                 /*for_video=*/false);
  base::RunLoop().RunUntilIdle();

  const message_center::Notification* displayed_notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          kUploadNotificationId);
  ASSERT_TRUE(displayed_notification);
  EXPECT_EQ(displayed_notification->type(),
            message_center::NOTIFICATION_TYPE_PROGRESS);
  EXPECT_EQ(displayed_notification->progress(), 0);
}

TEST_F(SkyvaultCaptureUploadNotificationTest, UpdateProgress) {
  SkyvaultCaptureUploadNotification notification(file_path_,
                                                 /*for_video=*/false);
  base::RunLoop().RunUntilIdle();

  notification.UpdateProgress(6);
  base::RunLoop().RunUntilIdle();

  const message_center::Notification* displayed_notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          kUploadNotificationId);
  ASSERT_TRUE(displayed_notification);
  EXPECT_EQ(displayed_notification->progress(), 50);
}

TEST_F(SkyvaultCaptureUploadNotificationTest, CancelClosure) {
  SkyvaultCaptureUploadNotification notification(file_path_,
                                                 /*for_video=*/false);
  base::RunLoop().RunUntilIdle();

  bool cancel_called = false;
  notification.SetCancelClosure(
      base::BindLambdaForTesting([&cancel_called]() { cancel_called = true; }));

  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      kUploadNotificationId, 0);

  EXPECT_TRUE(cancel_called);
}

}  // namespace policy::skyvault
