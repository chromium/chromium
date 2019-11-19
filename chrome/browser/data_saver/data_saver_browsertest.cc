// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace {

void SetDataSaverEnabled(content::BrowserContext* browser_context,
                         bool enabled) {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  data_reduction_proxy::DataReductionProxySettings::
      SetDataSaverEnabledForTesting(profile->GetPrefs(), enabled);
  base::RunLoop().RunUntilIdle();
}

std::unique_ptr<net::test_server::HttpResponse> CaptureHeaderHandlerWithContent(
    const std::string& path,
    net::test_server::HttpRequest::HeaderMap* header_map,
    const std::string& mime_type,
    const std::string& content,
    base::OnceClosure done_callback,
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  if (request_url.path() != path)
    return nullptr;

  *header_map = request.headers;
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (!mime_type.empty()) {
    response->set_content_type(mime_type);
  }
  response->set_content(content);
  std::move(done_callback).Run();
  return response;
}

// Test version of the observer. Used to wait for the event when the network
// quality tracker sends the network quality change notification.
class TestEffectiveConnectionTypeObserver
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  explicit TestEffectiveConnectionTypeObserver(
      network::NetworkQualityTracker* tracker)
      : run_loop_wait_effective_connection_type_(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN),
        run_loop_(std::make_unique<base::RunLoop>()),
        tracker_(tracker),
        effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    tracker_->AddEffectiveConnectionTypeObserver(this);
  }

  ~TestEffectiveConnectionTypeObserver() override {
    tracker_->RemoveEffectiveConnectionTypeObserver(this);
  }

  void WaitForNotification(
      net::EffectiveConnectionType run_loop_wait_effective_connection_type) {
    if (effective_connection_type_ == run_loop_wait_effective_connection_type)
      return;
    ASSERT_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
              run_loop_wait_effective_connection_type);
    run_loop_wait_effective_connection_type_ =
        run_loop_wait_effective_connection_type;
    run_loop_->Run();
    run_loop_.reset(new base::RunLoop());
  }

 private:
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
    run_loop_->Quit();
  }

  net::EffectiveConnectionType run_loop_wait_effective_connection_type_;
  std::unique_ptr<base::RunLoop> run_loop_;
  network::NetworkQualityTracker* tracker_;
  net::EffectiveConnectionType effective_connection_type_;

  DISALLOW_COPY_AND_ASSIGN(TestEffectiveConnectionTypeObserver);
};

// Test version of the observer. Used to wait for the event when the network
// quality tracker sends the network quality change notification.
class TestRTTAndThroughputEstimatesObserver
    : public network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver {
 public:
  explicit TestRTTAndThroughputEstimatesObserver(
      network::NetworkQualityTracker* tracker)
      : tracker_(tracker),
        downstream_throughput_kbps_(std::numeric_limits<int32_t>::max()) {
    tracker_->AddRTTAndThroughputEstimatesObserver(this);
  }

  ~TestRTTAndThroughputEstimatesObserver() override {
    tracker_->RemoveRTTAndThroughputEstimatesObserver(this);
  }

  void WaitForNotification(base::TimeDelta expected_http_rtt) {
    // It's not meaningful to wait for notification with RTT set to
    // base::TimeDelta() since that value implies that the network quality
    // estimate was unavailable.
    EXPECT_NE(base::TimeDelta(), expected_http_rtt);
    http_rtt_notification_wait_ = expected_http_rtt;
    if (http_rtt_notification_wait_ == http_rtt_)
      return;

    // WaitForNotification should not be called twice.
    EXPECT_EQ(nullptr, run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    EXPECT_EQ(expected_http_rtt, http_rtt_);
    run_loop_.reset();
  }

 private:
  // RTTAndThroughputEstimatesObserver implementation:
  void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) override {
    EXPECT_EQ(http_rtt, tracker_->GetHttpRTT());
    EXPECT_EQ(downstream_throughput_kbps,
              tracker_->GetDownstreamThroughputKbps());

    http_rtt_ = http_rtt;
    downstream_throughput_kbps_ = downstream_throughput_kbps;

    if (run_loop_ && http_rtt == http_rtt_notification_wait_)
      run_loop_->Quit();
  }

  network::NetworkQualityTracker* tracker_;
  // May be null.
  std::unique_ptr<base::RunLoop> run_loop_;
  base::TimeDelta http_rtt_;
  int32_t downstream_throughput_kbps_;
  base::TimeDelta http_rtt_notification_wait_;

  DISALLOW_COPY_AND_ASSIGN(TestRTTAndThroughputEstimatesObserver);
};

}  // namespace

class DataSaverBrowserTest : public InProcessBrowserTest {
 protected:
  void EnableDataSaver(bool enabled) {
    SetDataSaverEnabled(browser()->profile(), enabled);
  }

  void VerifySaveDataHeader(const std::string& expected_header_value,
                            Browser* browser = nullptr) {
    if (!browser)
      browser = InProcessBrowserTest::browser();
    ui_test_utils::NavigateToURL(
        browser, embedded_test_server()->GetURL("/echoheader?Save-Data"));
    std::string header_value;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.body.textContent);",
        &header_value));
    EXPECT_EQ(expected_header_value, header_value);
  }
};

IN_PROC_BROWSER_TEST_F(DataSaverBrowserTest, DataSaverEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableDataSaver(true);
  VerifySaveDataHeader("on");
}

IN_PROC_BROWSER_TEST_F(DataSaverBrowserTest, DataSaverDisabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableDataSaver(false);
  VerifySaveDataHeader("None");
}

IN_PROC_BROWSER_TEST_F(DataSaverBrowserTest, DataSaverDisabledInIncognito) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableDataSaver(true);
  VerifySaveDataHeader("None", CreateIncognitoBrowser());
}

class DataSaverWithServerBrowserTest : public InProcessBrowserTest {
 protected:
  void Init() {
    test_server_.reset(new net::EmbeddedTestServer());
    test_server_->RegisterRequestHandler(
        base::Bind(&DataSaverWithServerBrowserTest::VerifySaveDataHeader,
                   base::Unretained(this)));
    test_server_->ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  }

  void EnableDataSaver(bool enabled) {
    SetDataSaverEnabled(browser()->profile(), enabled);
  }

  net::EffectiveConnectionType GetEffectiveConnectionType() const {
    return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
               browser()->profile())
        ->data_reduction_proxy_service()
        ->GetEffectiveConnectionType();
  }

  base::Optional<base::TimeDelta> GetHttpRttEstimate() const {
    return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
               browser()->profile())
        ->data_reduction_proxy_service()
        ->GetHttpRttEstimate();
  }

  std::unique_ptr<net::test_server::HttpResponse> VerifySaveDataHeader(
      const net::test_server::HttpRequest& request) {
    auto save_data_header_it = request.headers.find("save-data");

    if (request.relative_url == "/favicon.ico") {
      // Favicon request could be received for the previous page load.
      return std::unique_ptr<net::test_server::HttpResponse>();
    }

    if (!expected_save_data_header_.empty()) {
      EXPECT_TRUE(save_data_header_it != request.headers.end())
          << request.relative_url;
      EXPECT_EQ(expected_save_data_header_, save_data_header_it->second)
          << request.relative_url;
    } else {
      EXPECT_TRUE(save_data_header_it == request.headers.end())
          << request.relative_url;
    }
    return std::unique_ptr<net::test_server::HttpResponse>();
  }

  std::unique_ptr<net::EmbeddedTestServer> test_server_;
  std::string expected_save_data_header_;
};

IN_PROC_BROWSER_TEST_F(DataSaverWithServerBrowserTest, ReloadPage) {
  Init();
  ASSERT_TRUE(test_server_->Start());
  EnableDataSaver(true);

  expected_save_data_header_ = "on";
  ui_test_utils::NavigateToURL(browser(),
                               test_server_->GetURL("/google/google.html"));

  // Reload the webpage and expect the main and the subresources will get the
  // correct save-data header.
  expected_save_data_header_ = "on";
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Reload the webpage with data saver disabled, and expect all the resources
  // will get no save-data header.
  EnableDataSaver(false);
  expected_save_data_header_ = "";
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
}

// Test that the data saver receives changes in effective connection type.
IN_PROC_BROWSER_TEST_F(DataSaverWithServerBrowserTest,
                       EffectiveConnectionType) {
  Init();

  // Add a test observer. To determine if data reduction proxy component has
  // received the network quality change notification, we check if the test
  // observer has received the notification. Note that all the observers are
  // notified in the same message loop by the network quality tracker.
  TestEffectiveConnectionTypeObserver observer(
      g_browser_process->network_quality_tracker());

  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);
  observer.WaitForNotification(net::EFFECTIVE_CONNECTION_TYPE_4G);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_4G, GetEffectiveConnectionType());

  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);
  observer.WaitForNotification(net::EFFECTIVE_CONNECTION_TYPE_2G);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_2G, GetEffectiveConnectionType());

  g_browser_process->network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_3G);
  observer.WaitForNotification(net::EFFECTIVE_CONNECTION_TYPE_3G);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_3G, GetEffectiveConnectionType());
}

// Test that the data saver receives changes in HTTP RTT estimate.
IN_PROC_BROWSER_TEST_F(DataSaverWithServerBrowserTest, HttpRttEstimate) {
  Init();

  // Add a test observer. To determine if data reduction proxy component has
  // received the network quality change notification, we check if the test
  // observer has received the notification. Note that all the observers are
  // notified in the same message loop by the network quality tracker.
  TestRTTAndThroughputEstimatesObserver observer(
      g_browser_process->network_quality_tracker());

  g_browser_process->network_quality_tracker()
      ->ReportRTTsAndThroughputForTesting(
          base::TimeDelta::FromMilliseconds(100), 0);
  observer.WaitForNotification(base::TimeDelta::FromMilliseconds(100));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(100), GetHttpRttEstimate());

  g_browser_process->network_quality_tracker()
      ->ReportRTTsAndThroughputForTesting(
          base::TimeDelta::FromMilliseconds(500), 0);
  observer.WaitForNotification(base::TimeDelta::FromMilliseconds(500));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500), GetHttpRttEstimate());
}

class DataSaverForWorkerBrowserTest : public InProcessBrowserTest,
                                      public testing::WithParamInterface<bool> {
 protected:
  void EnableDataSaver(bool enabled) {
    SetDataSaverEnabled(browser()->profile(), enabled);
  }

  // Sends a request to |url| and returns its headers via |header_map|. |script|
  // is provided as the response body.
  void RequestAndGetHeaders(
      const std::string& url,
      const std::string& script,
      net::test_server::HttpRequest::HeaderMap* header_map) {
    base::RunLoop loop;
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &CaptureHeaderHandlerWithContent, "/capture", header_map,
        "text/javascript", script, loop.QuitClosure()));
    ASSERT_TRUE(embedded_test_server()->Start());

    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(url));
    loop.Run();
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  static bool IsEnabledDataSaver() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         DataSaverForWorkerBrowserTest,
                         testing::Bool());

// Checks that the Save-Data header is sent in a request for dedicated worker
// script when the data saver is enabled.
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest, DedicatedWorker) {
  EnableDataSaver(IsEnabledDataSaver());

  const std::string kWorkerScript = R"(postMessage('DONE');)";
  net::test_server::HttpRequest::HeaderMap header_map;
  RequestAndGetHeaders(
      "/workers/create_dedicated_worker.html?worker_url=/capture",
      kWorkerScript, &header_map);

  if (IsEnabledDataSaver()) {
    EXPECT_TRUE(base::Contains(header_map, "Save-Data"));
    EXPECT_EQ("on", header_map["Save-Data"]);
  } else {
    EXPECT_FALSE(base::Contains(header_map, "Save-Data"));
  }

  // Wait until the worker script is loaded to stop the test from crashing
  // during destruction.
  EXPECT_EQ("DONE", content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        "waitForMessage();"));
}

// Checks that the Save-Data header is sent in a request for shared worker
// script when the data saver is enabled. Disabled on Android since a shared
// worker is not available on Android.
#if defined(OS_ANDROID)
#define MAYBE_SharedWorker DISABLED_SharedWorker
#else
#define MAYBE_SharedWorker SharedWorker
#endif
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest, MAYBE_SharedWorker) {
  EnableDataSaver(IsEnabledDataSaver());

  const std::string kWorkerScript =
      R"(self.onconnect = e => { e.ports[0].postMessage('DONE'); };)";
  net::test_server::HttpRequest::HeaderMap header_map;
  RequestAndGetHeaders("/workers/create_shared_worker.html?worker_url=/capture",
                       kWorkerScript, &header_map);

  if (IsEnabledDataSaver()) {
    EXPECT_TRUE(base::Contains(header_map, "Save-Data"));
    EXPECT_EQ("on", header_map["Save-Data"]);
  } else {
    EXPECT_FALSE(base::Contains(header_map, "Save-Data"));
  }

  // Wait until the worker script is loaded to stop the test from crashing
  // during destruction.
  EXPECT_EQ("DONE", content::EvalJs(
                        browser()->tab_strip_model()->GetActiveWebContents(),
                        "waitForMessage();"));
}

// Checks that the Save-Data header is not sent in a request for a service
// worker script when it's disabled.
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest, ServiceWorker_Register) {
  EnableDataSaver(IsEnabledDataSaver());
  net::test_server::HttpRequest::HeaderMap header_map;
  base::RunLoop loop;
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &CaptureHeaderHandlerWithContent, "/capture", &header_map,
      "text/javascript", "// empty", loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));

  EXPECT_EQ("DONE",
            content::EvalJs(GetActiveWebContents(), "register('/capture');"));
  loop.Run();

  if (IsEnabledDataSaver()) {
    EXPECT_TRUE(base::Contains(header_map, "Save-Data"));
    EXPECT_EQ("on", header_map["Save-Data"]);
  } else {
    EXPECT_FALSE(base::Contains(header_map, "Save-Data"));
  }

  // Service worker doesn't have to wait for onmessage event because
  // navigator.serviceWorker.ready can ensure that the script load has
  // been completed.
}

// Checks that the Save-Data header is not sent in a request for a service
// worker script when it's disabled.
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest, ServiceWorker_Update) {
  EnableDataSaver(IsEnabledDataSaver());
  net::test_server::HttpRequest::HeaderMap header_map;
  base::RunLoop loop;
  // Wait for two requests to capture the request header for updating.
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&CaptureHeaderHandlerWithContent, "/capture",
                          &header_map, "text/javascript", "// empty",
                          base::BarrierClosure(2, loop.QuitClosure())));
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));

  EXPECT_EQ("DONE",
            content::EvalJs(GetActiveWebContents(), "register('/capture');"));
  EXPECT_EQ("DONE", content::EvalJs(GetActiveWebContents(), "update();"));
  loop.Run();

  if (IsEnabledDataSaver()) {
    EXPECT_TRUE(base::Contains(header_map, "Save-Data"));
    EXPECT_EQ("on", header_map["Save-Data"]);
  } else {
    EXPECT_FALSE(base::Contains(header_map, "Save-Data"));
  }

  // Service worker doesn't have to wait for onmessage event because
  // navigator.serviceWorker.ready can ensure that the script load has
  // been completed.
}

// Checks that Save-Data header is appropriately set to requests from fetch() in
// a dedicated worker.
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest, FetchFromWorker) {
  EnableDataSaver(IsEnabledDataSaver());
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/workers/fetch_from_worker.html"));
  const char* expected = IsEnabledDataSaver() ? "on" : "None";
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "fetch_from_worker('/echoheader?Save-Data');"));
}

// Checks that Save-Data header is appropriately set to requests from fetch() in
// a shared worker.
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest, FetchFromSharedWorker) {
  EnableDataSaver(IsEnabledDataSaver());
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/workers/fetch_from_shared_worker.html"));
  const char* expected = IsEnabledDataSaver() ? "on" : "None";
  EXPECT_EQ(
      expected,
      content::EvalJs(GetActiveWebContents(),
                      "fetch_from_shared_worker('/echoheader?Save-Data');"));
}

// Checks that Save-Data header is appropriately set to requests from fetch() in
// a service worker.
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest, FetchFromServiceWorker) {
  EnableDataSaver(IsEnabledDataSaver());
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/fetch_from_service_worker.html"));
  EXPECT_EQ("ready", content::EvalJs(GetActiveWebContents(), "setup();"));

  const char* expected = IsEnabledDataSaver() ? "on" : "None";
  EXPECT_EQ(
      expected,
      content::EvalJs(GetActiveWebContents(),
                      "fetch_from_service_worker('/echoheader?Save-Data');"));
}

// Checks that Save-Data header is appropriately set to requests from fetch() in
// a page controlled by a service worker without fetch handler.
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest,
                       FetchFromServiceWorkerControlledPage_NoFetchHandler) {
  EnableDataSaver(IsEnabledDataSaver());
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE",
            content::EvalJs(GetActiveWebContents(), "register('empty.js');"));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html"));

  const char* expected = IsEnabledDataSaver() ? "on" : "None";
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "fetch_from_page('/echoheader?Save-Data');"));
}

// Checks that Save-Data header is appropriately set to requests from fetch() in
// a page controlled by a service worker with fetch handler but no respondWith.
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest,
                       FetchFromServiceWorkerControlledPage_PassThrough) {
  EnableDataSaver(IsEnabledDataSaver());
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE",
            content::EvalJs(GetActiveWebContents(),
                            "register('fetch_event_pass_through.js');"));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html"));

  const char* expected = IsEnabledDataSaver() ? "on" : "None";
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "fetch_from_page('/echoheader?Save-Data');"));
}

// Checks that Save-Data header is appropriately set to requests from fetch() in
// a page controlled by a service worker with fetch handler and responds with
// fetch().
IN_PROC_BROWSER_TEST_P(DataSaverForWorkerBrowserTest,
                       FetchFromServiceWorkerControlledPage_RespondWithFetch) {
  EnableDataSaver(IsEnabledDataSaver());
  ASSERT_TRUE(embedded_test_server()->Start());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE",
            content::EvalJs(GetActiveWebContents(),
                            "register('fetch_event_respond_with_fetch.js');"));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/service_worker/fetch_from_page.html"));

  const char* expected = IsEnabledDataSaver() ? "on" : "None";
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "fetch_from_page('/echoheader?Save-Data');"));
}
