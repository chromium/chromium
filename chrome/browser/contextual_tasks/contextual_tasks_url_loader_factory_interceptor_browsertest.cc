// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_url_loader_factory_interceptor.h"

#include "base/test/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace contextual_tasks {

namespace {

const char kTestEmail[] = "test@example.com";
const char kTestHost[] = "www.google.com";
const char kTestSubHost[] = "sub.google.com";

}  // namespace

class ContextualTasksUrlLoaderFactoryInterceptorBrowserTest
    : public InProcessBrowserTest {
 public:
  ContextualTasksUrlLoaderFactoryInterceptorBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksForceEntryPointEligibility},
        {});
  }
  ~ContextualTasksUrlLoaderFactoryInterceptorBrowserTest() override = default;

  void SetUp() override {
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ContextualTasksUrlLoaderFactoryInterceptorBrowserTest::HandleRequest,
        base::Unretained(this)));
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ContextualTasksUrlLoaderFactoryInterceptorBrowserTest::
            HandlePreflightRetryRequest,
        base::Unretained(this)));
    https_server_.RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url.find("/echoheader") != std::string::npos ||
              request.relative_url.find("/preflight-retry") !=
                  std::string::npos) {
            return nullptr;
          }
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          return response;
        }));
    ASSERT_TRUE(https_server_.InitializeAndListen());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("ignore-certificate-errors");
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksUrlLoaderFactoryInterceptorBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule(kTestHost, "127.0.0.1");
    host_resolver()->AddRule(kTestSubHost, "127.0.0.1");
    https_server_.StartAcceptingConnections();

    // Sign in.
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_environment_adaptor_->identity_test_env()
        ->MakePrimaryAccountAvailable(kTestEmail,
                                      signin::ConsentLevel::kSignin);
    identity_test_environment_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void TearDownOnMainThread() override {
    identity_test_environment_adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    contextual_tasks::ContextualTasksServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<testing::NiceMock<
                      contextual_tasks::MockContextualTasksService>>();
                }));
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.find("/echoheader?Authorization") !=
        std::string::npos) {
      auto it = request.headers.find("Authorization");
      std::string auth_header_value;
      if (it != request.headers.end()) {
        auth_header_value = it->second;
      }

      auto it_ua = request.headers.find("Sec-CH-UA-Full-Version-List");
      std::string ua_header_value;
      if (it_ua != request.headers.end()) {
        ua_header_value = it_ua->second;
      }

      if (!auth_header_value.empty() || !ua_header_value.empty() ||
          request.relative_url.find("onegoogle") != std::string::npos) {
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ContextualTasksUrlLoaderFactoryInterceptorBrowserTest::
                    OnHeadersCaptured,
                base::Unretained(this), auth_header_value, ua_header_value));
      }
      return std::make_unique<net::test_server::BasicHttpResponse>();
    }
    return nullptr;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandlePreflightRetryRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url.find("/preflight-retry") == std::string::npos) {
      return nullptr;
    }

    if (request.method == net::test_server::METHOD_OPTIONS) {
      // Return a redirect to trigger kPreflightDisallowedRedirect.
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);  // 302
      response->AddCustomHeader("Location", "/somewhere-else");
      return response;
    }

    if (request.method == net::test_server::METHOD_GET) {
      // Verify Authorization header is missing.
      auto it = request.headers.find("Authorization");
      if (it == request.headers.end()) {
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ContextualTasksUrlLoaderFactoryInterceptorBrowserTest::
                    OnRetrySuccess,
                base::Unretained(this)));
      }
      return std::make_unique<net::test_server::BasicHttpResponse>();
    }
    return nullptr;
  }

  void OnHeadersCaptured(const std::string& auth_header,
                         const std::string& ua_header) {
    captured_auth_header_ = auth_header;
    captured_ua_header_ = ua_header;
    if (header_capture_quit_closure_) {
      std::move(header_capture_quit_closure_).Run();
    }
  }

  void OnRetrySuccess() {
    if (retry_test_success_closure_) {
      std::move(retry_test_success_closure_).Run();
    }
  }

 protected:
  std::string captured_auth_header_;
  std::string captured_ua_header_;
  base::OnceClosure header_capture_quit_closure_;
  base::OnceClosure retry_test_success_closure_;
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
  content::ContentMockCertVerifier mock_cert_verifier_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksUrlLoaderFactoryInterceptorBrowserTest,
                       AuthorizationHeaderInjected) {
  base::RunLoop run_loop;
  header_capture_quit_closure_ = run_loop.QuitClosure();

  // Navigate to the Contextual Tasks WebUI.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));

  // Wait for the WebUI to load and create the webview.
  content::WebContents* web_ui_contents =
      TabListInterface::From(browser())->GetActiveTab()->GetContents();

  // Script to find the webview and navigate it.
  // Note: We access the shadowRoot of the app.
  std::string script = content::JsReplace(
      R"(
    (async () => {
      let app = document.querySelector('contextual-tasks-app');
      while (!app) {
        await new Promise(r => setTimeout(r, 100));
        app = document.querySelector('contextual-tasks-app');
      }
      // Wait for shadow root
      while (!app.shadowRoot) {
        await new Promise(r => setTimeout(r, 100));
      }
      // Wait for threadFrame
      let webview = app.shadowRoot.querySelector('#threadFrame');
      while (!webview) {
        await new Promise(r => setTimeout(r, 100));
        webview = app.shadowRoot.querySelector('#threadFrame');
      }
      webview.src = $1;
    })();
  )",
      https_server_.GetURL(kTestHost, "/echoheader?Authorization").spec());

  EXPECT_TRUE(content::ExecJs(web_ui_contents, script));

  // Wait for the request to reach the server.
  run_loop.Run();

  // Verify the header. The IdentityTestEnvironment issues tokens like
  // "access_token_...".
  EXPECT_THAT(captured_auth_header_,
              testing::StartsWith("Bearer access_token"));
}

class
    ContextualTasksUrlLoaderFactoryInterceptorFullVersionListDisabledBrowserTest
    : public ContextualTasksUrlLoaderFactoryInterceptorBrowserTest {
 public:
  ContextualTasksUrlLoaderFactoryInterceptorFullVersionListDisabledBrowserTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksForceEntryPointEligibility},
        {kContextualTasksSendFullVersionListEnabled});
  }
};

class ContextualTasksUrlLoaderFactoryInterceptorFullVersionListBrowserTest
    : public ContextualTasksUrlLoaderFactoryInterceptorBrowserTest {
 public:
  ContextualTasksUrlLoaderFactoryInterceptorFullVersionListBrowserTest() {
    feature_list_.Reset();
    feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksForceEntryPointEligibility,
         kContextualTasksSendFullVersionListEnabled},
        {});
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksUrlLoaderFactoryInterceptorFullVersionListDisabledBrowserTest,
    FullVersionListHeaderNotInjected) {
  base::RunLoop run_loop;
  header_capture_quit_closure_ = run_loop.QuitClosure();

  // Navigate to the Contextual Tasks WebUI.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));

  content::WebContents* web_ui_contents =
      TabListInterface::From(browser())->GetActiveTab()->GetContents();

  std::string script = content::JsReplace(
      R"(
    (async () => {
      let app = document.querySelector('contextual-tasks-app');
      while (!app) {
        await new Promise(r => setTimeout(r, 100));
        app = document.querySelector('contextual-tasks-app');
      }
      while (!app.shadowRoot) {
        await new Promise(r => setTimeout(r, 100));
      }
      let webview = app.shadowRoot.querySelector('#threadFrame');
      while (!webview) {
        await new Promise(r => setTimeout(r, 100));
        webview = app.shadowRoot.querySelector('#threadFrame');
      }
      webview.src = $1;
    })();
  )",
      https_server_.GetURL(kTestHost, "/echoheader?Authorization").spec());

  EXPECT_TRUE(content::ExecJs(web_ui_contents, script));

  run_loop.Run();

  EXPECT_TRUE(captured_ua_header_.empty());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksUrlLoaderFactoryInterceptorFullVersionListBrowserTest,
    FullVersionListHeaderInjected) {
  base::RunLoop run_loop;
  header_capture_quit_closure_ = run_loop.QuitClosure();

  // Navigate to the Contextual Tasks WebUI.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));

  content::WebContents* web_ui_contents =
      TabListInterface::From(browser())->GetActiveTab()->GetContents();

  std::string script = content::JsReplace(
      R"(
    (async () => {
      let app = document.querySelector('contextual-tasks-app');
      while (!app) {
        await new Promise(r => setTimeout(r, 100));
        app = document.querySelector('contextual-tasks-app');
      }
      while (!app.shadowRoot) {
        await new Promise(r => setTimeout(r, 100));
      }
      let webview = app.shadowRoot.querySelector('#threadFrame');
      while (!webview) {
        await new Promise(r => setTimeout(r, 100));
        webview = app.shadowRoot.querySelector('#threadFrame');
      }
      webview.src = $1;
    })();
  )",
      https_server_.GetURL(kTestHost, "/echoheader?Authorization").spec());

  EXPECT_TRUE(content::ExecJs(web_ui_contents, script));

  run_loop.Run();

  EXPECT_FALSE(captured_ua_header_.empty());
  EXPECT_THAT(captured_ua_header_, testing::HasSubstr("Chromium"));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUrlLoaderFactoryInterceptorBrowserTest,
                       PreflightRedirectTriggersRetry) {
  base::RunLoop run_loop;
  retry_test_success_closure_ = run_loop.QuitClosure();

  // Navigate to the Contextual Tasks WebUI.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));

  // Wait for the WebUI to load and create the webview.
  content::WebContents* web_ui_contents =
      TabListInterface::From(browser())->GetActiveTab()->GetContents();

  // Script to find the webview and perform a cross-origin fetch.
  // We navigate the webview to about:blank (no interception), then
  // fetch from "sub.google.com" (interception enabled).
  GURL fetch_url = https_server_.GetURL(kTestSubHost, "/preflight-retry");

  std::string script = content::JsReplace(
      R"(
    (async () => {
      const waitFor = (selector, scope = document) => {
        return new Promise(resolve => {
          if (scope.querySelector(selector)) {
            return resolve(scope.querySelector(selector));
          }
          const observer = new MutationObserver(() => {
            if (scope.querySelector(selector)) {
              observer.disconnect();
              resolve(scope.querySelector(selector));
            }
          });
          observer.observe(scope, {childList: true, subtree: true});
        });
      };

      const app = await waitFor('contextual-tasks-app');
      if (!app.shadowRoot) {
        await customElements.whenDefined('contextual-tasks-app');
      }
      const webview = await waitFor('#threadFrame', app.shadowRoot);

      // Navigate webview first
      const targetUrl = 'data:text/html,<html><body></body></html>';
      webview.src = targetUrl;

      // Wait for load
      await new Promise((resolve, reject) => {
        const stop = () => {
            webview.removeEventListener('loadstop', stop);
            webview.removeEventListener('loadabort', abort);
            resolve();
        };
        const abort = (e) => {
            if (e.url === targetUrl) {
                webview.removeEventListener('loadstop', stop);
                webview.removeEventListener('loadabort', abort);
                reject('Load aborted for ' + e.url + ': ' + e.reason);
            }
        };
        webview.addEventListener('loadstop', stop);
        webview.addEventListener('loadabort', abort);
      });

      // Execute fetch inside webview
      webview.executeScript({code: `fetch($1, {mode: 'cors'});`});
    })();
  )",
      fetch_url.spec());

  EXPECT_TRUE(content::ExecJs(web_ui_contents, script));

  // Wait for the retry request to reach the server.
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUrlLoaderFactoryInterceptorBrowserTest,
                       OneGoogleUrlDoesNotHaveAuthToken) {
  base::RunLoop run_loop;
  header_capture_quit_closure_ = run_loop.QuitClosure();

  // Navigate to the Contextual Tasks WebUI.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));

  // Wait for the WebUI to load and create the webview.
  content::WebContents* web_ui_contents =
      TabListInterface::From(browser())->GetActiveTab()->GetContents();

  // Script to find the webview and navigate it.
  // Note: We access the shadowRoot of the app.
  std::string script = content::JsReplace(
      R"(
    (async () => {
      let app = document.querySelector('contextual-tasks-app');
      while (!app) {
        await new Promise(r => setTimeout(r, 100));
        app = document.querySelector('contextual-tasks-app');
      }
      // Wait for shadow root
      while (!app.shadowRoot) {
        await new Promise(r => setTimeout(r, 100));
      }
      // Wait for threadFrame
      let webview = app.shadowRoot.querySelector('#threadFrame');
      while (!webview) {
        await new Promise(r => setTimeout(r, 100));
        webview = app.shadowRoot.querySelector('#threadFrame');
      }
      webview.src = $1;
    })();
  )",
      https_server_.GetURL(kTestHost, "/onegoogle/echoheader?Authorization")
          .spec());

  EXPECT_TRUE(content::ExecJs(web_ui_contents, script));

  // Wait for the request to reach the server.
  run_loop.Run();

  // Verify the header.
  EXPECT_TRUE(captured_auth_header_.empty());
}

}  // namespace contextual_tasks
