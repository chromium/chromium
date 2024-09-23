// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/field_trial_param_associator.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_quality_tracker.h"

// Tests if the save data header holdback works as expected.
class NetInfoNetworkQualityEstimatorHoldbackBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<bool> {
 protected:
  NetInfoNetworkQualityEstimatorHoldbackBrowserTest() {
    ConfigureHoldbackExperiment();
  }

  void SetUp() override {
    test_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    ASSERT_TRUE(test_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  void VerifyNetworkQualityNetInfoWebAPI(
      const std::string& expected_effective_connection_type) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), test_server_.GetURL("/net_info.html")));
    content::NavigationEntry* entry =
        GetWebContents()->GetController().GetVisibleEntry();
    EXPECT_EQ(content::PAGE_TYPE_NORMAL, entry->GetPageType());

    EXPECT_EQ(expected_effective_connection_type,
              RunScriptExtractString("getEffectiveType()"));

    if (expected_effective_connection_type == "slow-2g") {
      VerifyRtt(base::Milliseconds(3600), RunScriptExtractDouble("getRtt()"));
      VerifyDownlinkKbps(40, RunScriptExtractDouble("getDownlink()") * 1000);
    } else if (expected_effective_connection_type == "2g") {
      VerifyRtt(base::Milliseconds(1800), RunScriptExtractDouble("getRtt()"));
      VerifyDownlinkKbps(75, RunScriptExtractDouble("getDownlink()") * 1000);
    } else if (expected_effective_connection_type == "3g") {
      VerifyRtt(base::Milliseconds(450), RunScriptExtractDouble("getRtt()"));
      VerifyDownlinkKbps(400, RunScriptExtractDouble("getDownlink()") * 1000);
    } else if (expected_effective_connection_type == "4g") {
      VerifyRtt(base::Milliseconds(175), RunScriptExtractDouble("getRtt()"));
      VerifyDownlinkKbps(1600, RunScriptExtractDouble("getDownlink()") * 1000);
    } else {
      DCHECK(false);
    }
  }

  void ConfigureHoldbackExperiment() {
    std::map<std::string, std::string> params;
    if (GetParam()) {
      params["web_effective_connection_type_override"] = "2G";
    }
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kNetworkQualityEstimatorWebHoldback, params);
  }

  // Simulates a network quality change.
  void SimulateNetworkQualityChange(net::EffectiveConnectionType type) {
    g_browser_process->network_quality_tracker()
        ->ReportEffectiveConnectionTypeForTesting(type);

    // Values taken from net/nqe/network_quality_estimator_params.h.
    // TODO(tbansal): Declare the values in a common place, and read
    // them directly.
    if (type == net::EFFECTIVE_CONNECTION_TYPE_3G) {
      g_browser_process->network_quality_tracker()
          ->ReportRTTsAndThroughputForTesting(base::Milliseconds(450), 400);
    } else if (type == net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G) {
      g_browser_process->network_quality_tracker()
          ->ReportRTTsAndThroughputForTesting(base::Milliseconds(3600), 40);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

 private:
  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void VerifyRtt(base::TimeDelta expected_rtt, int32_t got_rtt_milliseconds) {
    EXPECT_EQ(0, got_rtt_milliseconds % 50)
        << " got_rtt_milliseconds=" << got_rtt_milliseconds;

    if (expected_rtt > base::Milliseconds(3000))
      expected_rtt = base::Milliseconds(3000);

    // The difference between the actual and the estimate value should be within
    // 10%. Add 50 (bucket size used in Blink) to account for the cases when the
    // sample may spill over to the next bucket due to the added noise of 10%.
    // For example, if sample is 300 msec, after adding noise, it may become
    // 330, and after rounding off, it would spill over to the next bucket of
    // 350 msec.
    EXPECT_GE((expected_rtt.InMilliseconds() * 0.1) + 50,
              std::abs(expected_rtt.InMilliseconds() - got_rtt_milliseconds));
  }

  void VerifyDownlinkKbps(double expected_kbps, double got_kbps) {
    // First verify that |got_kbps| is a multiple of 50.
    int quotient = static_cast<int>(got_kbps / 50);
    // |mod| is the remainder left after dividing |got_kbps| by 50 while
    // restricting the quotient to integer.  For example, if |got_kbps| is
    // 1050, then mod will be 0. If |got_kbps| is 1030, mod will be 30.
    double mod = got_kbps - 50 * quotient;
    EXPECT_LE(0.0, mod);
    EXPECT_GT(50.0, mod);
    // It is possible that |mod| is not exactly 0 because of floating point
    // computations. e.g., |got_kbps| may be 99.999999, in which case |mod|
    // will be 49.999999.
    EXPECT_TRUE(mod < (1e-5) || (50 - mod) < 1e-5) << " got_kbps=" << got_kbps;

    if (expected_kbps > 10000)
      expected_kbps = 10000;

    // The difference between the actual and the estimate value should be within
    // 10%. Add 50 (bucket size used in Blink) to account for the cases when the
    // sample may spill over to the next bucket due to the added noise of 10%.
    // For example, if sample is 300 kbps, after adding noise, it may become
    // 330, and after rounding off, it would spill over to the next bucket of
    // 350 kbps.
    EXPECT_GE((expected_kbps * 0.1) + 50, std::abs(expected_kbps - got_kbps));
  }

  std::string RunScriptExtractString(const std::string& script) {
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           script)
        .ExtractString();
  }

  double RunScriptExtractDouble(const std::string& script) {
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           script)
        .ExtractDouble();
  }

  int RunScriptExtractInt(const std::string& script) {
    return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           script)
        .ExtractInt();
  }

  net::EmbeddedTestServer test_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Make sure the changes in the effective connection typeare notified to the
// render thread.
IN_PROC_BROWSER_TEST_P(NetInfoNetworkQualityEstimatorHoldbackBrowserTest,
                       EffectiveConnectionTypeChangeNotified) {
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_3G);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/net_info.html")));

  if (GetParam()) {
    // ConfigureHoldbackExperiment() sets holdback ECT to 2G.
    VerifyNetworkQualityNetInfoWebAPI("2g");
  } else {
    VerifyNetworkQualityNetInfoWebAPI("3g");
  }
  SimulateNetworkQualityChange(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  base::RunLoop().RunUntilIdle();

  if (GetParam()) {
    // ConfigureHoldbackExperiment() sets holdback ECT to 2G.
    VerifyNetworkQualityNetInfoWebAPI("2g");
  } else {
    VerifyNetworkQualityNetInfoWebAPI("slow-2g");
  }
}

// The network quality estimator web holdback is enabled only if the first
// param is true.
INSTANTIATE_TEST_SUITE_P(All,
                         NetInfoNetworkQualityEstimatorHoldbackBrowserTest,
                         testing::Bool());
