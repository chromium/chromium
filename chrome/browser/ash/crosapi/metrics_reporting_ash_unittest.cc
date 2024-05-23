// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/metrics_reporting_ash.h"

#include <memory>

#include "base/callback_list.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

class TestObserver : public mojom::MetricsReportingObserver {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  // crosapi::mojom::MetricsReportingObserver:
  void OnMetricsReportingChanged(
      bool enabled,
      const std::optional<std::string>& client_id) override {
    metrics_enabled_ = enabled;
  }

  // Public because this is test code.
  std::optional<bool> metrics_enabled_;
  mojo::Receiver<mojom::MetricsReportingObserver> receiver_{this};
};

class TestDelegate : public MetricsReportingAsh::Delegate {
 public:
  TestDelegate() = default;
  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;
  ~TestDelegate() override = default;

  bool IsMetricsReportingEnabled() override { return metrics_enabled_; }

  // MetricsReportingAsh::Delegate:
  void SetMetricsReportingEnabled(bool enabled) override {
    metrics_enabled_ = enabled;
    observers_.Notify(metrics_enabled_);
  }

  std::string GetClientId() override { return client_id_; }

  base::CallbackListSubscription AddEnablementObserver(
      const base::RepeatingCallback<void(bool)>& observer) override {
    return observers_.Add(observer);
  }

  std::string client_id_;
  bool metrics_enabled_ = false;
  base::RepeatingCallbackList<void(bool)> observers_;
};

class MetricsReportingAshTest : public testing::Test {
 public:
  MetricsReportingAshTest() = default;
  MetricsReportingAshTest(const MetricsReportingAshTest&) = delete;
  MetricsReportingAshTest& operator=(const MetricsReportingAshTest&) = delete;
  ~MetricsReportingAshTest() override = default;

  base::test::TaskEnvironment task_environment_;
};

TEST_F(MetricsReportingAshTest, Basics) {
  auto delegate = std::make_unique<TestDelegate>();
  TestDelegate* test_delegate = delegate.get();

  // Simulate metrics reporting enabled.
  test_delegate->SetMetricsReportingEnabled(true);

  // Construct the object under test.
  MetricsReportingAsh metrics_reporting_ash(std::move(delegate));
  mojo::Remote<mojom::MetricsReporting> metrics_reporting_remote;
  metrics_reporting_ash.BindReceiver(
      metrics_reporting_remote.BindNewPipeAndPassReceiver());

  // Adding an observer results in it being fired with the current state.
  TestObserver observer;
  metrics_reporting_remote->AddObserver(
      observer.receiver_.BindNewPipeAndPassRemote());
  metrics_reporting_remote.FlushForTesting();
  ASSERT_TRUE(observer.metrics_enabled_.has_value());
  EXPECT_TRUE(observer.metrics_enabled_.value());

  // Disabling metrics reporting in ash fires the observer with the new value.
  observer.metrics_enabled_.reset();
  test_delegate->SetMetricsReportingEnabled(false);
  observer.receiver_.FlushForTesting();
  ASSERT_TRUE(observer.metrics_enabled_.has_value());
  EXPECT_FALSE(observer.metrics_enabled_.value());
}

TEST_F(MetricsReportingAshTest, SetMetricsReportingEnabled) {
  // Construct the object with a test delegate.
  auto delegate = std::make_unique<TestDelegate>();
  TestDelegate* test_delegate = delegate.get();

  // Simulate metrics reporting disabled.
  test_delegate->SetMetricsReportingEnabled(false);

  MetricsReportingAsh metrics_reporting_ash(std::move(delegate));
  mojo::Remote<mojom::MetricsReporting> metrics_reporting_remote;
  metrics_reporting_ash.BindReceiver(
      metrics_reporting_remote.BindNewPipeAndPassReceiver());

  // Calling SetMetricsReportingEnabled() over mojo calls through to the metrics
  // reporting subsystem.
  base::test::TestFuture<void> waiter;
  metrics_reporting_remote->SetMetricsReportingEnabled(true,
                                                       waiter.GetCallback());
  EXPECT_TRUE(waiter.Wait());
  EXPECT_TRUE(test_delegate->metrics_enabled_);
}

}  // namespace crosapi
