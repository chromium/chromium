// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests that Service Workers (a Content feature) work in the Chromium
// embedder.

#include <optional>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/page_load_metrics/observers/service_worker_page_load_metrics_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/nacl/common/buildflags.h"
#include "components/page_load_metrics/browser/page_load_metrics_test_waiter.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"
#include "url/origin.h"

using PageLoadMetricsTestWaiter = page_load_metrics::PageLoadMetricsTestWaiter;

namespace chrome_service_worker_browser_test {

const char kInstallAndWaitForActivatedPage[] =
    "<script>"
    "navigator.serviceWorker.register('./sw.js', {scope: './scope/'})"
    "  .then(function(reg) {"
    "      reg.addEventListener('updatefound', function() {"
    "          var worker = reg.installing;"
    "          worker.addEventListener('statechange', function() {"
    "              if (worker.state == 'activated')"
    "                document.title = 'READY';"
    "            });"
    "        });"
    "    });"
    "</script>";

const char kInstallAndWaitForActivatedPageWithModuleScript[] =
    R"(<script>
    navigator.serviceWorker.register(
        './sw.js', {scope: './scope/', type: 'module'})
      .then(function(reg) {
          reg.addEventListener('updatefound', function() {
              var worker = reg.installing;
              worker.addEventListener('statechange', function() {
                  if (worker.state == 'activated')
                    document.title = 'READY';
                });
            });
        });
    </script>)";

template <typename T>
static void ExpectResultAndRun(T expected,
                               base::OnceClosure continuation,
                               T actual) {
  EXPECT_EQ(expected, actual);
  std::move(continuation).Run();
}

class ChromeServiceWorkerTest : public InProcessBrowserTest {
 public:
  ChromeServiceWorkerTest(const ChromeServiceWorkerTest&) = delete;
  ChromeServiceWorkerTest& operator=(const ChromeServiceWorkerTest&) = delete;

 protected:
  ChromeServiceWorkerTest() {
    EXPECT_TRUE(service_worker_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(base::CreateDirectoryAndGetError(
        service_worker_dir_.GetPath().Append(
            FILE_PATH_LITERAL("scope")), nullptr));
  }
  ~ChromeServiceWorkerTest() override {}

  void WriteFile(const base::FilePath::StringType& filename,
                 std::string_view contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_TRUE(base::WriteFile(service_worker_dir_.GetPath().Append(filename),
                                contents));
  }

  void NavigateToPageAndWaitForReadyTitle(const std::string path) {
    const std::u16string expected_title1 = u"READY";
    content::TitleWatcher title_watcher1(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title1);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(path)));
    EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());
  }

  void InitializeServer() {
    embedded_test_server()->ServeFilesFromDirectory(
        service_worker_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::ServiceWorkerContext* GetServiceWorkerContext() {
    return browser()
        ->profile()
        ->GetDefaultStoragePartition()
        ->GetServiceWorkerContext();
  }

  void TestFallbackMainResourceRequestWhenJSDisabled(const char* test_script) {
    WriteFile(FILE_PATH_LITERAL("sw.js"),
              "self.onfetch = function(e) {"
              "  e.respondWith(new Response('<title>Fail</title>',"
              "                             {headers: {"
              "                             'Content-Type': 'text/html'}}));"
              "};");
    WriteFile(FILE_PATH_LITERAL("scope/done.html"), "<title>Done</title>");
    WriteFile(FILE_PATH_LITERAL("test.html"), test_script);
    InitializeServer();
    NavigateToPageAndWaitForReadyTitle("/test.html");

    GetServiceWorkerContext()->StopAllServiceWorkersForStorageKey(
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(embedded_test_server()->base_url())));
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                   CONTENT_SETTING_BLOCK);

    const std::u16string expected_title = u"Done";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/scope/done.html")));

    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    content::RenderFrameHost* main_frame = browser()
                                               ->tab_strip_model()
                                               ->GetActiveWebContents()
                                               ->GetPrimaryMainFrame();
    EXPECT_TRUE(
        content_settings::PageSpecificContentSettings::GetForFrame(main_frame)
            ->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  }

  void TestStartServiceWorkerAndDispatchMessage(const char* test_script) {
    base::RunLoop run_loop;
    const std::u16string message_data = u"testMessage";

    WriteFile(FILE_PATH_LITERAL("sw.js"), "self.onfetch = function(e) {};");
    WriteFile(FILE_PATH_LITERAL("test.html"), test_script);

    InitializeServer();
    NavigateToPageAndWaitForReadyTitle("/test.html");
    blink::TransferableMessage msg =
        blink::EncodeWebMessagePayload(message_data);
    msg.sender_agent_cluster_id = base::UnguessableToken::Create();

    GURL url = embedded_test_server()->GetURL("/scope/");
    GetServiceWorkerContext()->StartServiceWorkerAndDispatchMessage(
        url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
        std::move(msg),
        base::BindRepeating(&ExpectResultAndRun<bool>, true,
                            run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::unique_ptr<PageLoadMetricsTestWaiter> CreatePageLoadMetricsTestWaiter() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return std::make_unique<PageLoadMetricsTestWaiter>(web_contents);
  }

  base::ScopedTempDir service_worker_dir_;
};

// http://crbug.com/368570
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       CanShutDownWithRegisteredServiceWorker) {
  WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");

  embedded_test_server()->ServeFilesFromDirectory(
      service_worker_dir_.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::RunLoop run_loop;
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL("/"), blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/service_worker.js"), key, options,
      base::BindOnce(&ExpectResultAndRun<blink::ServiceWorkerStatusCode>,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();

  // Leave the Service Worker registered, and make sure that the browser can
  // shut down without DCHECK'ing. It'd be nice to check here that the SW is
  // actually occupying a process, but we don't yet have the public interface to
  // do that.
}

// http://crbug.com/419290
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       CanCloseIncognitoWindowWithServiceWorkerController) {
  WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");
  WriteFile(FILE_PATH_LITERAL("service_worker.js.mock-http-headers"),
            "HTTP/1.1 200 OK\nContent-Type: text/javascript");
  WriteFile(FILE_PATH_LITERAL("test.html"), "");
  InitializeServer();

  Browser* incognito = CreateIncognitoBrowser();

  base::RunLoop run_loop;
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL("/"), blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/service_worker.js"), key, options,
      base::BindOnce(&ExpectResultAndRun<blink::ServiceWorkerStatusCode>,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito, embedded_test_server()->GetURL("/test.html")));

  CloseBrowserSynchronously(incognito);

  // Test passes if we don't crash.
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       FailRegisterServiceWorkerWhenJSDisabled) {
  WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");
  InitializeServer();

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  base::RunLoop run_loop;
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL("/"), blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/service_worker.js"), key, options,
      base::BindOnce(&ExpectResultAndRun<blink::ServiceWorkerStatusCode>,
                     blink::ServiceWorkerStatusCode::kErrorDisallowed,
                     run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ChromeServiceWorkerTest,
    FallbackMainResourceRequestWhenJSDisabledForClassicServiceWorker) {
  TestFallbackMainResourceRequestWhenJSDisabled(
      kInstallAndWaitForActivatedPage);
}

IN_PROC_BROWSER_TEST_F(
    ChromeServiceWorkerTest,
    FallbackMainResourceRequestWhenJSDisabledForModuleServiceWorker) {
  TestFallbackMainResourceRequestWhenJSDisabled(
      kInstallAndWaitForActivatedPageWithModuleScript);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       StartServiceWorkerAndDispatchMessage) {
  TestStartServiceWorkerAndDispatchMessage(kInstallAndWaitForActivatedPage);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       StartServiceWorkerWithModuleScriptAndDispatchMessage) {
  TestStartServiceWorkerAndDispatchMessage(
      kInstallAndWaitForActivatedPageWithModuleScript);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest, SubresourceCountUKM) {
  base::RunLoop ukm_loop;
  ukm::TestAutoSetUkmRecorder test_recorder;
  test_recorder.SetOnAddEntryCallback(
      ukm::builders::ServiceWorker_OnLoad::kEntryName,
      // In the following test, there are two kinds of sub resources loaded;
      // one is handled with "respondWith", and the other is not.
      // `ukm_loop.Quit()` is called when both of them are recorded in UKM.
      base::BindLambdaForTesting([&]() {
        auto entries = test_recorder.GetEntriesByName(
            ukm::builders::ServiceWorker_OnLoad::kEntryName);
        CHECK(!entries.empty());
        const int64_t* v = ukm::TestAutoSetUkmRecorder::GetEntryMetric(
            entries[0],
            ukm::builders::ServiceWorker_OnLoad::kTotalSubResourceLoadName);
        CHECK(v);
        if (*v == 2) {
          ukm_loop.Quit();
        }
      }));

  WriteFile(FILE_PATH_LITERAL("fallback.css"), "");
  WriteFile(FILE_PATH_LITERAL("nofallback.css"), "");
  WriteFile(FILE_PATH_LITERAL("subresources.html"),
            "<link href='./fallback.css' rel='stylesheet'>"
            "<link href='./nofallback.css' rel='stylesheet'>");
  WriteFile(FILE_PATH_LITERAL("sw.js"),
            "this.onactivate = function(event) {"
            "  event.waitUntil(self.clients.claim());"
            "};"
            "this.onfetch = function(event) {"
            // We will fallback fallback.css.
            "  if (event.request.url.endsWith('/fallback.css')) {"
            "    return;"
            "  }"
            "  event.respondWith(fetch(event.request));"
            "};");
  WriteFile(FILE_PATH_LITERAL("test.html"),
            "<script>"
            "navigator.serviceWorker.register('./sw.js', {scope: './'})"
            "  .then(function(reg) {"
            "      reg.addEventListener('updatefound', function() {"
            "          var worker = reg.installing;"
            "          worker.addEventListener('statechange', function() {"
            "              if (worker.state == 'activated')"
            "                document.title = 'READY';"
            "            });"
            "        });"
            "    });"
            "</script>");

  InitializeServer();

  {
    // The message "READY" will be sent when the service worker is activated.
    const std::u16string expected_title = u"READY";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/test.html")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  {
    // Navigate to the service worker controlled page.
    auto waiter = CreatePageLoadMetricsTestWaiter();
    waiter->AddPageExpectation(
        PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/subresources.html")));
    waiter->Wait();
  }

  // Navigate away to record metrics.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Wait until the UKM record has enough entries.
  ukm_loop.Run();

  auto entries = test_recorder.GetEntriesByName(
      ukm::builders::ServiceWorker_OnLoad::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::ServiceWorker_OnLoad::kMainAndSubResourceLoadLocationName,
      6 /* = kMainResourceNotFallbackAndSubResourceMixed */);
  test_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::ServiceWorker_OnLoad::kTotalSubResourceLoadName, 2);
  test_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::ServiceWorker_OnLoad::kTotalSubResourceFallbackName, 1);
  test_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::ServiceWorker_OnLoad::kSubResourceFallbackRatioName, 50);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::ServiceWorker_OnLoad::kAudioFallbackName, 0);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::ServiceWorker_OnLoad::kAudioHandledName, 0);
  test_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::ServiceWorker_OnLoad::kCSSStyleSheetFallbackName, 1);
  test_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::ServiceWorker_OnLoad::kCSSStyleSheetHandledName, 1);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::ServiceWorker_OnLoad::kFontFallbackName, 0);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::ServiceWorker_OnLoad::kFontHandledName, 0);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::ServiceWorker_OnLoad::kImageFallbackName, 0);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::ServiceWorker_OnLoad::kImageHandledName, 0);
}

// TODO(crbug.com/355104619): The test is flaky. Re-enable it.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_StaticRoutingAPISubresourceHistogramTest \
  DISABLED_StaticRoutingAPISubresourceHistogramTest
#else
#define MAYBE_StaticRoutingAPISubresourceHistogramTest \
  StaticRoutingAPISubresourceHistogramTest
#endif
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       MAYBE_StaticRoutingAPISubresourceHistogramTest) {
  base::HistogramTester histogram_tester;
  WriteFile(FILE_PATH_LITERAL("scope/fallback.css"), "");
  WriteFile(FILE_PATH_LITERAL("scope/nofallback.css"), "");
  WriteFile(FILE_PATH_LITERAL("scope/subresources.html"),
            "<link href='./fallback.css' rel='stylesheet'>"
            "<link href='./nofallback.css' rel='stylesheet'>");
  WriteFile(FILE_PATH_LITERAL("sw.js"),
            R"( this.onactivate = function(event) {
                  event.waitUntil(self.clients.claim());
                };
                this.addEventListener('install', e => {
                  e.addRoutes([{
                    condition: {
                      urlPattern: new URLPattern()
                    },
                    source: 'fetch-event',
                  },
                  ]);
                });
                this.onfetch = function(event) {
                  if (event.request.url.endsWith('/fallback.css')) {
                    return;
                  }
                  event.respondWith(fetch(event.request));
                };)");

  WriteFile(FILE_PATH_LITERAL("test.html"), kInstallAndWaitForActivatedPage);

  InitializeServer();

  {
    // The message "READY" will be sent when the service worker is activated.
    const std::u16string expected_title = u"READY";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/test.html")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  {
    // Navigate to the service worker controlled page.
    content::TestFrameNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/scope/subresources.html")));
    observer.WaitForCommit();
  }

  {
    // Navigate away to record metrics.
    content::TestFrameNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
    observer.WaitForCommit();
  }

  histogram_tester.ExpectTotalCount(
      internal::kHistogramServiceWorkerSubresourceTotalRouterEvaluationTime, 1);
}

// TODO(crbug.com/360158408): The test is flaky on mac bots.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SubresourceCountUMA DISABLED_SubresourceCountUMA
#else
#define MAYBE_SubresourceCountUMA SubresourceCountUMA
#endif
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest, MAYBE_SubresourceCountUMA) {
  base::HistogramTester histogram_tester;

  WriteFile(FILE_PATH_LITERAL("fallback.css"), "");
  WriteFile(FILE_PATH_LITERAL("nofallback.css"), "");
  WriteFile(FILE_PATH_LITERAL("subresources.html"),
            "<link href='./fallback.css' rel='stylesheet'>"
            "<link href='./nofallback.css' rel='stylesheet'>");
  WriteFile(FILE_PATH_LITERAL("sw.js"),
            "this.onactivate = function(event) {"
            "  event.waitUntil(self.clients.claim());"
            "};"
            "this.onfetch = function(event) {"
            // We will fallback fallback.css.
            "  if (event.request.url.endsWith('/fallback.css')) {"
            "    return;"
            "  }"
            "  event.respondWith(fetch(event.request));"
            "};");
  WriteFile(FILE_PATH_LITERAL("test.html"),
            "<script>"
            "navigator.serviceWorker.register('./sw.js', {scope: './'})"
            "  .then(function(reg) {"
            "      reg.addEventListener('updatefound', function() {"
            "          var worker = reg.installing;"
            "          worker.addEventListener('statechange', function() {"
            "              if (worker.state == 'activated')"
            "                document.title = 'READY';"
            "            });"
            "        });"
            "    });"
            "</script>");

  InitializeServer();

  {
    // The message "READY" will be sent when the service worker is activated.
    const std::u16string expected_title = u"READY";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/test.html")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  // Navigate to the service worker controlled page.
  auto waiter = CreatePageLoadMetricsTestWaiter();
  waiter->AddPageExpectation(
      PageLoadMetricsTestWaiter::TimingField::kLoadEvent);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/subresources.html")));
  waiter->Wait();

  // Navigate away to record metrics.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));

  // Sync the histogram data between the renderer and browser processes.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectTotalCount("ServiceWorker.Subresource.Handled.Type2",
                                    1);
  histogram_tester.ExpectUniqueSample("ServiceWorker.Subresource.Handled.Type2",
                                      2 /* kCSSStyleSheet */, 1);
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.Subresource.Fallbacked.Type2", 1);
  histogram_tester.ExpectUniqueSample(
      "ServiceWorker.Subresource.Fallbacked.Type2", 2 /* kCSSStyleSheet */, 1);
}

class ChromeServiceWorkerFetchTest : public ChromeServiceWorkerTest {
 public:
  ChromeServiceWorkerFetchTest(const ChromeServiceWorkerFetchTest&) = delete;
  ChromeServiceWorkerFetchTest& operator=(const ChromeServiceWorkerFetchTest&) =
      delete;

 protected:
  ChromeServiceWorkerFetchTest() {}
  ~ChromeServiceWorkerFetchTest() override {}

  void SetUpOnMainThread() override {
    WriteServiceWorkerFetchTestFiles();
    embedded_test_server()->ServeFilesFromDirectory(
        service_worker_dir_.GetPath());
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
    InitializeServiceWorkerFetchTestPage();
  }

  std::string RequestString(const std::string& url,
                            const std::string& mode,
                            const std::string& credentials,
                            const std::string& destination) const {
    return base::StringPrintf(
        "url:%s, mode:%s, credentials:%s, destination:%s\n", url.c_str(),
        mode.c_str(), credentials.c_str(), destination.c_str());
  }

  std::string GetURL(const std::string& relative_url) const {
    return embedded_test_server()->GetURL(relative_url).spec();
  }

 private:
  void WriteServiceWorkerFetchTestFiles() {
    WriteFile(
        FILE_PATH_LITERAL("sw.js"),
        "this.onactivate = function(event) {"
        "  event.waitUntil(self.clients.claim());"
        "};"
        "this.onfetch = function(event) {"
        // Ignore the default favicon request. The default favicon request
        // is sent after the page loading is finished, and we can't
        // control the timing of the request. If the request is sent after
        // clients.claim() is called, fetch event for the default favicon
        // request is triggered and the tests become flaky. See
        // https://crbug.com/912543.
        "  if (event.request.url.endsWith('/favicon.ico')) {"
        "    return;"
        "  }"
        "  event.respondWith("
        "      self.clients.matchAll().then(function(clients) {"
        "          clients.forEach(function(client) {"
        "              client.postMessage("
        "                'url:' + event.request.url + ', ' +"
        "                'mode:' + event.request.mode + ', ' +"
        "                'credentials:' + event.request.credentials + ', ' +"
        "                'destination:' + event.request.destination"
        "              );"
        "            });"
        "          return fetch(event.request);"
        "        }));"
        "};");
    WriteFile(FILE_PATH_LITERAL("test.html"), R"(
              <script src='/result_queue.js'></script>
              <script>
              navigator.serviceWorker.register('./sw.js', {scope: './'})
                .then(function(reg) {
                    reg.addEventListener('updatefound', function() {
                        var worker = reg.installing;
                        worker.addEventListener('statechange', function() {
                            if (worker.state == 'activated')
                              document.title = 'READY';
                          });
                      });
                  });
              var reportOnFetch = true;
              var issuedRequests = [];
              var reports = new ResultQueue();
              function reportRequests() {
                var str = '';
                issuedRequests.forEach(function(data) {
                  str += data + '\n';
                });
                reports.push(str);
              }
              navigator.serviceWorker.addEventListener(
                  'message',
                  function(event) {
                    issuedRequests.push(event.data);
                    if (reportOnFetch) {
                      reportRequests();
                    }
                  }, false);
              </script>
              )");
  }

  void InitializeServiceWorkerFetchTestPage() {
    // The message "READY" will be sent when the service worker is activated.
    const std::u16string expected_title = u"READY";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/test.html")));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }
};

class FaviconUpdateWaiter : public favicon::FaviconDriverObserver {
 public:
  explicit FaviconUpdateWaiter(content::WebContents* web_contents) {
    scoped_observation_.Observe(
        favicon::ContentFaviconDriver::FromWebContents(web_contents));
  }

  FaviconUpdateWaiter(const FaviconUpdateWaiter&) = delete;
  FaviconUpdateWaiter& operator=(const FaviconUpdateWaiter&) = delete;

  ~FaviconUpdateWaiter() override = default;

  void Wait() {
    if (updated_)
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    updated_ = true;
    if (!quit_closure_.is_null())
      std::move(quit_closure_).Run();
  }

  bool updated_ = false;
  base::ScopedObservation<favicon::FaviconDriver,
                          favicon::FaviconDriverObserver>
      scoped_observation_{this};
  base::OnceClosure quit_closure_;
};

class ChromeServiceWorkerLinkFetchTest : public ChromeServiceWorkerFetchTest {
 public:
  ChromeServiceWorkerLinkFetchTest(const ChromeServiceWorkerLinkFetchTest&) =
      delete;
  ChromeServiceWorkerLinkFetchTest& operator=(
      const ChromeServiceWorkerLinkFetchTest&) = delete;

 protected:
  ChromeServiceWorkerLinkFetchTest() {}
  ~ChromeServiceWorkerLinkFetchTest() override {}
  void SetUpOnMainThread() override {
    // Map all hosts to localhost and setup the EmbeddedTestServer for
    // redirects.
    host_resolver()->AddRule("*", "127.0.0.1");
    ChromeServiceWorkerFetchTest::SetUpOnMainThread();
  }
  std::string ExecuteManifestFetchTest(const std::string& url,
                                       const std::string& cross_origin) {
    std::string js(
        base::StringPrintf("reportOnFetch = false;"
                           "var link = document.createElement('link');"
                           "link.rel = 'manifest';"
                           "link.href = '%s';",
                           url.c_str()));
    if (!cross_origin.empty()) {
      js +=
          base::StringPrintf("link.crossOrigin = '%s';", cross_origin.c_str());
    }
    js += "document.head.appendChild(link);";
    ExecuteJavaScriptForTests(js);
    return GetManifestAndIssuedRequests();
  }

  std::string ExecuteFaviconFetchTest(const std::string& url) {
    FaviconUpdateWaiter waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    std::string js(
        base::StringPrintf("reportOnFetch = false;"
                           "var link = document.createElement('link');"
                           "link.rel = 'icon';"
                           "link.href = '%s';"
                           "document.head.appendChild(link);",
                           url.c_str()));
    ExecuteJavaScriptForTests(js);
    waiter.Wait();
    return EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                  "reportRequests(); reports.pop();")
        .ExtractString();
  }

  void CopyTestFile(const std::string& src, const std::string& dst) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    EXPECT_TRUE(base::CopyFile(test_data_dir.AppendASCII(src),
                               service_worker_dir_.GetPath().AppendASCII(dst)));
  }

 private:
  void ExecuteJavaScriptForTests(const std::string& js) {
    base::RunLoop run_loop;
    browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame()
        ->ExecuteJavaScriptForTests(
            base::ASCIIToUTF16(js),
            base::BindOnce(
                [](base::OnceClosure quit_callback, base::Value result) {
                  std::move(quit_callback).Run();
                },
                run_loop.QuitClosure()),
            content::ISOLATED_WORLD_ID_GLOBAL);
    run_loop.Run();
  }

  std::string GetManifestAndIssuedRequests() {
    base::RunLoop run_loop;
    browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryPage()
        .GetManifest(
            base::BindOnce(&ManifestCallbackAndRun, run_loop.QuitClosure()));
    run_loop.Run();
    return EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                  "if (issuedRequests.length != 0) reportRequests();"
                  "else reportOnFetch = true;"
                  "reports.pop();")
        .ExtractString();
  }

  static void ManifestCallbackAndRun(base::OnceClosure continuation,
                                     blink::mojom::ManifestRequestResult,
                                     const GURL&,
                                     blink::mojom::ManifestPtr) {
    std::move(continuation).Run();
  }
};

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerLinkFetchTest, ManifestSameOrigin) {
  // <link rel="manifest" href="manifest.json">
  EXPECT_EQ(RequestString(GetURL("/manifest.json"), "cors", "omit", "manifest"),
            ExecuteManifestFetchTest("manifest.json", ""));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerLinkFetchTest,
                       ManifestSameOriginUseCredentials) {
  // <link rel="manifest" href="manifest.json" crossorigin="use-credentials">
  EXPECT_EQ(
      RequestString(GetURL("/manifest.json"), "cors", "include", "manifest"),
      ExecuteManifestFetchTest("manifest.json", "use-credentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerLinkFetchTest, ManifestOtherOrigin) {
  // <link rel="manifest" href="http://www.example.com:PORT/manifest.json">
  const std::string url = embedded_test_server()
                              ->GetURL("www.example.com", "/manifest.json")
                              .spec();
  EXPECT_EQ(RequestString(url, "cors", "omit", "manifest"),
            ExecuteManifestFetchTest(url, ""));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerLinkFetchTest,
                       ManifestOtherOriginUseCredentials) {
  // <link rel="manifest" href="http://www.example.com:PORT/manifest.json"
  //  crossorigin="use-credentials">
  const std::string url = embedded_test_server()
                              ->GetURL("www.example.com", "/manifest.json")
                              .spec();
  EXPECT_EQ(RequestString(url, "cors", "include", "manifest"),
            ExecuteManifestFetchTest(url, "use-credentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerLinkFetchTest, FaviconSameOrigin) {
  // <link rel="favicon" href="fav.png">
  CopyTestFile("favicon/icon.png", "fav.png");
  EXPECT_EQ(RequestString(GetURL("/fav.png"), "no-cors", "include", "image"),
            ExecuteFaviconFetchTest("/fav.png"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerLinkFetchTest, FaviconOtherOrigin) {
  // <link rel="favicon" href="http://www.example.com:PORT/fav.png">
  CopyTestFile("favicon/icon.png", "fav.png");
  const std::string url =
      embedded_test_server()->GetURL("www.example.com", "/fav.png").spec();
  EXPECT_EQ("", ExecuteFaviconFetchTest(url));
}

#if BUILDFLAG(ENABLE_NACL)
// This test registers a service worker and then loads a controlled iframe that
// creates a PNaCl plugin in an <embed> element. Once loaded, the PNaCl plugin
// is ordered to do a resource request for "/echo". The service worker records
// all the fetch events it sees. Since requests for plug-ins and requests
// initiated by plug-ins should not be interecepted by service workers, we
// expect that the the service worker only see the navigation request for the
// iframe.
class ChromeServiceWorkerFetchPPAPITest : public ChromeServiceWorkerFetchTest {
 public:
  ChromeServiceWorkerFetchPPAPITest(const ChromeServiceWorkerFetchPPAPITest&) =
      delete;
  ChromeServiceWorkerFetchPPAPITest& operator=(
      const ChromeServiceWorkerFetchPPAPITest&) = delete;

 protected:
  ChromeServiceWorkerFetchPPAPITest() {}
  ~ChromeServiceWorkerFetchPPAPITest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeServiceWorkerFetchTest::SetUpCommandLine(command_line);
    // Use --enable-nacl flag to ensure the PNaCl module can load (without
    // needing to use an OT token)
    command_line->AppendSwitch(switches::kEnableNaCl);
  }

  void SetUpOnMainThread() override {
    base::FilePath document_root;
    ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&document_root));
    embedded_test_server()->AddDefaultHandlers(
        document_root.Append(FILE_PATH_LITERAL("nacl_test_data"))
            .Append(FILE_PATH_LITERAL("pnacl")));
    ChromeServiceWorkerFetchTest::SetUpOnMainThread();
    test_page_url_ = GetURL("/pnacl_url_loader.html");
  }

  std::string GetNavigationRequestString(const std::string& fragment) const {
    return RequestString(test_page_url_ + fragment, "navigate", "include", "");
  }

  std::string ExecutePNACLUrlLoaderTest(const std::string& mode) {
    content::DOMMessageQueue message_queue;
    EXPECT_TRUE(content::ExecJs(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::StringPrintf("reportOnFetch = false;"
                           "var iframe = document.createElement('iframe');"
                           "iframe.src='%s#%s';"
                           "document.body.appendChild(iframe);",
                           test_page_url_.c_str(), mode.c_str())));

    std::string json;
    EXPECT_TRUE(message_queue.WaitForMessage(&json));

    base::Value result =
        base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS).value();

    EXPECT_TRUE(result.is_string());
    EXPECT_EQ(base::StringPrintf("OnOpen%s", mode.c_str()), result.GetString());
    return EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                  "reportRequests();")
        .ExtractString();
  }

 private:
  std::string test_page_url_;
};

// Flaky on Windows and Linux ASan. https://crbug.com/1113802
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest,
                       DISABLED_NotInterceptedByServiceWorker) {
  // Only the navigation to the iframe should be intercepted by the service
  // worker. The request for the PNaCl manifest ("/pnacl_url_loader.nmf"),
  // the request for the compiled code ("/pnacl_url_loader_newlib_pnacl.pexe"),
  // and any other requests initiated by the plug-in ("/echo") should not be
  // seen by the service worker.
  const std::string fragment =
      "NotIntercepted";  // this string is not important.
  EXPECT_EQ(GetNavigationRequestString("#" + fragment),
            ExecutePNACLUrlLoaderTest(fragment));
}
#endif  // BUILDFLAG(ENABLE_NACL)

class ChromeServiceWorkerNavigationHintTest : public ChromeServiceWorkerTest {
 protected:
  void RunNavigationHintTest(
      const char* scope,
      content::StartServiceWorkerForNavigationHintResult expected_result,
      bool expected_started) {
    base::RunLoop run_loop;
    GURL url = embedded_test_server()->GetURL(scope);
    GetServiceWorkerContext()->StartServiceWorkerForNavigationHint(
        url, blink::StorageKey::CreateFirstParty(url::Origin::Create(url)),
        base::BindOnce(&ExpectResultAndRun<
                           content::StartServiceWorkerForNavigationHintResult>,
                       expected_result, run_loop.QuitClosure()));
    run_loop.Run();
    if (expected_started) {
      histogram_tester_.ExpectBucketCount(
          "ServiceWorker.StartWorker.Purpose",
          27 /* ServiceWorkerMetrics::EventType::NAVIGATION_HINT  */, 1);
      histogram_tester_.ExpectBucketCount(
          "ServiceWorker.StartWorker.StatusByPurpose_NAVIGATION_HINT",
          0 /* SERVICE_WORKER_OK  */, 1);
    } else {
      histogram_tester_.ExpectTotalCount("ServiceWorker.StartWorker.Purpose",
                                         0);
      histogram_tester_.ExpectTotalCount(
          "ServiceWorker.StartWorker.StatusByPurpose_NAVIGATION_HINT", 0);
    }
  }

  void TestStarted(const char* test_script) {
    WriteFile(FILE_PATH_LITERAL("sw.js"), "self.onfetch = function(e) {};");
    WriteFile(FILE_PATH_LITERAL("test.html"), test_script);
    InitializeServer();
    NavigateToPageAndWaitForReadyTitle("/test.html");
    GetServiceWorkerContext()->StopAllServiceWorkersForStorageKey(
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(embedded_test_server()->base_url())));
    RunNavigationHintTest(
        "/scope/", content::StartServiceWorkerForNavigationHintResult::STARTED,
        true);
  }

  void TestAlreadyRunning(const char* test_script) {
    WriteFile(FILE_PATH_LITERAL("sw.js"), "self.onfetch = function(e) {};");
    WriteFile(FILE_PATH_LITERAL("test.html"), test_script);
    InitializeServer();
    NavigateToPageAndWaitForReadyTitle("/test.html");
    RunNavigationHintTest(
        "/scope/",
        content::StartServiceWorkerForNavigationHintResult::ALREADY_RUNNING,
        false);
  }

  void TestNoFetchHandler(const char* test_script) {
    WriteFile(FILE_PATH_LITERAL("sw.js"), "/* empty */");
    WriteFile(FILE_PATH_LITERAL("test.html"), test_script);
    InitializeServer();
    NavigateToPageAndWaitForReadyTitle("/test.html");
    GetServiceWorkerContext()->StopAllServiceWorkersForStorageKey(
        blink::StorageKey::CreateFirstParty(
            url::Origin::Create(embedded_test_server()->base_url())));
    RunNavigationHintTest(
        "/scope/",
        content::StartServiceWorkerForNavigationHintResult::NO_FETCH_HANDLER,
        false);
  }
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest, Started) {
  TestStarted(kInstallAndWaitForActivatedPage);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest,
                       StartedModuleScript) {
  TestStarted(kInstallAndWaitForActivatedPageWithModuleScript);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest, AlreadyRunning) {
  TestAlreadyRunning(kInstallAndWaitForActivatedPage);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest,
                       AlreadyRunningModuleScript) {
  TestAlreadyRunning(kInstallAndWaitForActivatedPageWithModuleScript);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest,
                       NoServiceWorkerRegistration) {
  InitializeServer();
  RunNavigationHintTest("/scope/",
                        content::StartServiceWorkerForNavigationHintResult::
                            NO_SERVICE_WORKER_REGISTRATION,
                        false);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest,
                       NoActiveServiceWorkerVersion) {
  WriteFile(FILE_PATH_LITERAL("sw.js"),
            "self.oninstall = function(e) {\n"
            "    e.waitUntil(new Promise(r => { /* never resolve */ }));\n"
            "  };\n"
            "self.onfetch = function(e) {};");
  InitializeServer();
  base::RunLoop run_loop;
  blink::mojom::ServiceWorkerRegistrationOptions options(
      embedded_test_server()->GetURL("/scope/"),
      blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  const blink::StorageKey key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(options.scope));
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/sw.js"), key, options,
      base::BindOnce(&ExpectResultAndRun<blink::ServiceWorkerStatusCode>,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();
  RunNavigationHintTest("/scope/",
                        content::StartServiceWorkerForNavigationHintResult::
                            NO_ACTIVE_SERVICE_WORKER_VERSION,
                        false);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest, NoFetchHandler) {
  TestNoFetchHandler(kInstallAndWaitForActivatedPage);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest,
                       NoFetchHandlerModuleScript) {
  TestNoFetchHandler(kInstallAndWaitForActivatedPageWithModuleScript);
}

// URLDataSource that serves an empty page for all URLs except source/sw.js
// for which it serves valid service worker code.
class StaticURLDataSource : public content::URLDataSource {
 public:
  explicit StaticURLDataSource(const std::string& source) : source_(source) {}

  StaticURLDataSource(const StaticURLDataSource&) = delete;
  StaticURLDataSource& operator=(const StaticURLDataSource&) = delete;

  ~StaticURLDataSource() override = default;

  // content::URLDataSource:
  std::string GetSource() override { return source_; }
  void StartDataRequest(const GURL& url,
                        const content::WebContents::Getter& wc_getter,
                        GotDataCallback callback) override {
    // If it's the service worker url, serve a valid Service Worker.
    if (url.ExtractFileName() == "sw.js") {
      // Use a working script instead of an empty one, otherwise the worker
      // would fail to be registered.
      std::string data = R"(
        self.oninstall = function(e) {
          e.waitUntil(new Promise(r => { /* never resolve */ }));
        };
        self.onfetch = function(e) {};
       )";
      std::move(callback).Run(
          base::MakeRefCounted<base::RefCountedString>(std::move(data)));
      return;
    }

    // Otherwise, serve an empty page.
    std::move(callback).Run(
        base::MakeRefCounted<base::RefCountedString>(std::string()));
  }
  std::string GetMimeType(const GURL& url) override {
    if (url.ExtractFileName() == "sw.js")
      return "application/javascript";
    return "text/html";
  }
  bool ShouldAddContentSecurityPolicy() override { return false; }

 private:
  const std::string source_;
};

class StaticWebUIController : public content::WebUIController {
 public:
  StaticWebUIController(content::WebUI* web_ui, const std::string& key)
      : WebUIController(web_ui) {
    content::URLDataSource::Add(Profile::FromWebUI(web_ui),
                                std::make_unique<StaticURLDataSource>(key));
  }
  ~StaticWebUIController() override = default;
};

class TestWebUIConfig : public content::WebUIConfig {
 public:
  explicit TestWebUIConfig(std::string_view scheme, std::string_view host)
      : content::WebUIConfig(scheme, host) {
    data_source_key_ = this->host();
    if (this->scheme() == "chrome-untrusted") {
      data_source_key_ = this->scheme() + "://" + this->host() + "/";
    }
  }

  ~TestWebUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<StaticWebUIController>(web_ui, data_source_key_);
  }

  void RegisterURLDataSource(
      content::BrowserContext* browser_context) override {
    content::URLDataSource::Add(
        browser_context,
        std::make_unique<StaticURLDataSource>(data_source_key_));
  }

 private:
  std::string data_source_key_;
};

class ChromeWebUIServiceWorkerTest : public ChromeServiceWorkerTest {
 protected:
  // Creates a WebUI at `base_url` and registers a service worker for
  // it. Returns the result of registering the Service Worker.
  blink::ServiceWorkerStatusCode CreateWebUIAndRegisterServiceWorker(
      const GURL& base_url) {
    auto webui_config =
        std::make_unique<TestWebUIConfig>(base_url.scheme(), base_url.host());
    if (base_url.SchemeIs(content::kChromeUIScheme)) {
      content::WebUIConfigMap::GetInstance().AddWebUIConfig(
          std::move(webui_config));
    } else {
      content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
          std::move(webui_config));
    }

    // Try to register the service worker.
    const GURL service_worker_url = base_url.Resolve("sw.js");
    base::RunLoop run_loop;
    std::optional<blink::ServiceWorkerStatusCode> result;
    blink::mojom::ServiceWorkerRegistrationOptions options(
        base_url, blink::mojom::ScriptType::kClassic,
        blink::mojom::ServiceWorkerUpdateViaCache::kNone);
    const blink::StorageKey key = blink::StorageKey::CreateFirstParty(
        url::Origin::Create(service_worker_url));
    GetServiceWorkerContext()->RegisterServiceWorker(
        service_worker_url, key, options,
        base::BindLambdaForTesting([&](blink::ServiceWorkerStatusCode r) {
          result = r;
          run_loop.Quit();
        }));

    run_loop.Run();
    return result.value();
  }

  // Creates a WebUI at `base_url` and tries to register a service worker
  // for it in JavaScript. Returns "ServiceWorkerRegistered" if it succeeds,
  // otherwise it returns the error string.
  content::EvalJsResult CreateWebUIAndRegisterServiceWorkerInJavaScript(
      const GURL& base_url) {
    auto webui_config =
        std::make_unique<TestWebUIConfig>(base_url.scheme(), base_url.host());
    if (base_url.SchemeIs(content::kChromeUIScheme)) {
      content::WebUIConfigMap::GetInstance().AddWebUIConfig(
          std::move(webui_config));
    } else {
      content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
          std::move(webui_config));
    }

    CHECK(ui_test_utils::NavigateToURL(browser(), base_url));

    const GURL service_worker_url = base_url.Resolve("sw.js");
    const std::string register_script = base::StringPrintf(
        R"(
     (async () => {
       const init = {};
       init['scope'] = '%s';
       try {
         await navigator.serviceWorker.register('%s', init);
         await navigator.serviceWorker.ready;
         return "ServiceWorkerRegistered";
       } catch (e) {
         return e.message;
       }
     })()
    )",
        base_url.spec().c_str(), service_worker_url.spec().c_str());
    return EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                  register_script);
  }
};

// Tests that registering a service worker in JavaScript with a chrome:// URL
// fails.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerTest,
                       DisallowChromeSchemeInJavaScript) {
  const GURL base_url("chrome://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorkerInJavaScript(base_url);
  EXPECT_EQ(
      "Failed to register a ServiceWorker: The URL protocol of the "
      "current origin ('chrome://dummyurl') is not supported.",
      result);
}

// Tests that registering a service worker with a chrome:// URL fails.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerTest, DisallowChromeScheme) {
  const GURL base_url("chrome://dummyurl");

  // Registration should fail without the flag being set. See the tests
  // below, which set kEnableServiceWorkersForChromeScheme.
  auto result = CreateWebUIAndRegisterServiceWorker(base_url);
  EXPECT_EQ(result, blink::ServiceWorkerStatusCode::kErrorNetwork);
}

// Tests that registering a service worker in JavaScript with a
// chrome-untrusted:// URL fails.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerTest,
                       DisallowChromeUntrustedSchemeInJavaScript) {
  const GURL base_url("chrome-untrusted://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorkerInJavaScript(base_url);
  // Even when we add chrome-untrusted:// to the list of Service Worker schemes
  // we should fail to register it because the flag is not enabled.
  EXPECT_EQ(
      "Failed to register a ServiceWorker: The URL protocol of the "
      "current origin ('chrome-untrusted://dummyurl') is not supported.",
      result);
}

// Tests that registering a service worker with a chrome-untrusted:// URL fails
// if the flag is not enabled.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerTest,
                       DisllowChromeUntrustedScheme) {
  const GURL base_url("chrome-untrusted://dummyurl");

  // Similar to the chrome:// test above, but this fails with a kErrorNetwork
  // error. This is because chrome-untrusted:// is registered as a Service
  // Worker scheme but the loader factories are only added when the
  // kEnableServiceWorkersForChromeUntrusted feature is enabled.
  auto result = CreateWebUIAndRegisterServiceWorker(base_url);
  EXPECT_EQ(result, blink::ServiceWorkerStatusCode::kErrorNetwork);
}

class ChromeWebUIServiceWorkerFlagTest : public ChromeWebUIServiceWorkerTest {
 public:
  ChromeWebUIServiceWorkerFlagTest()
      : features_(features::kEnableServiceWorkersForChromeScheme) {}

 private:
  base::test::ScopedFeatureList features_;
};

// Tests that registering a service worker in JavaScript with a
// chrome:// URL fails even if the flag is enabled.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerFlagTest,
                       DisallowChromeSchemeInJavaScript) {
  const GURL base_url("chrome://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorkerInJavaScript(base_url);
  EXPECT_EQ(
      "Failed to register a ServiceWorker: The document is in an invalid "
      "state.",
      result);
}

// Tests that registering a service worker with a chrome-untrusted:// URL fails
// even if the flag is enabled.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerFlagTest,
                       DisallowChromeUntrustedScheme) {
  const GURL base_url("chrome-untrusted://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorker(base_url);
  EXPECT_EQ(result, blink::ServiceWorkerStatusCode::kErrorNetwork);
}

// Tests that registering a service worker with a chrome:// URL works
// if the flag is enabled.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerFlagTest, AllowChromeScheme) {
  const GURL base_url("chrome://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorker(base_url);
  EXPECT_EQ(result, blink::ServiceWorkerStatusCode::kOk);
}

// Tests that registering a service worker in JavaScript with a
// chrome-untrusted:// URL fails.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerFlagTest,
                       DisallowChromeUntrustedSchemeInJavaScript) {
  const GURL base_url("chrome-untrusted://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorkerInJavaScript(base_url);
  // We expect all WebUI Service Worker registrations to happen from C++
  // so this should fail even when the flag is enabled.
  EXPECT_EQ(
      "Failed to register a ServiceWorker: The URL protocol of the current "
      "origin ('chrome-untrusted://dummyurl') is not supported.",
      result);
}

class ChromeWebUIServiceWorkerUntrustedFlagTest
    : public ChromeWebUIServiceWorkerTest {
 public:
  ChromeWebUIServiceWorkerUntrustedFlagTest()
      : features_(features::kEnableServiceWorkersForChromeUntrusted) {}

 private:
  base::test::ScopedFeatureList features_;
};

// Tests that registering a service worker in JavaScript with a chrome:// URL
// fails even if the untrusted flag is enabled.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerUntrustedFlagTest,
                       DisallowChromeSchemeInJavaScript) {
  const GURL base_url("chrome://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorkerInJavaScript(base_url);
  EXPECT_EQ(
      "Failed to register a ServiceWorker: The URL protocol of the current "
      "origin ('chrome://dummyurl') is not supported.",
      result);
}

// Tests that registering a service worker with a chrome:// URL fails even
// if the untrusted flag is enabled.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerUntrustedFlagTest,
                       DisallowChromeScheme) {
  const GURL base_url("chrome://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorker(base_url);
  EXPECT_EQ(result, blink::ServiceWorkerStatusCode::kErrorNetwork);
}

// Tests that registering a service worker with a chrome-untrusted:// URL works
// if the flag is enabled.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerUntrustedFlagTest,
                       AllowChromeUntrustedScheme) {
  const GURL base_url("chrome-untrusted://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorker(base_url);
  EXPECT_EQ(result, blink::ServiceWorkerStatusCode::kOk);
}

// Tests that registering a service worker in JavaScript with a
// chrome-untrusted:// URL fails.
IN_PROC_BROWSER_TEST_F(ChromeWebUIServiceWorkerUntrustedFlagTest,
                       DisallowChromeUntrustedSchemeInJavaScript) {
  const GURL base_url("chrome-untrusted://dummyurl");
  auto result = CreateWebUIAndRegisterServiceWorkerInJavaScript(base_url);
  // We expect all WebUI Service Worker registrations to happen from C++
  // so this should fail even when the flag is enabled.
  EXPECT_EQ(
      "Failed to register a ServiceWorker: The document is in an "
      "invalid state.",
      result);
}

enum class ServicifiedFeatures { kNone, kServiceWorker, kNetwork };

// A simple fixture used for navigation preload tests so far. The fixture
// stashes the HttpRequest to a certain URL, useful for inspecting the headers
// to see if it was a navigation preload request and if it contained cookies.
//
// This is in //chrome instead of //content since the tests exercise the
// kBlockThirdPartyCookies preference which is not a //content concept.
class ChromeServiceWorkerNavigationPreloadTest : public InProcessBrowserTest {
 public:
  ChromeServiceWorkerNavigationPreloadTest() = default;

  ChromeServiceWorkerNavigationPreloadTest(
      const ChromeServiceWorkerNavigationPreloadTest&) = delete;
  ChromeServiceWorkerNavigationPreloadTest& operator=(
      const ChromeServiceWorkerNavigationPreloadTest&) = delete;

  void SetUp() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &ChromeServiceWorkerNavigationPreloadTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Make all hosts resolve to 127.0.0.1 so the same embedded test server can
    // be used for cross-origin URLs.
    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_test_server()->StartAcceptingConnections();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Intercept requests to the "test" endpoint.
    GURL url = request.base_url;
    url = url.Resolve(request.relative_url);
    if (url.path() != "/service_worker/test")
      return nullptr;

    // Stash the request for testing. We'd typically prefer to echo back the
    // request and test the resulting page contents, but that becomes
    // cumbersome if the test involves cross-origin frames.
    EXPECT_FALSE(received_request_);
    received_request_ = request;

    // Respond with OK.
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("OK");
    http_response->set_content_type("text/plain");
    return http_response;
  }

  bool HasHeader(const net::test_server::HttpRequest& request,
                 const std::string& name) const {
    return request.headers.find(name) != request.headers.end();
  }

  std::string GetHeader(const net::test_server::HttpRequest& request,
                        const std::string& name) const {
    const auto& iter = request.headers.find(name);
    EXPECT_TRUE(iter != request.headers.end());
    if (iter == request.headers.end())
      return std::string();
    return iter->second;
  }

  bool has_received_request() const { return received_request_.has_value(); }

  const net::test_server::HttpRequest& received_request() const {
    return *received_request_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  // The request that hit the "test" endpoint.
  std::optional<net::test_server::HttpRequest> received_request_;
};

// Tests navigation preload during a navigation in the top-level frame
// when third-party cookies are blocked. The navigation preload request
// should be sent with cookies as normal. Regression test for
// https://crbug.com/913220.
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationPreloadTest,
                       TopFrameWithThirdPartyBlocking) {
  // Enable third-party cookie blocking.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  // Load a page that registers a service worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('navigation_preload_worker.js');"));

  // Also set cookies.
  EXPECT_EQ("foo=bar",
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   "document.cookie = 'foo=bar'; document.cookie;"));

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/service_worker/test")));

  // The navigation preload request should have occurred and included cookies.
  ASSERT_TRUE(has_received_request());
  EXPECT_EQ("true",
            GetHeader(received_request(), "Service-Worker-Navigation-Preload"));
  EXPECT_EQ("foo=bar", GetHeader(received_request(), "Cookie"));
}

// Tests navigation preload during a navigation in a third-party iframe
// when third-party cookies are blocked. This blocks service worker as well,
// so the navigation preload request should not be sent. And the navigation
// request should not include cookies.
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationPreloadTest,
                       SubFrameWithThirdPartyBlocking) {
  // Enable third-party cookie blocking.
  browser()->profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));

  // Load a page that registers a service worker.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('navigation_preload_worker.js');"));

  // Also set cookies.
  EXPECT_EQ("foo=bar",
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   "document.cookie = 'foo=bar'; document.cookie;"));

  // Generate a cross-origin URL.
  GURL top_frame_url = embedded_test_server()->GetURL(
      "/service_worker/page_with_third_party_iframe.html");
  GURL::Replacements replacements;
  replacements.SetHostStr("cross-origin.example.com");
  top_frame_url = top_frame_url.ReplaceComponents(replacements);

  // Navigate to the page and embed a third-party iframe to the test
  // page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), top_frame_url));
  GURL iframe_url = embedded_test_server()->GetURL("/service_worker/test");
  EXPECT_EQ(true, EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         "addIframe('" + iframe_url.spec() + "');"));

  // The request should have been received. Because the navigation was for a
  // third-party iframe with cookies blocked, the service worker should not have
  // handled the request so navigation preload should not have occurred.
  // Likewise, the cookies should not have been sent.
  ASSERT_TRUE(has_received_request());
  EXPECT_FALSE(
      HasHeader(received_request(), "Service-Worker-Navigation-Preload"));
  EXPECT_FALSE(HasHeader(received_request(), "Cookie"));
}

}  // namespace chrome_service_worker_browser_test
