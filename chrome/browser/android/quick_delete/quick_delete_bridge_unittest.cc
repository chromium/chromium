// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/quick_delete/quick_delete_bridge.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time_override.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace history {

class MockHistoryService : public history::HistoryService {
 public:
  MockHistoryService() = default;

  MOCK_METHOD(void,
              GetUniqueDomainsVisited,
              (const base::Time begin_time,
               const base::Time end_time,
               GetUniqueDomainsVisitedCallback callback,
               base::CancelableTaskTracker* tracker),
              (override));
};

}  // namespace history

std::unique_ptr<KeyedService> buildHistoryServiceMock(
    content::BrowserContext* context) {
  return std::make_unique<::testing::NiceMock<history::MockHistoryService>>();
}

class QuickDeleteBridgeTest : public testing::Test {
 public:
  QuickDeleteBridgeTest() : env_(base::android::AttachCurrentThread()) {}

  ~QuickDeleteBridgeTest() override = default;

  void SetUp() override {
    mock_history_service_ = static_cast<history::MockHistoryService*>(
        HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, base::BindRepeating(&buildHistoryServiceMock)));

    bridge_ = std::make_unique<QuickDeleteBridge>(&profile_);
  }

  QuickDeleteBridge* bridge() { return bridge_.get(); }

  JNIEnv* env() { return env_; }

  history::MockHistoryService* history_service() {
    return mock_history_service_;
  }

  static base::Time OverrideTimeNow() { return override_time_now_; }

  static void SetOverrideTimeNow(base::Time override_now) {
    override_time_now_ = override_now;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<JNIEnv> env_;
  raw_ptr<history::MockHistoryService> mock_history_service_;
  std::unique_ptr<QuickDeleteBridge> bridge_;
  TestingProfile profile_;
  static base::Time override_time_now_;
};

base::Time QuickDeleteBridgeTest::override_time_now_;

TEST_F(QuickDeleteBridgeTest, GetLastVisitedDomainAndUniqueDomainCount) {
  SetOverrideTimeNow(base::Time::Now());

  base::subtle::ScopedTimeClockOverrides time_override(
      &QuickDeleteBridgeTest::OverrideTimeNow,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr);

  const base::Time expected_begin_time = OverrideTimeNow() - base::Minutes(15);
  const base::Time expected_end_time = base::Time::Max();

  EXPECT_CALL(
      *history_service(),
      GetUniqueDomainsVisited(expected_begin_time, expected_end_time, _, _))
      .Times(1);

  bridge()->GetLastVisitedDomainAndUniqueDomainCount(
      env(), static_cast<jint>(browsing_data::TimePeriod::LAST_15_MINUTES),
      JavaParamRef<jobject>(nullptr));
}
