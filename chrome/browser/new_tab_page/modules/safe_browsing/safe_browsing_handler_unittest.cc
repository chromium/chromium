// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/safe_browsing/safe_browsing_handler.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/safe_browsing_metrics_collector_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search/ntp_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::safe_browsing::SafeBrowsingMetricsCollector;
using ::safe_browsing::SafeBrowsingMetricsCollectorFactory;
using ::safe_browsing::SafeBrowsingState;

namespace ntp {

class MockSafeBrowsingMetricsCollector : public SafeBrowsingMetricsCollector {
 public:
  explicit MockSafeBrowsingMetricsCollector(PrefService* pref_service)
      : SafeBrowsingMetricsCollector(pref_service) {}
  MOCK_METHOD(std::optional<base::Time>,
              GetLatestSecuritySensitiveEventTimestamp,
              (),
              (override));
};

class SafeBrowsingHandlerTest : public ::testing::Test {
 public:
  SafeBrowsingHandlerTest() : manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(manager_.SetUp());
    TestingProfile* profile = manager_.CreateTestingProfile("Test Profile");

    // Set up PrefService and register prefs
    pref_service_ = profile->GetTestingPrefService();

    // Set up MockSafeBrowsingMetricsCollector
    SafeBrowsingMetricsCollectorFactory::GetInstance()->SetTestingFactory(
        profile,
        base::BindRepeating(
            &SafeBrowsingHandlerTest::GetMockSafeBrowsingMetricsCollector,
            base::Unretained(this)));

    // Create SafeBrowsingHandler which will use the test prefs and
    // MockSafeBrowsingMetricsCollector
    module_handler_ = std::make_unique<SafeBrowsingHandler>(
        safe_browsing_handler_remote_.InitWithNewPipeAndPassReceiver(),
        profile);
  }

  void TearDown() override { metrics_collector_->Shutdown(); }

  std::unique_ptr<KeyedService> GetMockSafeBrowsingMetricsCollector(
      content::BrowserContext* browser_context) {
    std::unique_ptr<MockSafeBrowsingMetricsCollector> metrics_collector =
        std::make_unique<MockSafeBrowsingMetricsCollector>(
            Profile::FromBrowserContext(browser_context)->GetPrefs());
    metrics_collector_ = metrics_collector.get();
    return std::move(metrics_collector);
  }

 protected:
  std::unique_ptr<SafeBrowsingHandler> module_handler_;
  // Unowned MockSafeBrowsingMetricsCollector
  raw_ptr<MockSafeBrowsingMetricsCollector, DanglingUntriaged>
      metrics_collector_;
  // Unowned PrefService. Use TestingPrefServiceSyncable
  // as that is what TestingProfileManager returns.
  raw_ptr<sync_preferences::TestingPrefServiceSyncable, DanglingUntriaged>
      pref_service_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  TestingProfileManager manager_;
  mojo::PendingRemote<ntp::safe_browsing::mojom::SafeBrowsingHandler>
      safe_browsing_handler_remote_;
};

TEST_F(SafeBrowsingHandlerTest, CanShowModule_DoesNotShowForManaged) {
  pref_service_->SetManagedPref(prefs::kSafeBrowsingEnabled,
                                std::make_unique<base::Value>(true));
  pref_service_->SetManagedPref(prefs::kSafeBrowsingEnhanced,
                                std::make_unique<base::Value>(true));
  base::MockCallback<SafeBrowsingHandler::CanShowModuleCallback> callback;
  EXPECT_CALL(callback, Run(false)).Times(1);
  module_handler_->CanShowModule(callback.Get());
}

TEST_F(SafeBrowsingHandlerTest, CanShowModule_DoesNotShowForEnhanced) {
  SetSafeBrowsingState(pref_service_, SafeBrowsingState::ENHANCED_PROTECTION);
  base::MockCallback<SafeBrowsingHandler::CanShowModuleCallback> callback;
  EXPECT_CALL(callback, Run(false)).Times(1);
  module_handler_->CanShowModule(callback.Get());
}

TEST_F(SafeBrowsingHandlerTest, CanShowModule_Succeeds) {
  SetSafeBrowsingState(pref_service_, SafeBrowsingState::STANDARD_PROTECTION);
  base::MockCallback<SafeBrowsingHandler::CanShowModuleCallback> callback;
  EXPECT_CALL(*metrics_collector_, GetLatestSecuritySensitiveEventTimestamp())
      .Times(1)
      .WillOnce(testing::Return(base::Time::Now()));
  EXPECT_CALL(callback, Run(true)).Times(1);
  module_handler_->CanShowModule(callback.Get());
}

TEST_F(SafeBrowsingHandlerTest, ProcessModuleClick_DisablesModule) {
  SetSafeBrowsingState(pref_service_, SafeBrowsingState::STANDARD_PROTECTION);
  base::MockCallback<SafeBrowsingHandler::CanShowModuleCallback> callback;
  EXPECT_CALL(*metrics_collector_, GetLatestSecuritySensitiveEventTimestamp())
      .Times(1)
      .WillOnce(testing::Return(base::Time::Now()));
  EXPECT_CALL(callback, Run(true)).Times(1);
  module_handler_->CanShowModule(callback.Get());

  module_handler_->ProcessModuleClick();

  EXPECT_CALL(callback, Run(false)).Times(1);
  module_handler_->CanShowModule(callback.Get());
}

TEST_F(SafeBrowsingHandlerTest, DismissRestore_DisablesModuleInBetween) {
  SetSafeBrowsingState(pref_service_, SafeBrowsingState::STANDARD_PROTECTION);
  base::MockCallback<SafeBrowsingHandler::CanShowModuleCallback> callback;
  module_handler_->DismissModule();

  EXPECT_CALL(callback, Run(false)).Times(1);
  module_handler_->CanShowModule(callback.Get());

  module_handler_->RestoreModule();
  EXPECT_CALL(*metrics_collector_, GetLatestSecuritySensitiveEventTimestamp())
      .Times(1)
      .WillOnce(testing::Return(base::Time::Now()));
  EXPECT_CALL(callback, Run(true)).Times(1);
  module_handler_->CanShowModule(callback.Get());
}

TEST_F(SafeBrowsingHandlerTest, Cooldown_DisablesModuleInBetween) {
  SetSafeBrowsingState(pref_service_, SafeBrowsingState::STANDARD_PROTECTION);
  base::MockCallback<SafeBrowsingHandler::CanShowModuleCallback> callback;
  int module_shown_count_max = 5;
  double cooldown_days = 30.0;
  base::Time initial_security_sensitive_event_time = base::Time::Now();

  base::test::ScopedFeatureList feature_list;
  base::test::FeatureRefAndParams ntp_module_feature_params(
      ntp_features::kNtpSafeBrowsingModule,
      {{ntp_features::kNtpSafeBrowsingModuleCooldownPeriodDaysParam,
        base::NumberToString(cooldown_days)},
       {ntp_features::kNtpSafeBrowsingModuleCountMaxParam,
        base::NumberToString(module_shown_count_max)}});
  feature_list.InitWithFeaturesAndParameters({ntp_module_feature_params}, {});

  // Repeatedly show module to enter cooldown.
  EXPECT_CALL(*metrics_collector_, GetLatestSecuritySensitiveEventTimestamp())
      .Times(module_shown_count_max)
      .WillRepeatedly(testing::Return(initial_security_sensitive_event_time));
  EXPECT_CALL(callback, Run(true)).Times(module_shown_count_max);
  for (int i = 0; i < module_shown_count_max; i++) {
    module_handler_->CanShowModule(callback.Get());
  }

  // Entered cooldown.
  EXPECT_CALL(callback, Run(false)).Times(1);
  module_handler_->CanShowModule(callback.Get());

  // Jump forward in time.
  task_environment_.FastForwardBy(base::Days(static_cast<int>(cooldown_days)));

  // Exit cooldown, but no new security sensitive event. Module is still not
  // shown.
  EXPECT_CALL(*metrics_collector_, GetLatestSecuritySensitiveEventTimestamp())
      .Times(1)
      .WillOnce(testing::Return(initial_security_sensitive_event_time));
  EXPECT_CALL(callback, Run(false)).Times(1);
  module_handler_->CanShowModule(callback.Get());

  // New security sensitive event, should work now.
  EXPECT_CALL(*metrics_collector_, GetLatestSecuritySensitiveEventTimestamp())
      .Times(1)
      .WillOnce(testing::Return(base::Time::Now()));
  EXPECT_CALL(callback, Run(true)).Times(1);
  module_handler_->CanShowModule(callback.Get());
}

}  // namespace ntp
