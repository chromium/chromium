// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer.h"

#include <algorithm>

#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/constants.h"
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

class ExtensionRequestObserverTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(
            browser()->profile());
    ToggleExtensionRequest(true);
  }

  void ToggleExtensionRequest(bool enable) {
    policy_map_.Set(policy::key::kCloudReportingEnabled,
                    policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                    base::Value(true), nullptr);
    policy_map_.Set(policy::key::kCloudExtensionRequestEnabled,
                    policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                    base::Value(enable), nullptr);
    provider_.UpdateChromePolicy(policy_map_);
  }

  // Creates fake pending request in pref.
  void SetPendingList(const std::vector<std::string>& ids) {
    base::DictValue id_values;
    for (const auto& id : ids) {
      id_values.Set(
          id, base::DictValue().Set(extension_misc::kExtensionRequestTimestamp,
                                    ::base::TimeToValue(base::Time::Now())));
    }
    browser()->profile()->GetPrefs()->Set(prefs::kCloudExtensionRequestIds,
                                          base::Value(std::move(id_values)));
  }

  std::vector<std::optional<message_center::Notification>>
  GetAllNotifications() {
    return {display_service_tester_->GetNotification(kApprovedNotificationId),
            display_service_tester_->GetNotification(kRejectedNotificationId),
            display_service_tester_->GetNotification(kInstalledNotificationId)};
  }

  // Waits and verifies if the notifications are displayed or not.
  void VerifyNotification(bool has_notification) {
    base::RunLoop().RunUntilIdle();
    for (auto& notification : GetAllNotifications()) {
      EXPECT_EQ(has_notification, notification.has_value());
    }
  }

  void SetExtensionSettings(const std::string& settings_string) {
    std::optional<base::Value> settings = base::JSONReader::Read(
        settings_string, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(settings.has_value());

    policy_map_.Set(policy::key::kExtensionSettings,
                    policy::POLICY_LEVEL_MANDATORY,
                    policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
                    std::move(*settings), nullptr);
    provider_.UpdateChromePolicy(policy_map_);
  }

  void CloseNotificationAndVerify(
      const std::string& notification_id,
      const std::vector<std::string>& expected_removed_requests) {
    // Record the number of requests before closing any notification.
    size_t number_of_existing_requests =
        browser()
            ->profile()
            ->GetPrefs()
            ->GetDict(prefs::kCloudExtensionRequestIds)
            .size();

    // Close the notification
    base::RunLoop close_run_loop;
    display_service_tester_->SetNotificationClosedClosure(
        close_run_loop.QuitClosure());
    display_service_tester_->SimulateClick(
        NotificationHandler::Type::TRANSIENT, notification_id,
        std::optional<int>(), std::optional<std::u16string>());
    close_run_loop.Run();

    // Verify that only |expected_removed_requests| are removed from the pref.
    const base::DictValue& actual_pending_requests =
        browser()->profile()->GetPrefs()->GetDict(
            prefs::kCloudExtensionRequestIds);
    EXPECT_EQ(number_of_existing_requests - expected_removed_requests.size(),
              actual_pending_requests.size());
    for (auto it : actual_pending_requests) {
      EXPECT_FALSE(std::ranges::contains(expected_removed_requests, it.first));
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
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
  policy::PolicyMap policy_map_;
};

IN_PROC_BROWSER_TEST_F(ExtensionRequestObserverTest, NoPendingRequestTest) {
  SetPendingList({});
  ExtensionRequestObserver observer(browser()->profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(false);
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionRequestObserverTest, UserConfirmNotification) {
  SetPendingList({kExtensionId1, kExtensionId2, kExtensionId3, kExtensionId4,
                  kExtensionId5, kExtensionId6});
  ExtensionRequestObserver observer(browser()->profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(true);

  CloseNotificationAndVerify(kApprovedNotificationId, {kExtensionId1});
  CloseNotificationAndVerify(kRejectedNotificationId, {kExtensionId2});
  CloseNotificationAndVerify(kInstalledNotificationId,
                             {kExtensionId3, kExtensionId4});
}

IN_PROC_BROWSER_TEST_F(ExtensionRequestObserverTest,
                       NotificationClosedWithoutUserConfirmed) {
  std::vector<std::string> pending_list = {kExtensionId1, kExtensionId2,
                                           kExtensionId3, kExtensionId4,
                                           kExtensionId5, kExtensionId6};
  SetPendingList(pending_list);
  std::unique_ptr<ExtensionRequestObserver> observer =
      std::make_unique<ExtensionRequestObserver>(browser()->profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(true);

  observer.reset();
  VerifyNotification(false);

  // No request removed when notification is not closed by user.
  EXPECT_EQ(pending_list.size(), browser()
                                     ->profile()
                                     ->GetPrefs()
                                     ->GetDict(prefs::kCloudExtensionRequestIds)
                                     .size());
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionRequestObserverTest, NotificationClose) {
  SetPendingList({kExtensionId1, kExtensionId2, kExtensionId3, kExtensionId4,
                  kExtensionId5, kExtensionId6});
  ExtensionRequestObserver observer(browser()->profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(true);

  SetExtensionSettings("{}");
  VerifyNotification(false);
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionRequestObserverTest, NotificationUpdate) {
  SetPendingList({kExtensionId1, kExtensionId2, kExtensionId3, kExtensionId4,
                  kExtensionId5, kExtensionId6});
  ExtensionRequestObserver observer(browser()->profile());
  VerifyNotification(false);

  SetExtensionSettings(kExtensionSettings);
  VerifyNotification(true);

  SetExtensionSettings(kExtensionSettingsUpdate);
  VerifyNotification(true);
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionRequestObserverTest,
                       ExtensionRequestPolicyToggle) {
  std::vector<std::string> pending_list = {kExtensionId1, kExtensionId2,
                                           kExtensionId3, kExtensionId4,
                                           kExtensionId5, kExtensionId6};
  SetPendingList(pending_list);
  SetExtensionSettings(kExtensionSettings);
  ToggleExtensionRequest(false);

  // No notification without the policy.
  ExtensionRequestObserver observer(browser()->profile());
  VerifyNotification(false);

  // Show notification when the policy is turned on.
  ToggleExtensionRequest(true);
  VerifyNotification(true);

  // Close all notifictions when the policy is off again.
  ToggleExtensionRequest(false);
  VerifyNotification(false);

  // And no pending requests are removed.
  EXPECT_EQ(pending_list.size(), browser()
                                     ->profile()
                                     ->GetPrefs()
                                     ->GetDict(prefs::kCloudExtensionRequestIds)
                                     .size());
  histogram_tester()->ExpectTotalCount(kPendingListUpdateMetricsName, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionRequestObserverTest,
                       PendingRequestAddedAfterPolicyUpdated) {
  ExtensionRequestObserver observer(browser()->profile());
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

IN_PROC_BROWSER_TEST_F(ExtensionRequestObserverTest,
                       UpdateWithReportEnabledAndDisabled) {
  ExtensionRequestObserver observer(browser()->profile());

  base::MockCallback<ExtensionRequestObserver::ReportTrigger> callback;

  observer.EnableReport(callback.Get());
  EXPECT_CALL(callback, Run(browser()->profile())).Times(1);
  SetPendingList({kExtensionId1});

  observer.DisableReport();
  EXPECT_CALL(callback, Run(::testing::_)).Times(0);
  SetPendingList({kExtensionId1, kExtensionId2});
}

}  // namespace enterprise_reporting
