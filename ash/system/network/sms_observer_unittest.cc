// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/sms_observer.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/network/fake_network_metadata_store.h"
#include "chromeos/ash/components/network/metrics/cellular_network_metrics_logger.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_sms_handler.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"
#include "components/onc/onc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"

using message_center::MessageCenter;

namespace ash {

namespace {

constexpr char kTestGuid[] = "TestGuid";

std::optional<const std::string> GetStringOptional(const char* text) {
  if (text) {
    return std::make_optional<const std::string>(text);
  }
  return std::nullopt;
}

}  // namespace

struct SmsObserverTestCase {
  std::string test_name;
  bool use_suppress_text_message_flag;
};

class SmsObserverTest : public AshTestBase {
 public:
  SmsObserverTest() = default;

  SmsObserverTest(const SmsObserverTest&) = delete;
  SmsObserverTest& operator=(const SmsObserverTest&) = delete;

  ~SmsObserverTest() override = default;

  SmsObserver* GetSmsObserver() { return Shell::Get()->sms_observer_.get(); }

  void SetUp() override {
    AshTestBase::SetUp();
      test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
      NetworkHandler::Get()->text_message_provider()->SetNetworkMetadataStore(
          &network_metadata_store_);
  }

  void SimulateMessageReceived(
      const char* kDefaultMessage = "FakeSMSClient: \xF0\x9F\x98\x8A",
      const char* kDefaultNumber = "000-000-0000",
      const char* kDefaultTimestamp = "Fri Jun  8 13:26:04 EDT 2016") {
    TextMessageData message_data(GetStringOptional(kDefaultNumber),
                                 GetStringOptional(kDefaultMessage),
                                 GetStringOptional(kDefaultTimestamp));
    GetSmsObserver()->MessageReceived(kTestGuid, message_data);
  }

  NetworkMetadataStore* network_metadata_store() {
    return &network_metadata_store_;
  }

  ManagedNetworkConfigurationHandler* managed_network_configuration_handler() {
    return NetworkHandler::Get()->managed_network_configuration_handler();
  }

 private:
  base::test::ScopedFeatureList features_;
  std::unique_ptr<NetworkHandlerTestHelper> test_helper_;
  FakeNetworkMetadataStore network_metadata_store_;
};

// Verify if notification is received after receiving a sms message with
// number and content.
TEST_F(SmsObserverTest, SendTextMessage) {
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
TEST_F(SmsObserverTest, TextMessageMissingNumber) {
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  SimulateMessageReceived("FakeSMSClient: Test Message.", nullptr);
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if no notification is received if text body is empty in sms message.
TEST_F(SmsObserverTest, TextMessageEmptyText) {
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  SimulateMessageReceived("");
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if no notification is received if the text is missing in sms message.
TEST_F(SmsObserverTest, TextMessageMissingText) {
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
  SimulateMessageReceived("");
  EXPECT_EQ(0u, MessageCenter::Get()->GetVisibleNotifications().size());
}

// Verify if 2 notification received after receiving 2 sms messages from the
// same number.
TEST_F(SmsObserverTest, MultipleTextMessages) {
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

class SmsObserverSuppressTextMessagesEnabled : public SmsObserverTest {
 public:
  void SetUp() override {
    SmsObserverTest::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  void ChangeUserSuppressionState(UserTextMessageSuppressionState state) {
    network_metadata_store()->SetUserTextMessageSuppressionState(kTestGuid,
                                                                 state);
  }

  void ChangePolicySuppressionState(PolicyTextMessageSuppressionState state) {
    std::string state_onc;
    switch (state) {
      case PolicyTextMessageSuppressionState::kUnset:
        state_onc = ::onc::cellular::kTextMessagesUnset;
        break;
      case PolicyTextMessageSuppressionState::kAllow:
        state_onc = ::onc::cellular::kTextMessagesAllow;
        break;
      case PolicyTextMessageSuppressionState::kSuppress:
        state_onc = ::onc::cellular::kTextMessagesSuppress;
        break;
    }

    base::Value::Dict global_config;
    global_config.Set(::onc::global_network_config::kAllowTextMessages,
                      state_onc);
    managed_network_configuration_handler()->SetPolicy(
        ::onc::ONC_SOURCE_DEVICE_POLICY, /*userhash=*/std::string(),
        base::Value::List(), global_config);
    base::RunLoop().RunUntilIdle();
  }

  void AssertHistogramCounts(size_t user_suppressed_count,
                             size_t policy_suppressed_count,
                             size_t not_suppressed_count) {
    histogram_tester_->ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kAllowTextMessagesNotificationSuppressionState,
        CellularNetworkMetricsLogger::NotificationSuppressionState::
            kUserSuppressed,
        /*expected_count=*/user_suppressed_count);

    histogram_tester_->ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kAllowTextMessagesNotificationSuppressionState,
        CellularNetworkMetricsLogger::NotificationSuppressionState::
            kPolicySuppressed,
        /*expected_count=*/policy_suppressed_count);

    histogram_tester_->ExpectBucketCount(
        CellularNetworkMetricsLogger::
            kAllowTextMessagesNotificationSuppressionState,
        CellularNetworkMetricsLogger::NotificationSuppressionState::
            kNotSuppressed,
        /*expected_count=*/not_suppressed_count);

    size_t total_count =
        user_suppressed_count + policy_suppressed_count + not_suppressed_count;
    histogram_tester_->ExpectTotalCount(
        CellularNetworkMetricsLogger::
            kAllowTextMessagesNotificationSuppressionState,
        /*expected_count=*/total_count);
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

TEST_F(SmsObserverSuppressTextMessagesEnabled, SuccessGuardRailMetricsTest) {
  base::HistogramTester histogram_tester;

  AssertHistogramCounts(/*user_suppressed_count=*/0u,
                        /*policy_suppressed_count=*/0u,
                        /*not_suppressed_count=*/0u);
  ChangeUserSuppressionState(UserTextMessageSuppressionState::kAllow);
  ChangePolicySuppressionState(PolicyTextMessageSuppressionState::kUnset);
  SimulateMessageReceived();
  AssertHistogramCounts(/*user_suppressed_count=*/0u,
                        /*policy_suppressed_count=*/0u,
                        /*not_suppressed_count=*/1u);

  ChangeUserSuppressionState(UserTextMessageSuppressionState::kSuppress);
  ChangePolicySuppressionState(PolicyTextMessageSuppressionState::kUnset);
  SimulateMessageReceived();
  AssertHistogramCounts(/*user_suppressed_count=*/1u,
                        /*policy_suppressed_count=*/0u,
                        /*not_suppressed_count=*/1u);

  ChangeUserSuppressionState(UserTextMessageSuppressionState::kAllow);
  ChangePolicySuppressionState(PolicyTextMessageSuppressionState::kAllow);
  SimulateMessageReceived();
  AssertHistogramCounts(/*user_suppressed_count=*/1u,
                        /*policy_suppressed_count=*/0u,
                        /*not_suppressed_count=*/2u);

  ChangeUserSuppressionState(UserTextMessageSuppressionState::kSuppress);
  ChangePolicySuppressionState(PolicyTextMessageSuppressionState::kAllow);
  SimulateMessageReceived();
  AssertHistogramCounts(/*user_suppressed_count=*/1u,
                        /*policy_suppressed_count=*/0u,
                        /*not_suppressed_count=*/3u);

  ChangeUserSuppressionState(UserTextMessageSuppressionState::kSuppress);
  ChangePolicySuppressionState(PolicyTextMessageSuppressionState::kSuppress);
  SimulateMessageReceived();
  AssertHistogramCounts(/*user_suppressed_count=*/1u,
                        /*policy_suppressed_count=*/1u,
                        /*not_suppressed_count=*/3u);

  ChangeUserSuppressionState(UserTextMessageSuppressionState::kAllow);
  ChangePolicySuppressionState(PolicyTextMessageSuppressionState::kSuppress);
  SimulateMessageReceived();
  AssertHistogramCounts(/*user_suppressed_count=*/1u,
                        /*policy_suppressed_count=*/2u,
                        /*not_suppressed_count=*/3u);
}

}  // namespace ash
