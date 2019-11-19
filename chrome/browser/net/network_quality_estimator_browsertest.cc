// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/features.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class TestNetworkQualityObserver
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  explicit TestNetworkQualityObserver(network::NetworkQualityTracker* tracker)
      : run_loop_wait_effective_connection_type_(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
        tracker_(tracker),
        effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    tracker_->AddEffectiveConnectionTypeObserver(this);
  }

  ~TestNetworkQualityObserver() override {
    tracker_->RemoveEffectiveConnectionTypeObserver(this);
  }

  // NetworkQualityTracker::EffectiveConnectionTypeObserver implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    net::EffectiveConnectionType queried_type =
        tracker_->GetEffectiveConnectionType();
    EXPECT_EQ(type, queried_type);

    effective_connection_type_ = type;
    if (effective_connection_type_ != run_loop_wait_effective_connection_type_)
      return;
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForNotification(
      net::EffectiveConnectionType run_loop_wait_effective_connection_type) {
    if (effective_connection_type_ == run_loop_wait_effective_connection_type)
      return;
    ASSERT_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
              run_loop_wait_effective_connection_type);
    run_loop_.reset(new base::RunLoop());
    run_loop_wait_effective_connection_type_ =
        run_loop_wait_effective_connection_type;
    run_loop_->Run();
    run_loop_.reset();
  }

 private:
  net::EffectiveConnectionType run_loop_wait_effective_connection_type_;
  std::unique_ptr<base::RunLoop> run_loop_;
  network::NetworkQualityTracker* tracker_;
  net::EffectiveConnectionType effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkQualityObserver);
};

void CheckEffectiveConnectionType(net::EffectiveConnectionType expected) {
  TestNetworkQualityObserver network_quality_observer(
      g_browser_process->network_quality_tracker());
  network_quality_observer.WaitForNotification(expected);
}

class NetworkQualityEstimatorBrowserTest : public InProcessBrowserTest {
 public:
  NetworkQualityEstimatorBrowserTest() {}
  ~NetworkQualityEstimatorBrowserTest() override {}

  void SetUp() override {
    // Must start listening (And get a port for the proxy) before calling
    // SetUp(). Use two phase EmbeddedTestServer setup for proxy tests.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDown() override {
    // Need to stop this before |connection_listener_| is destroyed.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDown();
  }
};

class NetworkQualityEstimatorEctCommandLineBrowserTest
    : public NetworkQualityEstimatorBrowserTest {
 public:
  NetworkQualityEstimatorEctCommandLineBrowserTest() {}
  ~NetworkQualityEstimatorEctCommandLineBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("--force-effective-connection-type",
                                    "Slow-2G");
  }
};

IN_PROC_BROWSER_TEST_F(NetworkQualityEstimatorEctCommandLineBrowserTest,
                       ForceECTFromCommandLine) {
  CheckEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
}

class NetworkQualityEstimatorEctFieldTrialBrowserTest
    : public NetworkQualityEstimatorBrowserTest {
 public:
  NetworkQualityEstimatorEctFieldTrialBrowserTest() {}
  ~NetworkQualityEstimatorEctFieldTrialBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;
    variation_params["force_effective_connection_type"] = "2G";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kNetworkQualityEstimator, variation_params);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkQualityEstimatorEctFieldTrialBrowserTest,
                       ForceECTUsingFieldTrial) {
  CheckEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_2G);
}

class NetworkQualityEstimatorEctFieldTrialAndCommandLineBrowserTest
    : public NetworkQualityEstimatorEctFieldTrialBrowserTest {
 public:
  NetworkQualityEstimatorEctFieldTrialAndCommandLineBrowserTest() {}
  ~NetworkQualityEstimatorEctFieldTrialAndCommandLineBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    NetworkQualityEstimatorEctFieldTrialBrowserTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII("--force-effective-connection-type",
                                    "Slow-2G");
  }
};

IN_PROC_BROWSER_TEST_F(
    NetworkQualityEstimatorEctFieldTrialAndCommandLineBrowserTest,
    ECTFromCommandLineOverridesFieldTrial) {
  CheckEffectiveConnectionType(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
}

}  // namespace
