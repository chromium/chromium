// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/metrics_reporting_observer.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockMetricsServiceProxy
    : public MetricsReportingObserver::MetricsServiceProxy {
 public:
  MOCK_METHOD(void, SetReportingEnabled, (bool enabled), (override));
  MOCK_METHOD(void, SetExternalClientId, (const std::string&), (override));
  MOCK_METHOD(void, RecreateClientIdIfNecessary, (), (override));
};

class MetricsReportingObserverTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::unique_ptr<MetricsReportingObserver::MetricsServiceProxy>
        metrics_service = std::make_unique<MockMetricsServiceProxy>();
    mock_metrics_service_ =
        static_cast<MockMetricsServiceProxy*>(metrics_service.get());
    observer_ =
        std::make_unique<MetricsReportingObserver>(std::move(metrics_service));
  }

  MetricsReportingObserver* observer() { return observer_.get(); }

  MockMetricsServiceProxy* mock_metrics_service() {
    return mock_metrics_service_;
  }

 private:
  raw_ptr<MockMetricsServiceProxy> mock_metrics_service_;
  std::unique_ptr<MetricsReportingObserver> observer_;
};

TEST_F(MetricsReportingObserverTest, EnablingMetricsReporting) {
  std::string test_id = "my little id";

  EXPECT_CALL(*mock_metrics_service(),
              SetExternalClientId(testing::StrEq(test_id)));
  EXPECT_CALL(*mock_metrics_service(), SetReportingEnabled(testing::IsTrue()));
  EXPECT_CALL(*mock_metrics_service(), RecreateClientIdIfNecessary);

  observer()->OnMetricsReportingChanged(true, test_id);
}

TEST_F(MetricsReportingObserverTest, DisablingMetricsReporting) {
  EXPECT_CALL(*mock_metrics_service(), SetExternalClientId).Times(0);
  EXPECT_CALL(*mock_metrics_service(), SetReportingEnabled(testing::IsFalse()));
  EXPECT_CALL(*mock_metrics_service(), RecreateClientIdIfNecessary).Times(0);

  observer()->OnMetricsReportingChanged(false, absl::nullopt);
}

TEST_F(MetricsReportingObserverTest, DisablingMetricsReportingWithClientId) {
  // Attempting to set the client id on disable should result in a noop.
  EXPECT_CALL(*mock_metrics_service(), SetExternalClientId).Times(0);
  EXPECT_CALL(*mock_metrics_service(), SetReportingEnabled(testing::IsFalse()));
  EXPECT_CALL(*mock_metrics_service(), RecreateClientIdIfNecessary).Times(0);

  observer()->OnMetricsReportingChanged(false, absl::nullopt);
}
