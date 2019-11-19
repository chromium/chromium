// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_notification_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using ::testing::_;

class MockPermissionRequestNotificationHandlerDelegate
    : public PermissionRequestNotificationHandler::Delegate {
 public:
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(Click, void(int));
  MOCK_METHOD0(Closure, void());
};

class PermissionRequestNotificationHandlerTest : public testing::Test {
 public:
  PermissionRequestNotificationHandler*
  permission_request_notification_handler() {
    return &permission_request_notification_handler_;
  }

  void ExpectNotificationDelegatesSizeEqualTo(size_t expected_size) {
    EXPECT_EQ(expected_size, permission_request_notification_handler()
                                 ->notification_delegates_for_testing()
                                 .size());
  }

 private:
  PermissionRequestNotificationHandler permission_request_notification_handler_;
};

TEST_F(PermissionRequestNotificationHandlerTest,
       NotificationDelegates_UpdatesCorrectly) {
  MockPermissionRequestNotificationHandlerDelegate delegate;
  MockPermissionRequestNotificationHandlerDelegate delegate_2;

  // Adding delegate correctly inserts the delegate.
  permission_request_notification_handler()->AddNotificationDelegate("id_1",
                                                                     &delegate);
  ExpectNotificationDelegatesSizeEqualTo(1u);

  // Adding a second delegate inserts a second delegate.
  permission_request_notification_handler()->AddNotificationDelegate("id_2",
                                                                     &delegate);
  ExpectNotificationDelegatesSizeEqualTo(2u);

  // Removing a delegate removes that delegate.
  permission_request_notification_handler()->RemoveNotificationDelegate("id_2");
  ExpectNotificationDelegatesSizeEqualTo(1u);

  // Removing a non-existent delegate does nothing.
  permission_request_notification_handler()->RemoveNotificationDelegate(
      "not_id");
  ExpectNotificationDelegatesSizeEqualTo(1u);
}

TEST_F(PermissionRequestNotificationHandlerTest,
       OnClose_CallsDelegate_AndRemovesIt) {
  MockPermissionRequestNotificationHandlerDelegate delegate;
  permission_request_notification_handler()->AddNotificationDelegate("id",
                                                                     &delegate);

  ExpectNotificationDelegatesSizeEqualTo(1u);

  // Done closure is always called at the end.
  EXPECT_CALL(delegate, Closure).Times(2);
  EXPECT_CALL(delegate, Close).Times(2);

  permission_request_notification_handler()->OnClose(
      nullptr, GURL(), "id", true /* by_user */,
      base::BindOnce(&MockPermissionRequestNotificationHandlerDelegate::Closure,
                     base::Unretained(&delegate)));

  ExpectNotificationDelegatesSizeEqualTo(0u);

  permission_request_notification_handler()->AddNotificationDelegate("id",
                                                                     &delegate);

  ExpectNotificationDelegatesSizeEqualTo(1u);

  permission_request_notification_handler()->OnClose(
      nullptr, GURL(), "id", false /* by_user */,
      base::BindOnce(&MockPermissionRequestNotificationHandlerDelegate::Closure,
                     base::Unretained(&delegate)));

  ExpectNotificationDelegatesSizeEqualTo(0u);
}

TEST_F(PermissionRequestNotificationHandlerTest,
       OnClickWithoutActionIndex_DoesNotCallDelegate) {
  MockPermissionRequestNotificationHandlerDelegate delegate;
  permission_request_notification_handler()->AddNotificationDelegate("id",
                                                                     &delegate);

  // Done closure is always called at the end.
  EXPECT_CALL(delegate, Closure).Times(1);
  EXPECT_CALL(delegate, Click(_)).Times(0);

  permission_request_notification_handler()->OnClick(
      nullptr, GURL(), "id", base::nullopt, base::UTF8ToUTF16("reply"),
      base::BindOnce(&MockPermissionRequestNotificationHandlerDelegate::Closure,
                     base::Unretained(&delegate)));
}

TEST_F(PermissionRequestNotificationHandlerTest,
       OnClickWithActionIndex_CallsDelegate) {
  MockPermissionRequestNotificationHandlerDelegate delegate;
  permission_request_notification_handler()->AddNotificationDelegate("id",
                                                                     &delegate);

  const int kButtonIndex = 0;

  // Done closure is always called at the end.
  EXPECT_CALL(delegate, Closure).Times(1);
  EXPECT_CALL(delegate, Click(kButtonIndex)).Times(1);

  permission_request_notification_handler()->OnClick(
      nullptr, GURL(), "id", kButtonIndex, base::nullopt,
      base::BindOnce(&MockPermissionRequestNotificationHandlerDelegate::Closure,
                     base::Unretained(&delegate)));
}
