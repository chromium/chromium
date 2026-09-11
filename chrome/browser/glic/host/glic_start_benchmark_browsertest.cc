// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class GlicInitializationBenchmark
    : public GlicBrowserTestMixin<PlatformBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  GlicInitializationBenchmark() {
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;

    SetUseHttpsForGlicUrl(true);
    if (IsNoWebview()) {
      enabled.push_back(features::kGlicNoWebview);
      enabled.push_back(pwc::mojom::features::kPrivilegedWebContents);
    } else {
      disabled.push_back(features::kGlicNoWebview);
      disabled.push_back(pwc::mojom::features::kPrivilegedWebContents);
    }

    feature_list_.InitWithFeatures(enabled, disabled);
  }

  bool IsNoWebview() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicInitializationBenchmark, MeasureInitializationTime) {
  int iterations = 3;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          "glic-benchmark-iterations")) {
    int parsed = 0;
    if (base::StringToInt(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                "glic-benchmark-iterations"),
            &parsed) &&
        parsed > 0) {
      iterations = parsed;
    }
  }
  std::vector<double> init_times_ms;
  std::vector<double> script_start_times_ms;
  std::vector<double> bootstrap_received_times_ms;
  std::vector<double> client_init_times_ms;
  std::vector<double> panel_opened_times_ms;
  std::vector<double> script_to_init_times_ms;
  std::vector<double> init_to_open_times_ms;

  std::string mode_str = IsNoWebview() ? "NoWebview" : "Webview";

  LOG(INFO) << "=== Starting Glic Initialization Benchmark (" << mode_str
            << ", " << iterations << " iterations) ===";

  for (int i = 0; i < iterations; ++i) {
    tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
    ASSERT_TRUE(tab);

    base::ElapsedTimer timer;
    auto open_result = OpenGlicForActiveTab();
    ASSERT_OK(open_result);
    GlicInstanceImpl* instance = *open_result;
    ASSERT_TRUE(instance);

    ASSERT_OK(WaitForGlicClient(instance));
    base::TimeDelta elapsed = timer.Elapsed();
    double elapsed_ms = elapsed.InMillisecondsF();
    init_times_ms.push_back(elapsed_ms);

    double script_start_ms = 0.0;
    double bootstrap_received_ms = 0.0;
    double client_init_ms = 0.0;
    double panel_opened_ms = 0.0;
    double script_to_init_ms = 0.0;
    double init_to_open_ms = 0.0;

    content::WebContents* guest_contents =
        instance->host().web_client_contents();
    if (guest_contents) {
      auto get_mark = [&](const std::string& name) {
        std::string js =
            "performance.getEntriesByName('" + name + "')[0]?.startTime || 0";
        content::EvalJsResult result = content::EvalJs(guest_contents, js);
        return result.is_ok() ? result.ExtractDouble() : 0.0;
      };
      script_start_ms = get_mark("glic-client-main-script-start");
      bootstrap_received_ms = get_mark("glic-client-bootstrap-received");
      client_init_ms = get_mark("glic-client-initialized");
      panel_opened_ms = get_mark("glic-client-panel-opened");
      if (script_start_ms > 0.0 && client_init_ms > 0.0) {
        script_to_init_ms = client_init_ms - script_start_ms;
      }
      if (client_init_ms > 0.0 && panel_opened_ms > 0.0) {
        init_to_open_ms = panel_opened_ms - client_init_ms;
      }
    }
    script_start_times_ms.push_back(script_start_ms);
    bootstrap_received_times_ms.push_back(bootstrap_received_ms);
    client_init_times_ms.push_back(client_init_ms);
    panel_opened_times_ms.push_back(panel_opened_ms);
    script_to_init_times_ms.push_back(script_to_init_ms);
    init_to_open_times_ms.push_back(init_to_open_ms);

    LOG(INFO) << "[" << mode_str << "] Iteration " << (i + 1) << "/"
              << iterations << ": total_init=" << elapsed_ms << " ms"
              << ", script_start=" << script_start_ms << " ms"
              << ", bootstrap_received=" << bootstrap_received_ms << " ms"
              << ", client_init=" << client_init_ms << " ms"
              << ", panel_opened=" << panel_opened_ms << " ms"
              << ", script->init=" << script_to_init_ms << " ms"
              << ", init->open=" << init_to_open_ms << " ms";

    ASSERT_OK(CloseGlicForTabAndWait(tab));
  }

  double sum_init =
      std::accumulate(init_times_ms.begin(), init_times_ms.end(), 0.0);
  double mean_init = sum_init / init_times_ms.size();
  double min_init =
      *std::min_element(init_times_ms.begin(), init_times_ms.end());
  double max_init =
      *std::max_element(init_times_ms.begin(), init_times_ms.end());

  double sum_sq_diff = 0.0;
  for (double t : init_times_ms) {
    sum_sq_diff += (t - mean_init) * (t - mean_init);
  }
  double stddev_init = std::sqrt(sum_sq_diff / init_times_ms.size());

  auto calc_mean = [](const std::vector<double>& v) {
    return v.empty() ? 0.0
                     : std::accumulate(v.begin(), v.end(), 0.0) / v.size();
  };
  double mean_script_start = calc_mean(script_start_times_ms);
  double mean_bootstrap_received = calc_mean(bootstrap_received_times_ms);
  double mean_client_init = calc_mean(client_init_times_ms);
  double mean_panel_opened = calc_mean(panel_opened_times_ms);
  double mean_script_to_init = calc_mean(script_to_init_times_ms);
  double mean_init_to_open = calc_mean(init_to_open_times_ms);

  LOG(INFO) << "==========================================================";
  LOG(INFO) << "RESULTS for " << mode_str << ":";
  LOG(INFO) << "  Mean init time:          " << mean_init
            << " ms (stddev: " << stddev_init << " ms)";
  LOG(INFO) << "  Min / Max:               " << min_init << " ms / " << max_init
            << " ms";
  LOG(INFO) << "  Mean script start:       " << mean_script_start << " ms";
  LOG(INFO) << "  Mean bootstrap received: " << mean_bootstrap_received
            << " ms";
  LOG(INFO) << "  Mean client initialized: " << mean_client_init << " ms";
  LOG(INFO) << "  Mean panel opened:       " << mean_panel_opened << " ms";
  LOG(INFO) << "  Mean script->init:       " << mean_script_to_init << " ms";
  LOG(INFO) << "  Mean init->open:         " << mean_init_to_open << " ms";
  LOG(INFO) << "==========================================================";
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicInitializationBenchmark,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace glic
