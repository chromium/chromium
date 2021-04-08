// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests that Service Workers (a Content feature) work in the Chromium
// embedder.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
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
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "third_party/blink/public/common/messaging/string_message_codec.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

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
 protected:
  ChromeServiceWorkerTest() {
    EXPECT_TRUE(service_worker_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(base::CreateDirectoryAndGetError(
        service_worker_dir_.GetPath().Append(
            FILE_PATH_LITERAL("scope")), nullptr));
  }
  ~ChromeServiceWorkerTest() override {}

  void WriteFile(const base::FilePath::StringType& filename,
                 base::StringPiece contents) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_EQ(base::checked_cast<int>(contents.size()),
              base::WriteFile(service_worker_dir_.GetPath().Append(filename),
                              contents.data(), contents.size()));
  }

  void NavigateToPageAndWaitForReadyTitle(const std::string path) {
    const std::u16string expected_title1 = u"READY";
    content::TitleWatcher title_watcher1(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title1);
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(path));
    EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());
  }

  void InitializeServer() {
    embedded_test_server()->ServeFilesFromDirectory(
        service_worker_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::ServiceWorkerContext* GetServiceWorkerContext() {
    return content::BrowserContext::GetDefaultStoragePartition(
               browser()->profile())
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

    GetServiceWorkerContext()->StopAllServiceWorkersForOrigin(
        url::Origin::Create(embedded_test_server()->base_url()));
    HostContentSettingsMapFactory::GetForProfile(browser()->profile())
        ->SetDefaultContentSetting(ContentSettingsType::JAVASCRIPT,
                                   CONTENT_SETTING_BLOCK);

    const std::u16string expected_title = u"Done";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/scope/done.html"));

    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    content::RenderFrameHost* main_frame =
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
    EXPECT_TRUE(
        content_settings::PageSpecificContentSettings::GetForFrame(main_frame)
            ->IsContentBlocked(ContentSettingsType::JAVASCRIPT));
  }

  void TestStartServiceWorkerAndDispatchMessage(const char* test_script) {
    base::RunLoop run_loop;
    blink::TransferableMessage msg;
    const std::u16string message_data = u"testMessage";

    WriteFile(FILE_PATH_LITERAL("sw.js"), "self.onfetch = function(e) {};");
    WriteFile(FILE_PATH_LITERAL("test.html"), test_script);

    InitializeServer();
    NavigateToPageAndWaitForReadyTitle("/test.html");
    msg.owned_encoded_message = blink::EncodeStringMessage(message_data);
    msg.encoded_message = msg.owned_encoded_message;

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&content::ServiceWorkerContext::
                           StartServiceWorkerAndDispatchMessage,
                       base::Unretained(GetServiceWorkerContext()),
                       embedded_test_server()->GetURL("/scope/"),
                       std::move(msg),
                       base::BindRepeating(&ExpectResultAndRun<bool>, true,
                                           run_loop.QuitClosure())));
    run_loop.Run();
  }

  base::ScopedTempDir service_worker_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerTest);
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
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/service_worker.js"), options,
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
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/service_worker.js"), options,
      base::BindOnce(&ExpectResultAndRun<blink::ServiceWorkerStatusCode>,
                     blink::ServiceWorkerStatusCode::kOk,
                     run_loop.QuitClosure()));
  run_loop.Run();

  ui_test_utils::NavigateToURL(incognito,
                               embedded_test_server()->GetURL("/test.html"));

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
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/service_worker.js"), options,
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

class ChromeServiceWorkerFetchTest : public ChromeServiceWorkerTest {
 protected:
  ChromeServiceWorkerFetchTest() {}
  ~ChromeServiceWorkerFetchTest() override {}

  void SetUpOnMainThread() override {
    WriteServiceWorkerFetchTestFiles();
    embedded_test_server()->ServeFilesFromDirectory(
        service_worker_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());
    InitializeServiceWorkerFetchTestPage();
  }

  std::string ExecuteScriptAndExtractString(const std::string& js) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), js, &result));
    return result;
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
              "var reportOnFetch = true;"
              "var issuedRequests = [];"
              "function reportRequests() {"
              "  var str = '';"
              "  issuedRequests.forEach(function(data) {"
              "      str += data + '\\n';"
              "    });"
              "  window.domAutomationController.send(str);"
              "}"
              "navigator.serviceWorker.addEventListener("
              "    'message',"
              "    function(event) {"
              "      issuedRequests.push(event.data);"
              "      if (reportOnFetch) {"
              "        reportRequests();"
              "      }"
              "    }, false);"
              "</script>");
  }

  void InitializeServiceWorkerFetchTestPage() {
    // The message "READY" will be sent when the service worker is activated.
    const std::u16string expected_title = u"READY";
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL("/test.html"));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerFetchTest);
};

class FaviconUpdateWaiter : public favicon::FaviconDriverObserver {
 public:
  explicit FaviconUpdateWaiter(content::WebContents* web_contents) {
    scoped_observer_.Add(
        favicon::ContentFaviconDriver::FromWebContents(web_contents));
  }
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
  ScopedObserver<favicon::FaviconDriver, favicon::FaviconDriverObserver>
      scoped_observer_{this};
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(FaviconUpdateWaiter);
};

class ChromeServiceWorkerLinkFetchTest : public ChromeServiceWorkerFetchTest {
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
    return ExecuteScriptAndExtractString("reportRequests();");
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
        ->GetMainFrame()
        ->ExecuteJavaScriptForTests(
            base::ASCIIToUTF16(js),
            base::BindOnce(
                [](base::OnceClosure quit_callback, base::Value result) {
                  std::move(quit_callback).Run();
                },
                run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::string GetManifestAndIssuedRequests() {
    base::RunLoop run_loop;
    browser()->tab_strip_model()->GetActiveWebContents()->GetManifest(
        base::BindOnce(&ManifestCallbackAndRun, run_loop.QuitClosure()));
    run_loop.Run();
    return ExecuteScriptAndExtractString(
        "if (issuedRequests.length != 0) reportRequests();"
        "else reportOnFetch = true;");
  }

  static void ManifestCallbackAndRun(base::OnceClosure continuation,
                                     const GURL&,
                                     const blink::Manifest&) {
    std::move(continuation).Run();
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerLinkFetchTest);
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
    std::string result(ExecuteScriptAndExtractString(
        base::StringPrintf("reportOnFetch = false;"
                           "var iframe = document.createElement('iframe');"
                           "iframe.src='%s#%s';"
                           "document.body.appendChild(iframe);",
                           test_page_url_.c_str(), mode.c_str())));
    EXPECT_EQ(base::StringPrintf("OnOpen%s", mode.c_str()), result);
    return ExecuteScriptAndExtractString("reportRequests();");
  }

 private:
  std::string test_page_url_;

  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerFetchPPAPITest);
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
    GetServiceWorkerContext()->StartServiceWorkerForNavigationHint(
        embedded_test_server()->GetURL(scope),
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
    histogram_tester_.ExpectBucketCount(
        "ServiceWorker.StartForNavigationHint.Result",
        static_cast<int>(expected_result), 1);
  }

  void TestStarted(const char* test_script) {
    WriteFile(FILE_PATH_LITERAL("sw.js"), "self.onfetch = function(e) {};");
    WriteFile(FILE_PATH_LITERAL("test.html"), test_script);
    InitializeServer();
    NavigateToPageAndWaitForReadyTitle("/test.html");
    GetServiceWorkerContext()->StopAllServiceWorkersForOrigin(
        url::Origin::Create(embedded_test_server()->base_url()));
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
    GetServiceWorkerContext()->StopAllServiceWorkersForOrigin(
        url::Origin::Create(embedded_test_server()->base_url()));
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
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/sw.js"), options,
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

// Copied from devtools_browsertest.cc.
class StaticURLDataSource : public content::URLDataSource {
 public:
  StaticURLDataSource(const std::string& source, const std::string& content)
      : source_(source), content_(content) {}
  ~StaticURLDataSource() override = default;

  // content::URLDataSource:
  std::string GetSource() override { return source_; }
  void StartDataRequest(const GURL& url,
                        const content::WebContents::Getter& wc_getter,
                        GotDataCallback callback) override {
    std::string data(content_);
    std::move(callback).Run(base::RefCountedString::TakeString(&data));
  }
  std::string GetMimeType(const std::string& path) override {
    return "application/javascript";
  }
  bool ShouldAddContentSecurityPolicy() override { return false; }

 private:
  const std::string source_;
  const std::string content_;

  DISALLOW_COPY_AND_ASSIGN(StaticURLDataSource);
};

// Copied from devtools_browsertest.cc.
class MockWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MockWebUIProvider(const std::string& source, const std::string& content)
      : source_(source), content_(content) {}
  ~MockWebUIProvider() override = default;

  std::unique_ptr<content::WebUIController> NewWebUI(content::WebUI* web_ui,
                                                     const GURL& url) override {
    content::URLDataSource::Add(
        Profile::FromWebUI(web_ui),
        std::make_unique<StaticURLDataSource>(source_, content_));
    return std::make_unique<content::WebUIController>(web_ui);
  }

 private:
  const std::string source_;
  const std::string content_;
  DISALLOW_COPY_AND_ASSIGN(MockWebUIProvider);
};

// Tests that registering a service worker with a chrome:// URL fails.
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest, DisallowChromeScheme) {
  const GURL kScript("chrome://dummyurl/sw.js");
  const GURL kScope("chrome://dummyurl");

  // Make chrome://dummyurl/sw.js serve a service worker script.
  TestChromeWebUIControllerFactory test_factory;
  MockWebUIProvider mock_provider("serviceworker", "// empty service worker");
  test_factory.AddFactoryOverride(kScript.host(), &mock_provider);
  content::WebUIControllerFactory::RegisterFactory(&test_factory);

  // Try to register the service worker.
  base::RunLoop run_loop;
  blink::ServiceWorkerStatusCode result = blink::ServiceWorkerStatusCode::kOk;
  blink::mojom::ServiceWorkerRegistrationOptions options(
      kScope, blink::mojom::ScriptType::kClassic,
      blink::mojom::ServiceWorkerUpdateViaCache::kImports);
  GetServiceWorkerContext()->RegisterServiceWorker(
      kScript, options,
      base::BindOnce(
          [](base::OnceClosure quit_closure,
             blink::ServiceWorkerStatusCode* out_result,
             blink::ServiceWorkerStatusCode result) {
            *out_result = result;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &result));
  run_loop.Run();

  // Registration should fail. This is the desired behavior. At the time of this
  // writing, there are a few reasons the registration fails:
  // * OriginCanAccessServiceWorkers() returns false for the "chrome" scheme.
  // * Even if that returned true, the URL loader factory bundle used to make
  //   the resource request in ServiceWorkerNewScriptLoader doesn't support
  //   the "chrome" scheme. This is because:
  //     * The call to RegisterNonNetworkSubresourceURLLoaderFactories() from
  //       CreateFactoryBundle() in embedded_worker_instance.cc doesn't register
  //       the "chrome" scheme, because there is no frame/web_contents.
  //     * Even if that registered a factory, CreateFactoryBundle() would
  //       skip it because GetServiceWorkerSchemes() doesn't include "chrome".
  //
  // It's difficult to change all these, so the test author hasn't actually
  // changed Chrome in a way that makes this test fail, to prove that the test
  // would be effective at catching a regression.
  EXPECT_EQ(result, blink::ServiceWorkerStatusCode::kErrorInvalidArguments);
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
  base::Optional<net::test_server::HttpRequest> received_request_;

  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerNavigationPreloadTest);
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
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('navigation_preload_worker.js');"));

  // Also set cookies.
  EXPECT_EQ("foo=bar",
            EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                   "document.cookie = 'foo=bar'; document.cookie;"));

  // Load the test page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/service_worker/test"));

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
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
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
  ui_test_utils::NavigateToURL(browser(), top_frame_url);
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
