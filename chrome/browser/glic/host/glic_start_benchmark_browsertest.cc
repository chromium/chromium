// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include <tuple>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

// Owns the HTTP/2 server used to host the guest. Kept in a base class that is
// declared before GlicBrowserTestMixin so it outlives the mixin's
// GlicTestEnvironment, whose EmbeddedTestServerHandle shuts the server down.
class GlicBenchmarkH2ServerHolder {
 protected:
  net::test_server::EmbeddedTestServer h2_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS,
      net::test_server::HttpConnection::Protocol::kHttp2};

  // A second HTTPS server, on its own port, used only to warm the network
  // service before the first timed open. It must be a different origin from
  // the guest so the guest's first navigation still pays DNS/TCP/TLS: the TLS
  // session cache is keyed by host:port, and HTTP/1 connections cannot be
  // pooled with the guest's HTTP/2 session.
  net::test_server::EmbeddedTestServer warmup_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

class GlicInitializationBenchmark
    : public GlicBenchmarkH2ServerHolder,
      public GlicBrowserTestMixin<PlatformBrowserTest>,
      public testing::WithParamInterface<bool> {
 public:
  GlicInitializationBenchmark() {
    // Serve the guest over HTTP/2. The default HTTP/1 EmbeddedTestServer
    // closes the connection after every response, so each script fetch pays a
    // full TCP+TLS handshake serialized on the test server's single IO thread
    // (which lives inside the browser process). Production
    // (gemini.google.com/glic, gemini.gstatic.com) serves the client over one
    // multiplexed H2 connection, so that cost is a harness artifact.
    set_glic_https_test_server(&h2_server_);

    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;

    if (IsNoWebview()) {
      enabled.push_back(features::kGlicNoWebview);
    } else {
      disabled.push_back(features::kGlicNoWebview);
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

  // Warm the network service (disk cache backend, cookie store, cert verifier,
  // first HttpNetworkSession) before the first timed open. Real users open
  // Glic in a browser that has already been talking to the network, so those
  // one-time costs are a harness artifact. The warm-up deliberately uses a
  // different origin from the guest: Chrome does not preconnect for Glic, so
  // the "cold" number must include the guest's own DNS/TCP/TLS setup.
  {
    warmup_server_.RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html");
          response->set_content("<!doctype html>warmup");
          return response;
        }));
    ASSERT_TRUE(warmup_server_.Start());
    ASSERT_NE(warmup_server_.port(), GetGuestURL().EffectiveIntPort());
    // A top-level navigation (not a browser-process fetch) so it exercises the
    // same renderer-initiated loader path as the guest.
    tabs::TabInterface* warmup_tab =
        GetTabListInterface()->OpenTab(warmup_server_.GetURL("/warmup"), -1);
    ASSERT_TRUE(warmup_tab);
    std::ignore = content::WaitForLoadStop(warmup_tab->GetContents());
    // Don't leave an extra renderer alive for the rest of the run.
    GetTabListInterface()->CloseTab(warmup_tab->GetHandle());
  }

  for (int i = 0; i < iterations; ++i) {
    tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
    ASSERT_TRUE(tab);

    // Navigating the new tab consumes the existing spare renderer and triggers
    // background creation of a replacement spare. Wait for that normal steady-
    // state spare renderer to finish launching before opening Glic.
    ASSERT_TRUE(base::test::RunUntil([]() {
      return std::ranges::any_of(
          content::SpareRenderProcessHostManager::Get().GetSpares(),
          [](content::RenderProcessHost* spare) {
            return spare && spare->IsReady();
          });
    }));

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

      // Dump navigation + resource timing so the renderer-side gap between
      // commit and script start can be attributed (fetch vs. parse/compile).
      content::EvalJsResult timing = content::EvalJs(guest_contents, R"js(
        (() => {
          const n = performance.getEntriesByType('navigation')[0];
          const nav = n ? {
            fetchStart: n.fetchStart, connectStart: n.connectStart,
            secureConnectionStart: n.secureConnectionStart,
            connectEnd: n.connectEnd, requestStart: n.requestStart,
            responseStart: n.responseStart, responseEnd: n.responseEnd,
            domInteractive: n.domInteractive,
            domContentLoaded: n.domContentLoadedEventEnd,
            loadEventEnd: n.loadEventEnd } : null;
          const res = performance.getEntriesByType('resource').map(e => ({
            name: e.name.split('/').pop().split('?')[0],
            type: e.initiatorType, start: Math.round(e.startTime),
            reqStart: Math.round(e.requestStart),
            respStart: Math.round(e.responseStart),
            end: Math.round(e.responseEnd), size: e.transferSize }));
          const marks = performance.getEntriesByType('mark').map(
            m => [m.name, Math.round(m.startTime)]);
          return JSON.stringify({nav, res, marks});
        })()
      )js");
      LOG(INFO) << "[" << mode_str << "] Guest timing iteration " << (i + 1)
                << ": " << timing;
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
