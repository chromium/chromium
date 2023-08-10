// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/sms_observer.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;

namespace ash {

namespace {

base::Value::Dict CreateMessage(const char* kDefaultMessage,
                                const char* kDefaultNumber,
                                const char* kDefaultTimestamp) {
  base::Value::Dict sms;
  if (kDefaultNumber)
    sms.Set("number", kDefaultNumber);
  if (kDefaultMessage)
    sms.Set("text", kDefaultMessage);
  if (kDefaultTimestamp)
    sms.Set("timestamp", kDefaultMessage);
  return sms;
}

absl::optional<const std::string> GetStringOptional(const char* text) {
  if (text) {
    return absl::make_optional<const std::string>(text);
  }
  return absl::nullopt;
}

}  // namespace

struct SmsObserverTestCase {
  std::string test_name;
  bool use_suppress_text_message_flag;
};

class SmsObserverTest
    : public AshTestBase,
      public testing::WithParamInterface<SmsObserverTestCase> {
 public:
  SmsObserverTest() = default;

  SmsObserverTest(const SmsObserverTest&) = delete;
  SmsObserverTest& operator=(const SmsObserverTest&) = delete;

  ~SmsObserverTest() override = default;

  SmsObserver* GetSmsObserver() { return Shell::Get()->sms_observer_.get(); }

  void SetUp() override {
    AshTestBase::SetUp();
    if (GetParam().use_suppress_text_message_flag) {
      features_.InitAndEnableFeature(ash::features::kSuppressTextMessages);
    } else {
      features_.InitAndDisableFeature(ash::features::kSuppressTextMessages);
    }
  }

  void SimulateMessageReceived(
      const char* kDefaultMessage = "FakeSMSClient: \xF0\x9F\x98\x8A",
      const char* kDefaultNumber = "000-000-0000",
      const char* kDefaultTimestamp = "Fri Jun  8 13:26:04 EDT 2016") {
    if (GetParam().use_suppress_text_message_flag) {
      TextMessageData message_data(GetStringOptional(kDefaultNumber),
                                   GetStringOptional(kDefaultMessage),
                                   GetStringOptional(kDefaultTimestamp));
      GetSmsObserver()->MessageReceived(message_data);
    } else {
      GetSmsObserver()->MessageReceived(
          CreateMessage(kDefaultMessage, kDefaultNumber, kDefaultTimestamp));
    }
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    SmsObserverTests,
    SmsObserverTest,
    testing::ValuesIn<SmsObserverTestCase>({
        {"SuppressTextMessagesFlagEnabled", true},
        {"SuppressTextMessagesFlagDisabled", false},
    }),
    [](const testing::TestParamInfo<SmsObserverTest::ParamType>& info) {
      return info.param.test_name;
    });

// Verify if notification is received after receiving a sms message with
// number and content.
TEST_P(SmsObserverTest, SendTextMessage) {
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  SimulateMessageReceived();
  const message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(1u, notifications.size());

  EXPECT_EQ(u"000-000-0000", (*notifications.begin())->title());
  EXPECT_EQ(u"FakeSMSClient: 😊", (*notifications.begin())->message());
  MessageCenter::Get()->RemoveAllNotifications(false /* by_user */,
                                               MessageCenter::RemoveType::ALL);
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if no notification is received if phone number is missing in sms
// message.
TEST_P(SmsObserverTest, TextMessageMissingNumber) {
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  SimulateMessageReceived("FakeSMSClient: Test Message.", nullptr);
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if no notification is received if text body is empty in sms message.
TEST_P(SmsObserverTest, TextMessageEmptyText) {
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  SimulateMessageReceived("");
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if no notification is received if the text is missing in sms message.
TEST_P(SmsObserverTest, TextMessageMissingText) {
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  SimulateMessageReceived("");
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if 2 notification received after receiving 2 sms messages from the
// same number.
TEST_P(SmsObserverTest, MultipleTextMessages) {
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  SimulateMessageReceived("first message");
  SimulateMessageReceived("second message");
  const message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(2u, notifications.size());

  for (message_center::Notification* iter : notifications) {
    if (iter->id().find("chrome://network/sms1") != std::string::npos) {
      EXPECT_EQ(u"000-000-0000", iter->title());
      EXPECT_EQ(u"first message", iter->message());
    } else if (iter->id().find("chrome://network/sms2") != std::string::npos) {
      EXPECT_EQ(u"000-000-0000", iter->title());
      EXPECT_EQ(u"second message", iter->message());
    } else {
      ASSERT_TRUE(false);
    }
  }
}

}  // namespace ash
