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
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_warming_pool.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/public/identity_manager/test_accounts.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_connection.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/signin/e2e_tests/signin_util.h"
#endif

namespace glic {

namespace {
#if !BUILDFLAG(IS_ANDROID)
constexpr base::FilePath::StringViewType kTestAccountFilePath =
    FILE_PATH_LITERAL(
        "chrome/browser/internal/resources/signin/test_accounts.json");
#endif

bool IsProdMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      "glic-benchmark-prod");
}

bool HasCustomUrl() {
  return IsProdMode() || base::CommandLine::ForCurrentProcess()->HasSwitch(
                             "glic-benchmark-url");
}

int GetBenchmarkIterations() {
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
  return iterations;
}

class HistogramDeltaTracker {
 public:
  explicit HistogramDeltaTracker(std::string histogram_name)
      : histogram_name_(std::move(histogram_name)) {
    if (base::HistogramBase* hist =
            base::StatisticsRecorder::FindHistogram(histogram_name_)) {
      auto snapshot = hist->SnapshotSamples();
      prev_count_ = snapshot->TotalCount();
      prev_sum_ = snapshot->sum();
    }
  }

  bool HasNewSamples() const {
    base::HistogramBase* hist =
        base::StatisticsRecorder::FindHistogram(histogram_name_);
    return hist && hist->SnapshotSamples()->TotalCount() > prev_count_;
  }

  std::optional<double> RecordDelta() {
    base::HistogramBase* hist =
        base::StatisticsRecorder::FindHistogram(histogram_name_);
    if (!hist) {
      return std::nullopt;
    }
    auto snapshot = hist->SnapshotSamples();
    int curr_count = snapshot->TotalCount();
    int64_t curr_sum = snapshot->sum();
    int delta_count = curr_count - prev_count_;
    if (delta_count <= 0) {
      return std::nullopt;
    }
    double val = static_cast<double>(curr_sum - prev_sum_) / delta_count;
    prev_count_ = curr_count;
    prev_sum_ = curr_sum;
    return val;
  }

 private:
  std::string histogram_name_;
  int prev_count_ = 0;
  int64_t prev_sum_ = 0;
};

struct GuestMarks {
  std::string opened_viewport = "n/a";
  double script_start_ms = 0.0;
  double bootstrap_received_ms = 0.0;
  double client_init_ms = 0.0;
  double panel_opened_ms = 0.0;
  double script_to_init_ms = 0.0;
  double init_to_open_ms = 0.0;
};

struct BenchmarkMetrics {
  std::vector<double> init_times_ms;
  std::vector<double> presentation_times_ms;
  std::vector<double> time_to_warmed_times_ms;
  std::vector<double> script_start_times_ms;
  std::vector<double> bootstrap_received_times_ms;
  std::vector<double> client_init_times_ms;
  std::vector<double> panel_opened_times_ms;
  std::vector<double> script_to_init_times_ms;
  std::vector<double> init_to_open_times_ms;

  void RecordGuestMarks(const GuestMarks& m) {
    script_start_times_ms.push_back(m.script_start_ms);
    bootstrap_received_times_ms.push_back(m.bootstrap_received_ms);
    client_init_times_ms.push_back(m.client_init_ms);
    panel_opened_times_ms.push_back(m.panel_opened_ms);
    script_to_init_times_ms.push_back(m.script_to_init_ms);
    init_to_open_times_ms.push_back(m.init_to_open_ms);
  }
};

std::string QueryViewportAndDomInfo(content::WebContents* contents) {
  if (!contents) {
    return "n/a";
  }
  auto res = content::EvalJs(
      contents,
      "`${window.innerWidth}x${window.innerHeight}(dom=${document."
      "querySelectorAll('*').length})`");
  return res.is_ok() ? res.ExtractString() : "n/a";
}

GuestMarks CollectAndLogGuestMarks(content::WebContents* guest_contents,
                                   const std::string& config_str,
                                   int iteration_num) {
  GuestMarks marks;
  if (!guest_contents) {
    return marks;
  }
  marks.opened_viewport = QueryViewportAndDomInfo(guest_contents);
  auto get_mark = [&](const std::string& name) {
    std::string js =
        "performance.getEntriesByName('" + name + "')[0]?.startTime || 0";
    content::EvalJsResult result = content::EvalJs(guest_contents, js);
    return result.is_ok() ? result.ExtractDouble() : 0.0;
  };
  marks.script_start_ms = get_mark("glic-client-main-script-start");
  marks.bootstrap_received_ms = get_mark("glic-client-bootstrap-received");
  marks.client_init_ms = get_mark("glic-client-initialized");
  marks.panel_opened_ms = get_mark("glic-client-panel-opened");
  if (marks.script_start_ms > 0.0 && marks.client_init_ms > 0.0) {
    marks.script_to_init_ms = marks.client_init_ms - marks.script_start_ms;
  }
  if (marks.client_init_ms > 0.0 && marks.panel_opened_ms > 0.0) {
    marks.init_to_open_ms = marks.panel_opened_ms - marks.client_init_ms;
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
  LOG(INFO) << "[" << config_str << "] Guest timing iteration " << iteration_num
            << ": " << timing;
  return marks;
}

void LogSummaryStats(const std::string& config_str, const BenchmarkMetrics& m) {
  auto calc_stats = [](const std::vector<double>& v) {
    if (v.empty()) {
      return std::make_tuple(0.0, 0.0, 0.0, 0.0);
    }
    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    double mean = sum / v.size();
    double min = *std::min_element(v.begin(), v.end());
    double max = *std::max_element(v.begin(), v.end());
    double sum_sq_diff = 0.0;
    for (double x : v) {
      sum_sq_diff += (x - mean) * (x - mean);
    }
    double stddev = std::sqrt(sum_sq_diff / v.size());
    return std::make_tuple(mean, stddev, min, max);
  };
  auto calc_mean = [](const std::vector<double>& v) {
    return v.empty() ? 0.0
                     : std::accumulate(v.begin(), v.end(), 0.0) / v.size();
  };

  auto [mean_init, stddev_init, min_init, max_init] =
      calc_stats(m.init_times_ms);
  auto [mean_pres, stddev_pres, min_pres, max_pres] =
      calc_stats(m.presentation_times_ms);

  LOG(INFO) << "==========================================================";
  LOG(INFO) << "RESULTS for " << config_str << ":";
  LOG(INFO) << "  Total init time:         " << mean_init
            << " ms (stddev: " << stddev_init << " ms, min: " << min_init
            << " ms, max: " << max_init << " ms)";
  LOG(INFO) << "  Panel presentation time: " << mean_pres
            << " ms (stddev: " << stddev_pres << " ms, min: " << min_pres
            << " ms, max: " << max_pres << " ms)";
  LOG(INFO) << "  Mean time to warmed:     "
            << calc_mean(m.time_to_warmed_times_ms) << " ms";
  LOG(INFO) << "  Mean script start:       "
            << calc_mean(m.script_start_times_ms) << " ms";
  LOG(INFO) << "  Mean bootstrap received: "
            << calc_mean(m.bootstrap_received_times_ms) << " ms";
  LOG(INFO) << "  Mean client initialized: "
            << calc_mean(m.client_init_times_ms) << " ms";
  LOG(INFO) << "  Mean panel opened:       "
            << calc_mean(m.panel_opened_times_ms) << " ms";
  LOG(INFO) << "  Mean script->init:       "
            << calc_mean(m.script_to_init_times_ms) << " ms";
  LOG(INFO) << "  Mean init->open:         "
            << calc_mean(m.init_to_open_times_ms) << " ms";
  LOG(INFO) << "==========================================================";
}
}  // namespace

struct BenchmarkParams {
  bool is_no_webview;
  bool is_warmed;
};

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
      public testing::WithParamInterface<BenchmarkParams> {
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

#if BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID)
    // Mobile Android does not warm a spare renderer by default in production
    // (neither kSitePerProcess nor kAndroidWarmUpSpareRendererWithTimeout is
    // enabled by default). Disable the fieldtrial_testing_config.json study so
    // the benchmark measures Mobile Android's real no-spare behavior rather
    // than warming a one-off spare after the warmup tab loads.
    disabled.push_back(::features::kAndroidWarmUpSpareRendererWithTimeout);
#endif

    if (IsProdMode()) {
      // Use real Gaia sign-in with the OTA rather than fake IdentityManager.
      glic_test_environment().SetForceSigninAndModelExecutionCapability(false);
    }

    feature_list_.InitWithFeatures(enabled, disabled);
  }

  void SetUpInProcessBrowserTestFixture() override {
    if (HasCustomUrl()) {
      host_resolver()->AllowDirectLookup("*");
    }
    GlicBrowserTestMixin<
        PlatformBrowserTest>::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTestMixin<PlatformBrowserTest>::SetUpCommandLine(command_line);
    if (command_line->HasSwitch("glic-benchmark-url")) {
      command_line->RemoveSwitch(::switches::kGlicGuestURL);
      command_line->AppendSwitchASCII(
          ::switches::kGlicGuestURL,
          command_line->GetSwitchValueASCII("glic-benchmark-url"));
    } else if (IsProdMode()) {
      command_line->RemoveSwitch(::switches::kGlicGuestURL);
    }
  }

  bool IsNoWebview() const { return GetParam().is_no_webview; }
  bool IsWarmed() const { return GetParam().is_warmed; }

  std::string GetConfigName() const {
    return base::StrCat({IsNoWebview() ? "NoWebview" : "Webview", "_",
                         IsWarmed() ? "Warmed" : "Cold"});
  }

  void SignInForProdModeIfNeeded() {
#if !BUILDFLAG(IS_ANDROID)
    if (!IsProdMode()) {
      return;
    }
    signin::TestAccountsConfig test_accounts;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      base::FilePath root_path;
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root_path);
      ASSERT_TRUE(test_accounts.Init(root_path.Append(kTestAccountFilePath)));
    }
    std::optional<signin::TestAccountSigninCredentials> test_account =
        test_accounts.GetAccount("GLIC_ACCOUNT");
    ASSERT_TRUE(test_account.has_value());
    LOG(INFO) << "Signing in with OTA account: " << test_account->user;

    auto* service = GlicKeyedService::Get(GetProfile());
    ASSERT_TRUE(service);
    if (auto* auth_controller = service->GetAuthController()) {
      auth_controller->SetCookieSynchronizerForTesting(
          std::make_unique<GlicCookieSynchronizer>(
              GetProfile(),
              IdentityManagerFactory::GetForProfile(GetProfile())));
    }

    signin::test::SignInFunctions sign_in_functions(
        base::BindLambdaForTesting(
            [this]() -> BrowserWindowInterface* { return this->GetBrowser(); }),
        base::BindLambdaForTesting(
            [this](int index, const GURL& url,
                   ui::PageTransition transition) -> bool {
              return this->AddTabAtIndex(index, url, transition);
            }));
    sign_in_functions.SignInFromSettingsWithSyncChoice(
        *test_account, 0,
        signin::test::SignInFunctions::SyncChoice::
            kAcceptAllOptionalDataTypesSync);
    SetGlicCapability(GetProfile(), true);
    SetFRECompletion(GetProfile(), prefs::FreStatus::kCompleted);
    coordinator().GetWebContentsWarmingPoolForTesting().Shutdown();
#endif
  }

  // Warms the network service (disk cache backend, cookie store, cert verifier,
  // first HttpNetworkSession) before the first timed open. Real users open
  // Glic in a browser that has already been talking to the network, so those
  // one-time costs are a harness artifact. The warm-up deliberately uses a
  // different origin from the guest: Chrome does not preconnect for Glic, so
  // the "cold" number must include the guest's own DNS/TCP/TLS setup.
  void WarmNetworkService(const GURL& guest_url) {
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
    ASSERT_NE(warmup_server_.port(), guest_url.EffectiveIntPort());
    // A top-level navigation (not a browser-process fetch) so it exercises the
    // same renderer-initiated loader path as the guest.
    tabs::TabInterface* warmup_tab =
        GetTabListInterface()->OpenTab(warmup_server_.GetURL("/warmup"), -1);
    ASSERT_TRUE(warmup_tab);
    std::ignore = content::WaitForLoadStop(warmup_tab->GetContents());
    // Don't leave an extra renderer alive for the rest of the run.
    GetTabListInterface()->CloseTab(warmup_tab->GetHandle());
  }

  void PrepareWarmingPoolForIteration(bool has_custom_url,
                                      std::string* warmed_viewport) {
    *warmed_viewport = "n/a";
    auto& warming_pool = coordinator().GetWebContentsWarmingPoolForTesting();
    if (!IsWarmed()) {
      warming_pool.Shutdown();
      return;
    }

    warming_pool.MaybeStartWarming(GlicWarmingTrigger::kStartup);
    ASSERT_TRUE(base::test::RunUntil([&]() {
      auto* container = warming_pool.GetWarmedContainerForTesting();
      return container && container->web_client_manager()
                              .has_pending_web_client_receiver_for_testing();
    }));
    auto* container = warming_pool.GetWarmedContainerForTesting();
    content::WebContents* guest_wc =
        container ? container->web_client_manager().web_client_contents()
                  : nullptr;
    if (!guest_wc) {
      return;
    }
    content::WaitForLoadStop(guest_wc);
    if (has_custom_url) {
      // Wait for deferred Gemini Web islands (e.g. islands-B4mbu8EI.js) to
      // finish loading and hydrating in the warming pool so we measure a
      // genuinely warmed container rather than racing async script load.
      (void)content::EvalJs(
          guest_wc,
          "new Promise(resolve => {"
          "  let lastCount = 0;"
          "  let stableTicks = 0;"
          "  const start = performance.now();"
          "  const check = () => {"
          "    const count = document.querySelectorAll('*').length;"
          "    if (count >= 220 && count === lastCount) {"
          "      stableTicks++;"
          "      if (stableTicks >= 3) { resolve(count); return; }"
          "    } else {"
          "      stableTicks = 0;"
          "      lastCount = count;"
          "    }"
          "    if (performance.now() - start > 3000) { resolve(count); "
          "return; }"
          "    setTimeout(check, 150);"
          "  };"
          "  setTimeout(check, 150);"
          "})");
    }
    *warmed_viewport = QueryViewportAndDomInfo(guest_wc);
  }

  // On platforms that maintain a spare renderer by default (Desktop and
  // Desktop Android, where kSitePerProcess is enabled; not Mobile Android),
  // navigating a tab triggers background creation of a replacement spare via
  // PrepareForFutureRequests(). Wait for any pending spare to finish launching
  // before timing Glic open. Use `all_of` so Mobile Android (where GetSpares()
  // is empty) and bots that decline spare creation under memory/process limits
  // proceed without hanging.
  void WaitForSpareRenderersReady() {
    ASSERT_TRUE(base::test::RunUntil([]() {
      return std::ranges::all_of(
          content::SpareRenderProcessHostManager::Get().GetSpares(),
          [](content::RenderProcessHost* spare) {
            return !spare || spare->IsReady();
          });
    }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(GlicInitializationBenchmark, MeasureInitializationTime) {
  const int iterations = GetBenchmarkIterations();
  const std::string config_str = GetConfigName();
  const bool has_custom_url = HasCustomUrl();
  const GURL guest_url = glic::GetGuestURL(GetProfile());

  LOG(INFO) << "=== Starting Glic Initialization Benchmark (" << config_str
            << ", " << iterations << " iterations, url=" << guest_url
            << ") ===";

  SignInForProdModeIfNeeded();
  if (HasFatalFailure()) {
    return;
  }
  WarmNetworkService(guest_url);
  if (HasFatalFailure()) {
    return;
  }

  HistogramDeltaTracker presentation_tracker(
      "Glic.Instance.PanelPresentationTime.SidePanel");
  HistogramDeltaTracker warmed_tracker("Glic.Contents.TimeToWarmed");
  BenchmarkMetrics metrics;

  const int warmup_iterations = has_custom_url ? 1 : 0;
  for (int i = -warmup_iterations; i < iterations; ++i) {
    const bool is_warmup = (i < 0);
    std::string warmed_viewport;
    PrepareWarmingPoolForIteration(has_custom_url, &warmed_viewport);
    if (HasFatalFailure()) {
      return;
    }

    tabs::TabInterface* tab = CreateAndActivateTab(GURL("about:blank"));
    ASSERT_TRUE(tab);
    WaitForSpareRenderersReady();
    if (HasFatalFailure()) {
      return;
    }

    base::ElapsedTimer timer;
    auto open_result = OpenGlicForActiveTab();
    ASSERT_OK(open_result);
    GlicInstanceImpl* instance = *open_result;
    ASSERT_TRUE(instance);

    ASSERT_OK(WaitForGlicClient(instance));
    const double elapsed_ms = timer.Elapsed().InMillisecondsF();

    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return presentation_tracker.HasNewSamples(); }));

    double presentation_time_ms = 0.0;
    if (auto delta = presentation_tracker.RecordDelta()) {
      presentation_time_ms = *delta;
      if (!is_warmup) {
        metrics.presentation_times_ms.push_back(presentation_time_ms);
      }
    }

    double time_to_warmed_ms = 0.0;
    if (auto delta = warmed_tracker.RecordDelta()) {
      time_to_warmed_ms = *delta;
      if (!is_warmup) {
        metrics.time_to_warmed_times_ms.push_back(time_to_warmed_ms);
      }
    }

    if (is_warmup) {
      ASSERT_OK(CloseGlicForTabAndWait(tab));
      continue;
    }

    metrics.init_times_ms.push_back(elapsed_ms);
    GuestMarks marks = CollectAndLogGuestMarks(
        instance->host().web_client_contents(), config_str, i + 1);
    metrics.RecordGuestMarks(marks);

    LOG(INFO) << "[" << config_str << "] Iteration " << (i + 1) << "/"
              << iterations << ": total_init=" << elapsed_ms << " ms"
              << ", presentation_time=" << presentation_time_ms << " ms"
              << ", time_to_warmed=" << time_to_warmed_ms << " ms"
              << ", warmed_vp=" << warmed_viewport
              << ", opened_vp=" << marks.opened_viewport
              << ", panel_size=" << instance->GetPanelSize().ToString()
              << ", script_start=" << marks.script_start_ms << " ms"
              << ", bootstrap_received=" << marks.bootstrap_received_ms << " ms"
              << ", client_init=" << marks.client_init_ms << " ms"
              << ", panel_opened=" << marks.panel_opened_ms << " ms"
              << ", script->init=" << marks.script_to_init_ms << " ms"
              << ", init->open=" << marks.init_to_open_ms << " ms";

    ASSERT_OK(CloseGlicForTabAndWait(tab));
  }

  LogSummaryStats(config_str, metrics);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GlicInitializationBenchmark,
    testing::Values(BenchmarkParams{.is_no_webview = false, .is_warmed = false},
                    BenchmarkParams{.is_no_webview = false, .is_warmed = true},
                    BenchmarkParams{.is_no_webview = true, .is_warmed = false},
                    BenchmarkParams{.is_no_webview = true, .is_warmed = true}),
    [](const testing::TestParamInfo<BenchmarkParams>& info) {
      return base::StrCat({info.param.is_no_webview ? "NoWebview" : "Webview",
                           "_", info.param.is_warmed ? "Warmed" : "Cold"});
    });

}  // namespace glic
