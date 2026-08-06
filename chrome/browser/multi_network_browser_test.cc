// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "net/base/isolation_info.h"
#include "net/base/network_handle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/install_default_websocket_handlers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration_options.mojom.h"

namespace {

using URLLoaderFactoryType =
    content::ContentBrowserClient::URLLoaderFactoryType;

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
// URLLoaderFactories for filesystem: and data: schemes are not backed by the
// network service. With that in mind, they should never be using the target
// network.
constexpr size_t kExpectedNonNetworkFactories = 2u;
#else
// On Desktop Android and Desktop platforms, when extensions are enabled,
// URLLoaderFactories for chrome-extension: scheme are also not backed by the
// network service, this means they will also not be using the target network.
constexpr size_t kExpectedNonNetworkFactories = 3u;
#endif

class TestChromeContentBrowserClient : public ChromeContentBrowserClient {
 public:
  TestChromeContentBrowserClient()
      : old_client_(content::SetBrowserClientForTesting(this)) {}
  ~TestChromeContentBrowserClient() override {
    content::SetBrowserClientForTesting(old_client_);
  }

  void WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      const net::IsolationInfo& isolation_info,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner,
      bool is_for_network_service) override {
    ChromeContentBrowserClient::WillCreateURLLoaderFactory(
        browser_context, frame, render_process_id, type, request_initiator,
        isolation_info, navigation_id, ukm_source_id, factory_builder,
        header_client, bypass_redirect_checks, disable_secure_dns,
        factory_override, navigation_response_task_runner,
        is_for_network_service);
    factory_bound_networks_[type].push_back(
        factory_builder.target_network_for_testing());
    factory_is_for_network_service_[type].push_back(is_for_network_service);
  }

  bool HasBoundNetwork(URLLoaderFactoryType type,
                       net::handles::NetworkHandle network) const {
    return CountBoundNetwork(type, network) > 0;
  }

  size_t CountBoundNetwork(URLLoaderFactoryType type,
                           net::handles::NetworkHandle network) const {
    auto it = factory_bound_networks_.find(type);
    if (it != factory_bound_networks_.end()) {
      return std::ranges::count(it->second, network);
    }
    return 0;
  }

  size_t CountBoundNetwork(net::handles::NetworkHandle network) const {
    size_t count = 0;
    for (const auto& [type, networks] : factory_bound_networks_) {
      count += std::ranges::count(networks, network);
    }
    return count;
  }

  bool HasIsForNetworkService(URLLoaderFactoryType type, bool expected) const {
    auto it = factory_is_for_network_service_.find(type);
    if (it != factory_is_for_network_service_.end()) {
      return std::ranges::contains(it->second, expected);
    }
    return false;
  }

 private:
  raw_ptr<content::ContentBrowserClient> old_client_;
  std::map<URLLoaderFactoryType, std::vector<net::handles::NetworkHandle>>
      factory_bound_networks_;
  std::map<URLLoaderFactoryType, std::vector<bool>>
      factory_is_for_network_service_;
};

constexpr auto kAllURLLoaderFactoryTypes = std::to_array({
    URLLoaderFactoryType::kNavigation,
    URLLoaderFactoryType::kDownload,
    URLLoaderFactoryType::kDocumentSubResource,
    URLLoaderFactoryType::kWorkerMainResource,
    URLLoaderFactoryType::kWorkerSubResource,
    URLLoaderFactoryType::kServiceWorkerScript,
    URLLoaderFactoryType::kServiceWorkerSubResource,
    URLLoaderFactoryType::kPrefetch,
    URLLoaderFactoryType::kDevTools,
    URLLoaderFactoryType::kEarlyHints,
});

struct ExpectedFactoryCounts {
  size_t navigation = 0;
  size_t download = 0;
  size_t document_subresource = 0;
  size_t worker_main_resource = 0;
  size_t worker_subresource = 0;
  size_t service_worker_script = 0;
  size_t service_worker_subresource = 0;
  size_t prefetch = 0;
  size_t devtools = 0;
  size_t early_hints = 0;
  // The number of URLLoaderFactories that are not targeting a specific network
  // is always expected to be the ones for schemes that are not supported by the
  // network service. There is no need to specify this in each test.
  size_t invalid_network_handle = kExpectedNonNetworkFactories;

  size_t count_for(URLLoaderFactoryType type) const {
    switch (type) {
      case URLLoaderFactoryType::kNavigation:
        return navigation;
      case URLLoaderFactoryType::kDownload:
        return download;
      case URLLoaderFactoryType::kDocumentSubResource:
        return document_subresource;
      case URLLoaderFactoryType::kWorkerMainResource:
        return worker_main_resource;
      case URLLoaderFactoryType::kWorkerSubResource:
        return worker_subresource;
      case URLLoaderFactoryType::kServiceWorkerScript:
        return service_worker_script;
      case URLLoaderFactoryType::kServiceWorkerSubResource:
        return service_worker_subresource;
      case URLLoaderFactoryType::kPrefetch:
        return prefetch;
      case URLLoaderFactoryType::kDevTools:
        return devtools;
      case URLLoaderFactoryType::kEarlyHints:
        return early_hints;
    }
  }
};

std::string_view URLLoaderFactoryTypeToString(URLLoaderFactoryType type) {
  switch (type) {
    case URLLoaderFactoryType::kNavigation:
      return "kNavigation";
    case URLLoaderFactoryType::kDownload:
      return "kDownload";
    case URLLoaderFactoryType::kDocumentSubResource:
      return "kDocumentSubResource";
    case URLLoaderFactoryType::kWorkerMainResource:
      return "kWorkerMainResource";
    case URLLoaderFactoryType::kWorkerSubResource:
      return "kWorkerSubResource";
    case URLLoaderFactoryType::kServiceWorkerScript:
      return "kServiceWorkerScript";
    case URLLoaderFactoryType::kServiceWorkerSubResource:
      return "kServiceWorkerSubResource";
    case URLLoaderFactoryType::kPrefetch:
      return "kPrefetch";
    case URLLoaderFactoryType::kDevTools:
      return "kDevTools";
    case URLLoaderFactoryType::kEarlyHints:
      return "kEarlyHints";
  }
}

void VerifyFactoryCounts(const TestChromeContentBrowserClient& test_client,
                         net::handles::NetworkHandle target_network,
                         const ExpectedFactoryCounts& expected) {
  size_t total_expected_bound_network = 0;

  for (auto type : kAllURLLoaderFactoryTypes) {
    SCOPED_TRACE(testing::Message()
                 << "\n"
                 << "type = " << URLLoaderFactoryTypeToString(type) << "\n");
    size_t expected_count = expected.count_for(type);
    EXPECT_EQ(expected_count,
              test_client.CountBoundNetwork(type, target_network));
    total_expected_bound_network += expected_count;

    if (expected_count > 0 &&
        type != URLLoaderFactoryType::kWorkerSubResource) {
      EXPECT_TRUE(test_client.HasIsForNetworkService(type, true));
    }
  }

  EXPECT_EQ(total_expected_bound_network,
            test_client.CountBoundNetwork(target_network));
  EXPECT_EQ(expected.invalid_network_handle,
            test_client.CountBoundNetwork(net::handles::kInvalidNetworkHandle));
}

}  // namespace

class MultiNetworkBrowserTest : public PlatformBrowserTest {
 public:
  void SetExpectedTargetNetworkForTesting(
      std::optional<net::handles::NetworkHandle> target_network) {
    GetProfile()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->SetExpectedTargetNetworkForTesting(target_network);
  }

  void TearDownOnMainThread() override {
    SetExpectedTargetNetworkForTesting(std::nullopt);
    PlatformBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest, NavigationSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 2;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  VerifyFactoryCounts(
      test_client, network,
      // 1 navigation factory for /title1.html, and 2 document subresource
      // factories (1 for the initial empty document created during WebContents
      // initialization, and 1 for /title1.html).
      {.navigation = 1, .document_subresource = 2});
}

IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest, SubresourceSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 3;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Issue a subresource fetch request from the document. The request reuses the
  // kDocumentSubResource URLLoaderFactory created during navigation commit.
  EXPECT_EQ("ok", content::EvalJs(
                      web_contents.get(),
                      "fetch('/title1.html').then(r => r.ok ? 'ok' : 'fail')"));

  VerifyFactoryCounts(
      test_client, network,
      // 1 navigation factory for /title1.html, and 2 document subresource
      // factories (1 for the initial empty document created during WebContents
      // initialization, and 1 for /title1.html). The subresource fetch does not
      // create a new URLLoaderFactory, but instead reuses the
      // kDocumentSubResource URLLoaderFactory created during navigation commit.
      {.navigation = 1, .document_subresource = 2});
}

IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest, DownloadSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 4;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::DownloadManager* download_manager =
      GetProfile()->GetDownloadManager();
  auto params =
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          web_contents.get(), url, TRAFFIC_ANNOTATION_FOR_TESTS);
  // Necessary to prevent the download to trigger checks for whether the
  // download is allowed.
  params->set_content_initiated(false);
  download_manager->DownloadUrl(std::move(params));
  // TODO(crbug.com/543377467): Wait for the download to complete and check it
  // succeeds. Rearchitecturing MultiNetworkBrowserTest to not rely on a CHECK
  // in CreateRequest will make it easier to do that.

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_client.CountBoundNetwork(URLLoaderFactoryType::kDownload,
                                         network) == 1;
  }));

  VerifyFactoryCounts(
      test_client, network,
      {.download = 1,
       // We are not triggering a navigation, so the kDocumentSubResource
       // URLLoaderFactories are not created. Only a single URLLoaderFactory for
       // kDownload is created.
       .invalid_network_handle = 0});
}

class PopupTestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  explicit PopupTestWebContentsDelegate(
      content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    content::WebContents::CreateParams create_params(browser_context_,
                                                     source->GetSiteInstance());
    create_params.target_network = source->GetTargetNetwork();
    opened_contents_ = content::WebContents::Create(create_params);
    content::TestNavigationObserver observer(opened_contents_.get());
    content::NavigationController::LoadURLParams load_params(params.url);
    opened_contents_->GetController().LoadURLWithParams(load_params);
    observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    return opened_contents_.get();
  }

  content::WebContents* AddNewContents(
      content::WebContents* source,
      std::unique_ptr<content::WebContents> new_contents,
      const GURL& target_url,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& window_features,
      bool user_gesture,
      bool* was_blocked) override {
    opened_contents_ = std::move(new_contents);
    return opened_contents_.get();
  }

  std::unique_ptr<content::WebContents> opened_contents_;
  raw_ptr<content::BrowserContext> browser_context_;
};

// Tests that when a popup is created by clicking on a link, the new tab
// correctly inherits target_network from the opener.
IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest,
                       OpenLinkInPopupSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 5;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  PopupTestWebContentsDelegate delegate(GetProfile());
  web_contents->SetDelegate(&delegate);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  content::OpenURLParams open_params(
      url, content::Referrer(), WindowOpenDisposition::NEW_POPUP,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
  content::WebContents* popup_contents =
      web_contents->OpenURL(open_params, /*navigation_handle_callback=*/{});
  ASSERT_TRUE(popup_contents);
  EXPECT_EQ(popup_contents, delegate.opened_contents_.get());
  EXPECT_EQ(network, popup_contents->GetTargetNetwork());
  content::WaitForLoadStop(popup_contents);

  VerifyFactoryCounts(
      test_client, network,
      // We are triggering 2 navigations, so we expect double the amount of
      // URLLoaderFactories to be created.
      {.navigation = 2,
       .document_subresource = 4,
       .invalid_network_handle = 2 * kExpectedNonNetworkFactories});
}

// Tests that when a popup is created by clicking on a link with rel=noopener,
// the new tab correctly inherits target_network from the opener, even though
// the opener relationship is later dropped.
IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest,
                       OpenLinkWithRelNoOpenerSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 12;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  PopupTestWebContentsDelegate delegate(GetProfile());
  web_contents->SetDelegate(&delegate);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  content::OpenURLParams open_params(
      url, content::Referrer(), WindowOpenDisposition::NEW_POPUP,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
  open_params.has_rel_opener = false;
  content::WebContents* popup_contents =
      web_contents->OpenURL(open_params, /*navigation_handle_callback=*/{});
  ASSERT_TRUE(popup_contents);
  EXPECT_EQ(network, popup_contents->GetTargetNetwork());

  VerifyFactoryCounts(
      test_client, network,
      // We trigger 2 navigations: one for the main window; the other for the
      // popup window, triggered via WebContents::OpenURL with
      // `has_rel_opener = false`. Because OpenURLFromTab creates a standalone
      // WebContents via WebContents::Create, an initial empty document is
      // created for each window (not only the main window), resulting in a
      // total of 4 document subresource factories (2 for main window, 2 for
      // popup window).
      {.navigation = 2,
       .document_subresource = 4,
       .invalid_network_handle = 2 * kExpectedNonNetworkFactories});
}

IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest,
                       DedicatedWorkerSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 6;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/worker.js") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/javascript");
          response->set_content(
              "importScripts('/imported.js');\n"
              "fetch('/fetch.txt').then(() => postMessage('done'));\n");
          return response;
        }
        if (request.relative_url == "/imported.js" ||
            request.relative_url == "/fetch.txt") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/javascript");
          response->set_content("// ok");
          return response;
        }
        return nullptr;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();

  EXPECT_EQ("done",
            content::EvalJs(web_contents.get(),
                            "new Promise(resolve => {"
                            "  const w = new Worker('/worker.js');"
                            "  w.onerror = (e) => resolve('err: ' + e.message);"
                            "  w.onmessage = (e) => resolve(e.data);"
                            "})"));

  VerifyFactoryCounts(test_client, network,
                      {.navigation = 1,
                       .document_subresource = 2,
                       .worker_main_resource = 1,
                       .worker_subresource = 1});
}

IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest, PrefetchSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 7;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  EXPECT_TRUE(content::EvalJs(web_contents.get(),
                              "const l = document.createElement('link'); l.rel "
                              "= 'prefetch'; l.href = "
                              "'/title2.html'; document.head.appendChild(l);")
                  .is_ok());

  VerifyFactoryCounts(test_client, network,
                      // 1 navigation factory for /title1.html, and 2 document
                      // subresource factories (1 for the initial empty document
                      // created during WebContents initialization, and 1 for
                      // /title1.html). The subresource fetch does not create a
                      // new URLLoaderFactory, but instead reuses the
                      // kDocumentSubResource URLLoaderFactory created during
                      // navigation commit.
                      {.navigation = 1, .document_subresource = 2});
}

// This test is not prescriptive, but rather demonstrates that service workers
// currently ignores the target_network.
IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest,
                       ServiceWorkerIgnoresTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 8;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/service_worker/generated_sw.js") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/javascript");
          response->set_content(
              "self.addEventListener('install', event => {"
              "event.waitUntil("
              "fetch('/sw_fetch.txt')"
              ".then(response => {"
              "if (!response.ok) {"
              "throw new Error('status ' + response.status);"
              "}"
              "return response.text();"
              "})"
              ");"
              "});");

          return response;
        }
        if (request.relative_url == "/sw_fetch.txt") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/plain");
          response->set_content("ok");
          return response;
        }
        return nullptr;
      }));

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Service worker script requests pass frame == nullptr to
  // WillCreateURLLoaderFactory, making it currently impossible to retrieve the
  // target_network from the frame. We reset SetExpectedTargetNetworkForTesting
  // to expect the default network (kInvalidNetworkHandle) so that
  // URLRequestContext::CreateRequest won't fail when fetching the service
  // worker script.
  SetExpectedTargetNetworkForTesting(std::nullopt);

  EXPECT_EQ(
      "ok",
      content::EvalJs(
          web_contents.get(),
          "navigator.serviceWorker.register('/service_worker/generated_sw.js')"
          ".then(() => 'ok').catch(e => 'err: ' + e.message)"));

  VerifyFactoryCounts(
      test_client, network,
      {.navigation = 1,
       .document_subresource = 2,
       .invalid_network_handle = kExpectedNonNetworkFactories + 4u});
}

// This test is not prescriptive, but rather demonstrates that WebSockets
// currently ignore the target_network.
IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest, WebSocketIgnoresTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 9;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  net::EmbeddedTestServer ws_server(net::EmbeddedTestServer::TYPE_HTTP);
  ws_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(ws_server.Start());

  GURL url = ws_server.GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // WebSocket requests currently don't retrieve the target_network from the
  // frame. Reset SetExpectedTargetNetworkForTesting to expect the default
  // network (kInvalidNetworkHandle) so that URLRequestContext::CreateRequest
  // won't fail when creating the WebSocket connection.
  // TODO(crbug.com/527777927): Support target_network in WebSockets and make
  // this test fail if WebSockets do not use the target_network.
  SetExpectedTargetNetworkForTesting(std::nullopt);

  GURL ws_url = net::test_server::GetWebSocketURL(ws_server, "/echo");
  std::string script = content::JsReplace(
      "new Promise(resolve => {"
      "  const ws = new WebSocket($1);"
      "  ws.onclose = () => resolve();"
      "});",
      ws_url.spec().c_str());
  EXPECT_TRUE(content::EvalJs(web_contents.get(), script).is_ok());

  VerifyFactoryCounts(test_client, network,
                      {.navigation = 1, .document_subresource = 2});
}

// This test is not prescriptive, but rather demonstrates that
// WebRTCPeerConnections currently ignore the target_network.
IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest,
                       WebRTCPeerConnectionIgnoresTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 10;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  // TODO(crbug.com/537268694): Consider supporting target_network in WebRTC.
  // SetExpectedTargetNetworkForTesting should also be extended to support
  // checking at the socket-layer, so that we can correctly surface that WebRTC
  // ignores the target_network.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  EXPECT_TRUE(content::EvalJs(web_contents.get(),
                              "new RTCPeerConnection({iceServers: [{urls: "
                              "'stun:127.0.0.1:3478'}]});")
                  .is_ok());

  VerifyFactoryCounts(test_client, network,
                      {.navigation = 1, .document_subresource = 2});
}

IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest, SubframeSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 11;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  content::TestNavigationObserver subframe_observer(web_contents.get());
  EXPECT_TRUE(
      content::EvalJs(
          web_contents.get(),
          "const iframe = document.createElement('iframe'); iframe.src = "
          "'/title2.html'; document.body.appendChild(iframe);")
          .is_ok());
  subframe_observer.Wait();
  EXPECT_TRUE(subframe_observer.last_navigation_succeeded());

  VerifyFactoryCounts(
      test_client, network,
      // We trigger 2 navigations (1 for main frame, 1 for iframe). Main frame
      // creates 2 document subresource factories (1 for initial empty document,
      // 1 for /title1.html), while the iframe directly commits /title2.html,
      // creating only 1 document subresource factory (total = 3).
      {.navigation = 2,
       .document_subresource = 3,
       .invalid_network_handle = 2 * kExpectedNonNetworkFactories});
}

// Tests that renderer-initiated window creation correctly inherits
// target_network from opener_rfh in WebContentsImpl::CreateWithOpener.
IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest, WindowOpenSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 16;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  PopupTestWebContentsDelegate delegate(GetProfile());
  web_contents->SetDelegate(&delegate);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  GURL popup_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationObserver popup_observer(popup_url);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(
      web_contents.get(), content::JsReplace("window.open($1);", popup_url)));
  popup_observer.Wait();
  EXPECT_TRUE(popup_observer.last_navigation_succeeded());

  ASSERT_TRUE(delegate.opened_contents_);
  // Verifies that target_network_ is inherited from opener_rfh in
  // WebContentsImpl::CreateWithOpener, along with window.opener.
  EXPECT_EQ(network, delegate.opened_contents_->GetTargetNetwork());
  EXPECT_EQ(true, content::EvalJs(delegate.opened_contents_.get(),
                                  "window.opener !== null"));

  VerifyFactoryCounts(
      test_client, network,
      // We trigger 2 navigations: one for the main window; the other for the
      // popup window, triggered via JS window.open(). The main window creates 2
      // document subresource factories (1 for the initial empty document, 1
      // for /title1.html), while the popup created with an opener only creates
      // 1 document subresource factory for /title2.html (total = 3).
      {.navigation = 2,
       .document_subresource = 3,
       .invalid_network_handle = 2 * kExpectedNonNetworkFactories});
}

// Tests that renderer-initiated window creation correctly inherits
// target_network from opener_rfh in WebContentsImpl::CreateWithOpener, even
// though the opener relationship is later dropped.
IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest,
                       WindowOpenNoOpenerSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 17;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  PopupTestWebContentsDelegate delegate(GetProfile());
  web_contents->SetDelegate(&delegate);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  GURL popup_url = embedded_test_server()->GetURL("/title2.html");
  content::TestNavigationObserver popup_observer(popup_url);
  popup_observer.StartWatchingNewWebContents();
  EXPECT_TRUE(content::ExecJs(
      web_contents.get(),
      content::JsReplace("window.open($1, '_blank', 'noopener');", popup_url)));
  popup_observer.Wait();
  EXPECT_TRUE(popup_observer.last_navigation_succeeded());

  ASSERT_TRUE(delegate.opened_contents_);
  // Verifies that target_network_ is inherited from opener_rfh in
  // WebContentsImpl::CreateWithOpener before the opener relationship is
  // disowned, resulting in window.opener === null in JavaScript while
  // preserving the target network.
  EXPECT_EQ(network, delegate.opened_contents_->GetTargetNetwork());
  EXPECT_EQ(true, content::EvalJs(delegate.opened_contents_.get(),
                                  "window.opener === null"));

  VerifyFactoryCounts(
      test_client, network,
      // We trigger 2 navigations: one for main window; the other for popup via
      // JS window.open(..., 'noopener')). Because 'noopener' disowns the opener
      // relationship, the popup window is created in a separate
      // BrowsingInstance with an initial empty document (about:blank) before
      // navigating to /title2.html. Thus, 4 document subresource factories are
      // created (2 for main window, 2 for popup window).
      {.navigation = 2,
       .document_subresource = 4,
       .invalid_network_handle = 2 * kExpectedNonNetworkFactories});
}

IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest, BeaconSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 13;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  EXPECT_EQ(true, content::EvalJs(web_contents.get(),
                                  "navigator.sendBeacon('/beacon')"));

  VerifyFactoryCounts(test_client, network,
                      {.navigation = 1, .document_subresource = 2});
}

// This test is not prescriptive, but rather demonstrates that shared workers
// currently ignore the target_network for subresources.
IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest,
                       SharedWorkerIgnoresTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 14;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/shared_worker.js") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/javascript");
          response->set_content(
              "onconnect = function(e) {\n"
              "  var port = e.ports[0];\n"
              "  port.postMessage('msg');\n"
              "};\n");
          return response;
        }
        return nullptr;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  EXPECT_EQ("msg",
            content::EvalJs(web_contents.get(),
                            "new Promise(resolve => {"
                            "  const w = new SharedWorker('/shared_worker.js');"
                            "  w.onerror = (e) => resolve('err: ' + e.message);"
                            "  w.port.onmessage = (e) => resolve(e.data);"
                            "})"));

  VerifyFactoryCounts(
      test_client, network,
      {.navigation = 1,
       .document_subresource = 2,
       // Created to fetch the shared worker script (/shared_worker.js).
       .worker_main_resource = 1,
       // Worker subresource requests are unbound to target_network because
       // SharedWorkerHost::CreateNetworkFactoryForSubresources passes frame =
       // nullptr to WillCreateURLLoaderFactory.
       .invalid_network_handle = kExpectedNonNetworkFactories + 1u});
}

IN_PROC_BROWSER_TEST_F(MultiNetworkBrowserTest,
                       PaymentRequestSetsTargetNetwork) {
  TestChromeContentBrowserClient test_client;

  constexpr net::handles::NetworkHandle network = 15;
  // This is necessary to prevent requests from failing due to a fake network
  // being used. See
  // net::URLRequestContext::set_expected_target_network_for_testing for more
  // information.
  SetExpectedTargetNetworkForTesting(network);

  content::WebContents::CreateParams create_params(GetProfile());
  create_params.target_network = network;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(create_params);

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/pay_manifest.json") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("application/json");
          response->set_content(
              "{\"default_applications\": [\"/app_manifest.json\"]}");
          return response;
        }
        if (request.relative_url == "/app_manifest.json") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("application/json");
          response->set_content("{\"name\": \"PayApp\"}");
          return response;
        }
        return nullptr;
      }));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");

  content::TestNavigationObserver observer(web_contents.get());
  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);
  observer.Wait();
  EXPECT_TRUE(observer.last_navigation_succeeded());

  GURL pay_method_url = embedded_test_server()->GetURL("/pay_manifest.json");

  std::string script = content::JsReplace(
      "const req = new PaymentRequest([{supportedMethods: $1}], {"
      "  total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}}"
      "});"
      "req.canMakePayment();",
      pay_method_url.spec());

  // PaymentRequest.canMakePayment() triggers manifest downloads using
  // CreateNetworkServiceDefaultFactory on RenderFrameHost.
  EXPECT_TRUE(content::EvalJs(web_contents.get(), script).is_ok());

  VerifyFactoryCounts(
      test_client, network,
      // 1 navigation factory for /title1.html, and 2 document subresource
      // factories (1 for the initial empty document created during WebContents
      // initialization, and 1 for /title1.html). Chrome's
      // PaymentManifestDownloader does not reuse the existing URLLoaderFactory
      // for subresources created during navigation commit. Instead, it uses a
      // dedicated URLLoaderFactory for each payment manifest download (created
      // via RenderFrameHost::CreateNetworkServiceDefaultFactory, which
      // correctly propagates the target_network because it calls
      // WillCreateURLLoaderFactory and passes that RenderFrameHost).
      {.navigation = 1, .document_subresource = 3});
}
