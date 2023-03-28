// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/test/test_network_quality_tracker.h"

namespace {

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
  // For example, if sample is 300 kbps, after adding noise, it may become 330,
  // and after rounding off, it would spill over to the next bucket of 350 kbps.
  EXPECT_GE((expected_kbps * 0.1) + 50, std::abs(expected_kbps - got_kbps))
      << " expected_kbps=" << expected_kbps << " got_kbps=" << got_kbps;
}

}  // namespace

class NetInfoBrowserTest : public InProcessBrowserTest {
 public:
  network::NetworkQualityTracker* GetNetworkQualityTracker() const {
    return g_browser_process->network_quality_tracker();
  }

 protected:
  void SetUpOnMainThread() override {}

  // Runs |script| repeatedly until the |script| returns |expected_value|.
  void RunScriptUntilExpectedStringValueMet(const std::string& script,
                                            const std::string& expected_value) {
    while (true) {
      if (RunScriptExtractString(script) == expected_value)
        return;
      base::RunLoop().RunUntilIdle();
    }
  }

  // Repeatedly runs NetInfo JavaScript API to get RTT until it returns
  // |expected_rtt|.
  void RunGetRttUntilExpectedValueReached(base::TimeDelta expected_rtt) {
    if (expected_rtt > base::Milliseconds(3000))
      expected_rtt = base::Milliseconds(3000);

    while (true) {
      int32_t got_rtt_milliseconds = RunScriptExtractInt("getRtt()");
      EXPECT_EQ(0, got_rtt_milliseconds % 50)
          << " got_rtt_milliseconds=" << got_rtt_milliseconds;

      // The difference between the actual and the estimate value should be
      // within 10%. Add 50 (bucket size used in Blink) to account for the cases
      // when the sample may spill over to the next bucket due to the added
      // noise of 10%. For example, if sample is 300 msec, after adding noise,
      // it may become 330, and after rounding off, it would spill over to the
      // next bucket of 350 msec.
      if (expected_rtt.InMilliseconds() * 0.1 + 50 >=
          std::abs(expected_rtt.InMilliseconds() - got_rtt_milliseconds)) {
        return;
      }
    }
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
};

// Make sure the changes in the effective connection type are notified to the
// render thread.
IN_PROC_BROWSER_TEST_F(NetInfoBrowserTest,
                       EffectiveConnectionTypeChangeNotfied) {
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  EXPECT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/net_info.html")));

  // Change effective connection type so that the renderer process is notified.
  // Changing the effective connection type from 2G to 3G is guaranteed to
  // generate the notification to the renderers, irrespective of the current
  // effective connection type.
  GetNetworkQualityTracker()->ReportEffectiveConnectionTypeForTesting(
      net::EFFECTIVE_CONNECTION_TYPE_2G);
  RunScriptUntilExpectedStringValueMet("getEffectiveType()", "2g");
  GetNetworkQualityTracker()->ReportEffectiveConnectionTypeForTesting(
      net::EFFECTIVE_CONNECTION_TYPE_3G);
  RunScriptUntilExpectedStringValueMet("getEffectiveType()", "3g");
}

// Make sure the changes in the network quality are notified to the render
// thread, and the changed network quality is accessible via Javascript API.
IN_PROC_BROWSER_TEST_F(NetInfoBrowserTest, NetworkQualityChangeNotified) {
  GetNetworkQualityTracker()->ReportRTTsAndThroughputForTesting(
      base::Seconds(1), 300);

  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  EXPECT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/net_info.html")));

  RunGetRttUntilExpectedValueReached(base::Seconds(1));
  // If the updated RTT is available via JavaScript, then downlink must have
  // been updated too.
  VerifyDownlinkKbps(300, RunScriptExtractDouble("getDownlink()") * 1000);

  // Verify that the network quality change is accessible via Javascript API.
  GetNetworkQualityTracker()->ReportRTTsAndThroughputForTesting(
      base::Seconds(10), 3000);
  RunGetRttUntilExpectedValueReached(base::Seconds(10));
  VerifyDownlinkKbps(3000, RunScriptExtractDouble("getDownlink()") * 1000);
}
