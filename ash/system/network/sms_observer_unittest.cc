// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/sms_observer.h"

#include <memory>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;

namespace ash {

namespace {

std::unique_ptr<base::DictionaryValue> CreateMessage(
    const char* kDefaultMessage = "FakeSMSClient: \xF0\x9F\x98\x8A",
    const char* kDefaultNumber = "000-000-0000",
    const char* kDefaultTimestamp = "Fri Jun  8 13:26:04 EDT 2016") {
  std::unique_ptr<base::DictionaryValue> sms =
      std::make_unique<base::DictionaryValue>();
  if (kDefaultNumber)
    sms->SetString("number", kDefaultNumber);
  if (kDefaultMessage)
    sms->SetString("text", kDefaultMessage);
  if (kDefaultTimestamp)
    sms->SetString("timestamp", kDefaultMessage);
  return sms;
}

}  // namespace

class SmsObserverTest : public AshTestBase {
 public:
  SmsObserverTest() = default;
  ~SmsObserverTest() override = default;

  SmsObserver* GetSmsObserver() { return Shell::Get()->sms_observer_.get(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(SmsObserverTest);
};

// Verify if notification is received after receiving a sms message with
// number and content.
TEST_F(SmsObserverTest, SendTextMessage) {
  SmsObserver* sms_observer = GetSmsObserver();
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());

  std::unique_ptr<base::DictionaryValue> sms(CreateMessage());
  sms_observer->MessageReceived(*sms);

  const message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(1u, notifications.size());

  EXPECT_EQ(base::ASCIIToUTF16("000-000-0000"),
            (*notifications.begin())->title());
  EXPECT_EQ(base::UTF8ToUTF16("FakeSMSClient: \xF0\x9F\x98\x8A"),
            (*notifications.begin())->message());
  MessageCenter::Get()->RemoveAllNotifications(false /* by_user */,
                                               MessageCenter::RemoveType::ALL);
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if no notification is received if phone number is missing in sms
// message.
TEST_F(SmsObserverTest, TextMessageMissingNumber) {
  SmsObserver* sms_observer = GetSmsObserver();
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());

  std::unique_ptr<base::DictionaryValue> sms(
      CreateMessage("FakeSMSClient: Test Message.", nullptr));
  sms_observer->MessageReceived(*sms);
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if no notification is received if text body is empty in sms message.
TEST_F(SmsObserverTest, TextMessageEmptyText) {
  SmsObserver* sms_observer = GetSmsObserver();
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());

  std::unique_ptr<base::DictionaryValue> sms(CreateMessage(""));
  sms_observer->MessageReceived(*sms);
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if no notification is received if the text is missing in sms message.
TEST_F(SmsObserverTest, TextMessageMissingText) {
  SmsObserver* sms_observer = GetSmsObserver();
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  std::unique_ptr<base::DictionaryValue> sms(CreateMessage(nullptr));
  sms_observer->MessageReceived(*sms);
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if 2 notification received after receiving 2 sms messages from the
// same number.
TEST_F(SmsObserverTest, MultipleTextMessages) {
  SmsObserver* sms_observer = GetSmsObserver();
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());

  std::unique_ptr<base::DictionaryValue> sms(CreateMessage("first message"));
  sms_observer->MessageReceived(*sms);
  std::unique_ptr<base::DictionaryValue> sms2(CreateMessage("second message"));
  sms_observer->MessageReceived(*sms2);
  const message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(2u, notifications.size());

  for (message_center::Notification* iter : notifications) {
    if (iter->id().find("chrome://network/sms1") != std::string::npos) {
      EXPECT_EQ(base::ASCIIToUTF16("000-000-0000"), iter->title());
      EXPECT_EQ(base::ASCIIToUTF16("first message"), iter->message());
    } else if (iter->id().find("chrome://network/sms2") != std::string::npos) {
      EXPECT_EQ(base::ASCIIToUTF16("000-000-0000"), iter->title());
      EXPECT_EQ(base::ASCIIToUTF16("second message"), iter->message());
    } else {
      ASSERT_TRUE(false);
    }
  }
}

}  // namespace ash
