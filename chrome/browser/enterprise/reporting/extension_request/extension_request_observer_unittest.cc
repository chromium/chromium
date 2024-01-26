// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"

#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "extensions/browser/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

namespace {

constexpr char kApprovedNotificationId[] = "extension_approved_notificaiton";
constexpr char kRejectedNotificationId[] = "extension_rejected_notificaiton";
constexpr char kInstalledNotificationId[] = "extension_installed_notificaiton";

constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "abcdefghijklmnopabcdefghijklmnpo";
constexpr char kExtensionId3[] = "abcdefghijklmnopabcdefghijklmoop";
constexpr char kExtensionId4[] = "abcdefghijklmnopabcdefghijklmopo";
constexpr char kExtensionId5[] = "abcdefghijklmnopabcdefghijklmpop";
constexpr char kExtensionId6[] = "abcdefghijklmnopabcdefghijklmppo";

constexpr char kExtensionSettings[] = R"({
  "abcdefghijklmnopabcdefghijklmnop" : {
    "installation_mode": "allowed"
  },
  "abcdefghijklmnopabcdefghijklmnpo" : {
    "installation_mode": "blocked"
  },
  "abcdefghijklmnopabcdefghijklmoop" : {
    "installation_mode": "force_installed",
    "update_url": "https://clients2.google.com/service/update2/crx"
  },
  "abcdefghijklmnopabcdefghijklmopo" : {
    "installation_mode": "normal_installed",
    "update_url": "https://clients2.google.com/service/update2/crx"
  }
})";

constexpr char kExtensionSettingsUpdate[] = R"({
  "abcdefghijklmnopabcdefghijklmnop" : {
    "installation_mode": "blocked"
  },
  "abcdefghijklmnopabcdefghijklmnpo" : {
    "installation_mode": "allowed"
  },
  "abcdefghijklmnopabcdefghijklmoop" : {
    "installation_mode": "normal_installed",
    "update_url": "https://clients2.google.com/service/update2/crx"
  },
  "abcdefghijklmnopabcdefghijklmopo" : {
    "installation_mode": "force_installed",
    "update_url": "https://clients2.google.com/service/update2/crx"
  }
})";

constexpr char kPendingListUpdateMetricsName[] =
    "Enterprise.CloudExtensionRequestUpdated";

}  // namespace

class ExtensionRequestObserverTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    ToggleExtensionRequest(true);
  }

  void ToggleExtensionRequest(bool enable) {
    profile()->GetTestingPrefService()->SetManagedPref(
        prefs::kCloudExtensionRequestEnabled,
        std::make_unique<base::Value>(enable));
  }

  // Creates fake pending request in pref.
  void SetPendingList(const std::vector<std::string>& ids) {
    base::Value::Dict id_values;
    for (const auto& id : ids) {
      id_values.Set(id, base::Value::Dict().Set(
                            extension_misc::kExtensionRequestTimestamp,
                            ::base::TimeToValue(base::Time::Now())));
    }
    profile()->GetTestingPrefService()->SetUserPref(
        prefs::kCloudExtensionRequestIds, std::move(id_values));
  }

  std::vector<std::optional<message_center::Notification>>
  GetAllNotifications() {
    return {display_service_tester_->GetNotification(kApprovedNotificationId),
            display_service_tester_->GetNotification(kRejectedNotificationId),
            display_service_tester_->GetNotification(kInstalledNotificationId)};
  }

  // Waits and verifies if the notifications are displayed or not.
  void VerifyNotification(bool has_notification) {
    task_environment()->RunUntilIdle();
    for (auto& notification : GetAllNotifications())
      EXPECT_EQ(has_notification, notification.has_value());
  }

  //
  void SetExtensionSettings(const std::string& settings_string) {
    std::optional<base::Value> settings =
        base::JSONReader::Read(settings_string);
    ASSERT_TRUE(settings.has_value());
    profile()->GetTestingPrefService()->SetManagedPref(
        extensions::pref_names::kExtensionManagement, std::move(*settings));
  }

  void CloseNotificationAndVerify(
      const std::string& notification_id,
      const std::vector<std::string>& expected_removed_requests) {
    // Record the number of requests before closing any notification.
    size_t number_of_existing_requests =
        profile()->GetPrefs()->GetDict(prefs::kCloudExtensionRequestIds).size();

    // Close the notification
    base::RunLoop close_run_loop;
    display_service_tester_->SetNotificationClosedClosure(
        close_run_loop.QuitClosure());
    display_service_tester_->SimulateClick(
        NotificationHandler::Type::TRANSIENT, notification_id,
        std::optional<int>(), std::optional<std::u16string>());
    close_run_loop.Run();

    // Verify that only |expected_removed_requests| are removed from the pref.
    const base::Value::Dict& actual_pending_requests =
        profile()->GetPrefs()->GetDict(prefs::kCloudExtensionRequestIds);
    EXPECT_EQ(number_of_existing_requests - expected_removed_requests.size(),
              actual_pending_requests.size());
    for (auto it : actual_pending_requests) {
      EXPECT_FALSE(base::Contains(expected_removed_requests, it.first));
    }
    closed_notification_count_ += 1;
    histogram_tester()->ExpectBucketCount(kPendingListUpdateMetricsName,
                                          /*removed*/ 1,
                                          closed_notification_count_);
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;
  int closed_notification_count_ = 0;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;
};

TEST_F(ExtensionRequestObserverTest, NoPendingRequestTest) {
  SetPendingList({});
  ExtensionRequestObserver observer(profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(false);
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

TEST_F(ExtensionRequestObserverTest, UserConfirmNotification) {
  SetPendingList({kExtensionId1, kExtensionId2, kExtensionId3, kExtensionId4,
                  kExtensionId5, kExtensionId6});
  ExtensionRequestObserver observer(profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(true);

  CloseNotificationAndVerify(kApprovedNotificationId, {kExtensionId1});
  CloseNotificationAndVerify(kRejectedNotificationId, {kExtensionId2});
  CloseNotificationAndVerify(kInstalledNotificationId,
                             {kExtensionId3, kExtensionId4});
}

TEST_F(ExtensionRequestObserverTest, NotificationClosedWithoutUserConfirmed) {
  std::vector<std::string> pending_list = {kExtensionId1, kExtensionId2,
                                           kExtensionId3, kExtensionId4,
                                           kExtensionId5, kExtensionId6};
  SetPendingList(pending_list);
  std::unique_ptr<ExtensionRequestObserver> observer =
      std::make_unique<ExtensionRequestObserver>(profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(true);

  observer.reset();
  VerifyNotification(false);

  // No request removed when notification is not closed by user.
  EXPECT_EQ(
      pending_list.size(),
      profile()->GetPrefs()->GetDict(prefs::kCloudExtensionRequestIds).size());
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

TEST_F(ExtensionRequestObserverTest, NotificationClose) {
  SetPendingList({kExtensionId1, kExtensionId2, kExtensionId3, kExtensionId4,
                  kExtensionId5, kExtensionId6});
  ExtensionRequestObserver observer(profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(true);

  SetExtensionSettings("{}");
  VerifyNotification(false);
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

TEST_F(ExtensionRequestObserverTest, NotificationUpdate) {
  SetPendingList({kExtensionId1, kExtensionId2, kExtensionId3, kExtensionId4,
                  kExtensionId5, kExtensionId6});
  ExtensionRequestObserver observer(profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(true);

  SetExtensionSettings(kExtensionSettingsUpdate);
  VerifyNotification(true);
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

TEST_F(ExtensionRequestObserverTest, ExtensionRequestPolicyToggle) {
  std::vector<std::string> pending_list = {kExtensionId1, kExtensionId2,
                                           kExtensionId3, kExtensionId4,
                                           kExtensionId5, kExtensionId6};
  SetPendingList(pending_list);
  SetExtensionSettings(kExtensionSettings);
  ToggleExtensionRequest(false);

  // No notification without the policy.
  ExtensionRequestObserver observer(profile());
  VerifyNotification(false);

  // Show notification when the policy is turned on.
  ToggleExtensionRequest(true);
  VerifyNotification(true);

  // Close all notifictions when the policy is off again.
  ToggleExtensionRequest(false);
  VerifyNotification(false);

  // And no pending requests are removed.
  EXPECT_EQ(
      pending_list.size(),
      profile()->GetPrefs()->GetDict(prefs::kCloudExtensionRequestIds).size());
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

TEST_F(ExtensionRequestObserverTest, PendingRequestAddedAfterPolicyUpdated) {
  ExtensionRequestObserver observer(profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(false);
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);

  SetPendingList({kExtensionId1, kExtensionId2, kExtensionId3, kExtensionId4,
                  kExtensionId5, kExtensionId6});
  VerifyNotification(true);
  histogram_tester()->ExpectUniqueSample(kPendingListUpdateMetricsName,
                                         /*added*/ 0, 1);
}

TEST_F(ExtensionRequestObserverTest, UpdateWithReportEnabledAndDisabled) {
  ExtensionRequestObserver observer(profile());

  base::MockCallback<ExtensionRequestObserver::ReportTrigger> callback;

  observer.EnableReport(callback.Get());
  EXPECT_CALL(callback, Run(profile())).Times(1);
  SetPendingList({kExtensionId1});

  observer.DisableReport();
  EXPECT_CALL(callback, Run(::testing::_)).Times(0);
  SetPendingList({kExtensionId1, kExtensionId2});
}

}  // namespace enterprise_reporting
