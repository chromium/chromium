// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

// Key and ID for an allowlisted MV3 extension permitted to use the
// "webRequestBlocking" permission.
constexpr char kAllowlistedExtensionKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8xv6iO+j4kzj1HiBL93+XVJH/CRyAQMUHS/"
    "Z0l8nCAzaAFkW/JsNwxJqQhrZspnxLqbQxNncXs6g6bsXAwKHiEs+LSs+bIv0Gc/2ycZdhXJ8G"
    "hEsSMakog5dpQd1681c2gLK/8CrAoewE/0GIKhaFcp7a2iZlGh4Am6fgMKy0iQIDAQAB";
constexpr char kAllowlistedExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

}  // namespace

// Test fixture for validation of URLLoader::FollowRedirect() in
// WebRequestProxyingURLLoaderFactory. Regression coverage for
// crbug.com/497494634, where a compromised renderer could send an unsolicited
// FollowRedirect() to swap the request URL out from under a pending blocking
// webRequest.onBeforeRequest listener, reaching a URL the listener never saw.
//
// These tests drive the frame's URLLoaderFactory directly, which is what a
// compromised renderer has access to.
class WebRequestProxyingURLLoaderFactoryFollowRedirectTest
    : public ExtensionApiTest {
 public:
  WebRequestProxyingURLLoaderFactoryFollowRedirectTest() = default;
  ~WebRequestProxyingURLLoaderFactoryFollowRedirectTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kAllowlistedExtensionId);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    // Record every request that actually reaches the server, so that tests can
    // assert a rejected FollowRedirect() never made it onto the wire. Must be
    // registered before the server starts; runs on the test server's thread.
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &WebRequestProxyingURLLoaderFactoryFollowRedirectTest::MonitorRequest,
        base::Unretained(this)));
    ASSERT_TRUE(StartEmbeddedTestServer());

    // Proxy requests even when no extension has registered a listener yet, so
    // that the tests which don't load an extension still exercise the proxy.
    ForceProxy();

    ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                              embedded_test_server()->GetURL("/simple.html")));
    initiator_origin_ =
        GetActiveWebContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  }

 protected:
  void ForceProxy() {
    auto* web_request_api =
        BrowserContextKeyedAPIFactory<WebRequestAPI>::Get(profile());
    ASSERT_TRUE(web_request_api);
    web_request_api->ForceProxyForTesting();
    profile()->GetDefaultStoragePartition()->FlushNetworkInterfaceForTesting();
  }

  // Binds a URLLoaderFactory equivalent to the one the renderer holds, i.e.
  // one wrapped by WebRequestProxyingURLLoaderFactory. Must be called after any
  // extension is loaded, since loading an extension re-creates the factory.
  mojo::Remote<network::mojom::URLLoaderFactory> CreateFrameFactory() {
    mojo::Remote<network::mojom::URLLoaderFactory> factory;
    EXPECT_TRUE(GetActiveWebContents()
                    ->GetPrimaryMainFrame()
                    ->CreateNetworkServiceDefaultFactory(
                        factory.BindNewPipeAndPassReceiver()));
    return factory;
  }

  void StartRequest(network::mojom::URLLoaderFactory* factory,
                    mojo::Remote<network::mojom::URLLoader>& loader,
                    network::TestURLLoaderClient& client,
                    const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.request_initiator = initiator_origin_;
    factory->CreateLoaderAndStart(
        loader.BindNewPipeAndPassReceiver(), next_request_id_++,
        network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }

  // Returns a URL which server-redirects to `target`.
  GURL RedirectingUrlTo(const GURL& target) {
    return embedded_test_server()->GetURL("/server-redirect?" + target.spec());
  }

  bool DidReachNetwork(const std::string& relative_url) {
    base::AutoLock lock(lock_);
    return observed_request_headers_.contains(relative_url);
  }

  std::optional<std::string> GetObservedHeader(const std::string& relative_url,
                                               const std::string& header_name) {
    base::AutoLock lock(lock_);
    auto req_it = observed_request_headers_.find(relative_url);
    if (req_it == observed_request_headers_.end()) {
      return std::nullopt;
    }
    auto header_it = req_it->second.find(header_name);
    if (header_it == req_it->second.end()) {
      return std::nullopt;
    }
    return header_it->second;
  }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    base::AutoLock lock(lock_);
    observed_request_headers_[request.relative_url] = request.headers;
  }

  url::Origin initiator_origin_;
  int32_t next_request_id_ = 1;
  base::Lock lock_;
  std::map<std::string, net::test_server::HttpRequest::HeaderMap>
      observed_request_headers_ GUARDED_BY(lock_);
};

// Verifies that calls to FollowRedirect() that are unexpected (either in their
// arguments or ordering) are rejected.
IN_PROC_BROWSER_TEST_F(WebRequestProxyingURLLoaderFactoryFollowRedirectTest,
                       BadFollowRedirectCallsAreRejected) {
  mojo::Remote<network::mojom::URLLoaderFactory> factory = CreateFrameFactory();
  const GURL echo_url = embedded_test_server()->GetURL("/echo");
  const GURL redirect_url = RedirectingUrlTo(echo_url);

  // 1. Calling FollowRedirect() when no redirect is pending.
  {
    mojo::test::BadMessageObserver bad_message_observer;
    network::TestURLLoaderClient client;
    mojo::Remote<network::mojom::URLLoader> loader;
    StartRequest(factory.get(), loader, client, echo_url);

    loader->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                           std::nullopt);

    EXPECT_EQ("Unexpected FollowRedirect",
              bad_message_observer.WaitForBadMessage());
    client.RunUntilComplete();
    EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  }

  // 2. Calling FollowRedirect() twice sequentially for a single redirect.
  // Verifies that `deferred_redirect_url_` is properly reset after the first
  // follow and cannot be followed again.
  {
    mojo::test::BadMessageObserver bad_message_observer;
    network::TestURLLoaderClient client;
    mojo::Remote<network::mojom::URLLoader> loader;
    StartRequest(factory.get(), loader, client, redirect_url);

    client.RunUntilRedirectReceived();
    ASSERT_TRUE(client.has_received_redirect());

    loader->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                           std::nullopt);
    loader->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                           std::nullopt);

    EXPECT_EQ("Unexpected FollowRedirect",
              bad_message_observer.WaitForBadMessage());
    client.RunUntilComplete();
    EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  }

  // 3. Calling FollowRedirect() with a cross-origin `new_url`.
  {
    mojo::test::BadMessageObserver bad_message_observer;
    network::TestURLLoaderClient client;
    mojo::Remote<network::mojom::URLLoader> loader;
    StartRequest(factory.get(), loader, client, redirect_url);

    client.RunUntilRedirectReceived();
    ASSERT_TRUE(client.has_received_redirect());

    loader->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                           GURL("https://example.com/other"));

    EXPECT_EQ("Unexpected new_url in FollowRedirect",
              bad_message_observer.WaitForBadMessage());
    client.RunUntilComplete();
    EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  }

  // 4. Calling FollowRedirect() with same-origin but credentialed `new_url`.
  {
    GURL::Replacements add_credentials;
    add_credentials.SetUsernameStr("user");
    add_credentials.SetPasswordStr("password");

    mojo::test::BadMessageObserver bad_message_observer;
    network::TestURLLoaderClient client;
    mojo::Remote<network::mojom::URLLoader> loader;
    StartRequest(factory.get(), loader, client, redirect_url);

    client.RunUntilRedirectReceived();
    ASSERT_TRUE(client.has_received_redirect());

    loader->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                           echo_url.ReplaceComponents(add_credentials));

    EXPECT_EQ("new_url with credentials in FollowRedirect",
              bad_message_observer.WaitForBadMessage());
    client.RunUntilComplete();
    EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);
  }
}

// Tests that using `new_url` to adjust the redirect to another URL in the same
// origin is allowed (e.g., appending a query parameter). This is the intended
// use of `new_url`.
IN_PROC_BROWSER_TEST_F(WebRequestProxyingURLLoaderFactoryFollowRedirectTest,
                       FollowRedirectWithSameOriginNewUrlIsAllowed) {
  mojo::Remote<network::mojom::URLLoaderFactory> factory = CreateFrameFactory();

  const GURL announced_url = embedded_test_server()->GetURL("/echo?announced");
  const GURL modified_url = embedded_test_server()->GetURL("/echo?modified");

  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  StartRequest(factory.get(), loader, client, RedirectingUrlTo(announced_url));

  client.RunUntilRedirectReceived();
  ASSERT_EQ(announced_url, client.redirect_info().new_url);
  client.ClearHasReceivedRedirect();

  loader->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                         modified_url);

  // Supplying `new_url` does not silently rewrite the destination: the network
  // stack synthesizes an internal redirect to it, which the proxy forwards on
  // as a normal redirect (giving extensions a chance to see the new URL). The
  // client is expected to follow that one too.
  client.RunUntilRedirectReceived();
  ASSERT_EQ(modified_url, client.redirect_info().new_url);
  client.ClearHasReceivedRedirect();

  loader->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                         std::nullopt);

  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);

  // The modified destination is fetched.
  EXPECT_TRUE(DidReachNetwork("/echo?modified"));
  EXPECT_FALSE(DidReachNetwork("/echo?announced"));
}

// Tests that an unexpected FollowRedirect() cannot route a request to a URL
// that a blocking onBeforeRequest listener would have cancelled.
// (Regression test for the specific scenario from crbug.com/497494634.)
IN_PROC_BROWSER_TEST_F(WebRequestProxyingURLLoaderFactoryFollowRedirectTest,
                       UnexpectedFollowRedirectCannotBypassBlockingListener) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      R"({
        "name": "Policy Filter Extension",
        "version": "1.0",
        "manifest_version": 3,
        "key": "%s",
        "permissions": ["webRequest", "webRequestBlocking"],
        "host_permissions": ["<all_urls>"],
        "background": {"service_worker": "background.js"}
      })",
      kAllowlistedExtensionKey));

  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.webRequest.onBeforeRequest.addListener(
      function(details) {
        return {
          cancel: details.url.indexOf("sample_blocked_destination") !== -1
        };
      },
      {urls: ["<all_urls>"]},
      ["blocking"]
    );
    chrome.test.sendMessage("ready");
  )");

  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_EQ(kAllowlistedExtensionId, extension->id());
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ForceProxy();

  const GURL allowed_url =
      embedded_test_server()->GetURL("/echo?sample_allowed");
  const GURL blocked_url =
      embedded_test_server()->GetURL("/echo?sample_blocked_destination");

  // Control: requesting the blocked URL directly is cancelled by the listener.
  // Without this, the assertions below could pass simply because the extension
  // was never actually filtering anything.
  {
    mojo::Remote<network::mojom::URLLoaderFactory> factory =
        CreateFrameFactory();
    network::TestURLLoaderClient client;
    mojo::Remote<network::mojom::URLLoader> loader;
    StartRequest(factory.get(), loader, client, blocked_url);

    client.RunUntilComplete();
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT,
              client.completion_status().error_code);
    EXPECT_FALSE(DidReachNetwork("/echo?sample_blocked_destination"));
  }

  // Attack: start an allowed request and immediately send an unsolicited
  // FollowRedirect() naming the blocked URL. Sending it without waiting would
  // reproduce the original bug: the browser processes FollowRedirect() while
  // the onBeforeRequest dispatch for `allowed_url` is still outstanding in the
  // extension's service worker (net::ERR_IO_PENDING), which is the window in
  // which the URL swap used to go unnoticed.
  {
    mojo::Remote<network::mojom::URLLoaderFactory> factory =
        CreateFrameFactory();
    mojo::test::BadMessageObserver bad_message_observer;
    network::TestURLLoaderClient client;
    mojo::Remote<network::mojom::URLLoader> loader;
    StartRequest(factory.get(), loader, client, allowed_url);

    loader->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                           blocked_url);

    EXPECT_EQ("Unexpected FollowRedirect",
              bad_message_observer.WaitForBadMessage());
    client.RunUntilComplete();
    EXPECT_EQ(net::ERR_INVALID_ARGUMENT, client.completion_status().error_code);

    // The blocked URL must never have been fetched.
    EXPECT_FALSE(DidReachNetwork("/echo?sample_blocked_destination"));
  }
}

class WebRequestProxyingURLLoaderFactorySubresourceTaskQueueTest
    : public WebRequestProxyingURLLoaderFactoryFollowRedirectTest {
 public:
  WebRequestProxyingURLLoaderFactorySubresourceTaskQueueTest() {
    feature_list_.InitAndEnableFeature(
        features::kSubresourceLoaderProxyTaskQueue);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that when `kSubresourceLoaderProxyTaskQueue` is enabled, a main
// frame's proxied subresource URLLoaderFactory binds secondary receivers
// (`Clone()`d factory receivers and `TrustedHeaderClient` receivers created in
// `OnLoaderCreated()` for `extraHeaders` listeners) to the high-priority
// `kSubresourceLoaderProxy` task queue rather than the default UI task queue.
IN_PROC_BROWSER_TEST_F(
    WebRequestProxyingURLLoaderFactorySubresourceTaskQueueTest,
    ClonedFactoryAndHeaderClientUseSubresourceLoaderProxyQueue) {
  TestExtensionDir test_dir;
  test_dir.WriteManifest(base::StringPrintf(
      R"({
        "name": "ExtraHeaders Extension",
        "version": "1.0",
        "manifest_version": 3,
        "key": "%s",
        "permissions": ["webRequest", "webRequestBlocking"],
        "host_permissions": ["<all_urls>"],
        "background": {"service_worker": "background.js"}
      })",
      kAllowlistedExtensionKey));

  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
    chrome.webRequest.onBeforeSendHeaders.addListener(
      function(details) {
        details.requestHeaders.push({name: "X-Added-By-Extension", value: "1"});
        return {requestHeaders: details.requestHeaders};
      },
      {urls: ["<all_urls>"]},
      ["blocking", "requestHeaders", "extraHeaders"]
    );
    chrome.test.sendMessage("ready");
  )");

  ExtensionTestMessageListener ready_listener("ready");
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());
  ForceProxy();

  auto proxy_task_runner = content::GetUIThreadTaskRunner(
      {content::BrowserTaskType::kSubresourceLoaderProxy});
  auto default_task_runner =
      content::GetUIThreadTaskRunner({content::BrowserTaskType::kDefault});

  mojo::Remote<network::mojom::URLLoaderFactory> factory = CreateFrameFactory();

  // 1. Verify that `Clone()` binds the cloned receiver to
  // `kSubresourceLoaderProxy` (`kHighPriority`) rather than `kDefault`.
  // After queueing `factory->Clone()` (on `kSubresourceLoaderProxy`), a nested
  // `cloned_factory->Clone()` + `FlushAsyncForTesting()` will only be bound and
  // dispatched when `WebRequestProxyingURLLoaderFactory::Clone()` runs. Because
  // `Clone()` binds the receiver to `kSubresourceLoaderProxy`, the nested clone
  // and flush complete ahead of a task already queued on `kDefault`.
  mojo::Remote<network::mojom::URLLoaderFactory> cloned_factory;
  mojo::Remote<network::mojom::URLLoaderFactory> second_cloned_factory;
  factory->Clone(cloned_factory.BindNewPipeAndPassReceiver(proxy_task_runner));
  cloned_factory->Clone(
      second_cloned_factory.BindNewPipeAndPassReceiver(proxy_task_runner));

  base::RunLoop flush_loop;
  std::vector<std::string> execution_order;
  second_cloned_factory.FlushAsyncForTesting(base::BindLambdaForTesting([&]() {
    execution_order.push_back("cloned_factory_flushed");
    if (execution_order.size() == 2u) {
      flush_loop.Quit();
    }
  }));
  default_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&]() {
        execution_order.push_back("default_queue_task");
        if (execution_order.size() == 2u) {
          flush_loop.Quit();
        }
      }));
  flush_loop.Run();

  ASSERT_EQ(2u, execution_order.size());
  EXPECT_EQ("cloned_factory_flushed", execution_order[0]);
  EXPECT_EQ("default_queue_task", execution_order[1]);

  // 2. Verify that `OnLoaderCreated()` binds `header_client_receiver_` to
  // `kSubresourceLoaderProxy` (`kHighPriority`) rather than `kDefault`.
  // Drive `MaybeProxyURLLoaderFactory` with `proxy_task_runner` so the test
  // holds the `TrustedURLLoaderHeaderClient` remote directly. If
  // `header_client_receiver_` omitted the task runner in `Bind()`, Mojo would
  // fall back to `SequencedTaskRunner::GetCurrentDefault()` (`kDefault`) even
  // though `OnLoaderCreated()` ran on `kSubresourceLoaderProxy`, queueing the
  // `FlushAsyncForTesting()` message behind `default_queue_task`.
  {
    auto* web_request_api =
        BrowserContextKeyedAPIFactory<WebRequestAPI>::Get(profile());
    ASSERT_TRUE(web_request_api);

    network::URLLoaderFactoryBuilder factory_builder;
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_pending_remote;
    content::RenderFrameHost* main_frame =
        GetActiveWebContents()->GetPrimaryMainFrame();
    const url::Origin main_frame_origin = main_frame->GetLastCommittedOrigin();
    ASSERT_TRUE(web_request_api->MaybeProxyURLLoaderFactory(
        profile(), main_frame, main_frame->GetProcess()->GetDeprecatedID(),
        content::ContentBrowserClient::URLLoaderFactoryType::
            kDocumentSubResource,
        /*navigation_id=*/std::nullopt,
        ukm::SourceIdObj::FromInt64(main_frame->GetPageUkmSourceId()),
        factory_builder, &header_client_pending_remote, proxy_task_runner,
        main_frame_origin));
    ASSERT_TRUE(header_client_pending_remote.is_valid());

    network::TestURLLoaderFactory terminal_factory;
    mojo::PendingRemote<network::mojom::URLLoaderFactory> terminal_remote;
    terminal_factory.Clone(terminal_remote.InitWithNewPipeAndPassReceiver());
    mojo::Remote<network::mojom::URLLoaderFactory> proxied_factory;
    std::move(factory_builder)
        .Finish(proxied_factory.BindNewPipeAndPassReceiver(proxy_task_runner),
                std::move(terminal_remote));
    mojo::Remote<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_factory(std::move(header_client_pending_remote),
                              proxy_task_runner);

    network::ResourceRequest request;
    request.url = embedded_test_server()->GetURL("/echo?header_queue_check");
    request.request_initiator = main_frame_origin;
    mojo::Remote<network::mojom::URLLoader> test_loader;
    network::TestURLLoaderClient test_client;
    proxied_factory->CreateLoaderAndStart(
        test_loader.BindNewPipeAndPassReceiver(proxy_task_runner),
        /*request_id=*/777, network::mojom::kURLLoadOptionNone, request,
        test_client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return terminal_factory.NumPending() == 1; }));

    const int32_t network_request_id =
        terminal_factory.GetPendingRequest(0)->request_id;
    mojo::Remote<network::mojom::TrustedHeaderClient> header_client;
    header_client_factory->OnLoaderCreated(
        network_request_id,
        header_client.BindNewPipeAndPassReceiver(proxy_task_runner));

    base::RunLoop header_flush_loop;
    std::vector<std::string> header_execution_order;
    header_client.FlushAsyncForTesting(base::BindLambdaForTesting([&]() {
      header_execution_order.push_back("header_client_flushed");
      if (header_execution_order.size() == 2u) {
        header_flush_loop.Quit();
      }
    }));
    default_task_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&]() {
          header_execution_order.push_back("default_queue_task");
          if (header_execution_order.size() == 2u) {
            header_flush_loop.Quit();
          }
        }));
    header_flush_loop.Run();

    EXPECT_TRUE(header_client.is_connected());
    ASSERT_EQ(2u, header_execution_order.size());
    EXPECT_EQ("header_client_flushed", header_execution_order[0]);
    EXPECT_EQ("default_queue_task", header_execution_order[1]);
  }

  // 3. End-to-end verification that a real subresource request through the
  // cloned factory with an `extraHeaders` listener succeeds and applies the
  // extension's header modification.
  network::TestURLLoaderClient client;
  mojo::Remote<network::mojom::URLLoader> loader;
  StartRequest(cloned_factory.get(), loader, client,
               embedded_test_server()->GetURL("/echo"));
  client.RunUntilComplete();
  EXPECT_EQ(net::OK, client.completion_status().error_code);
  EXPECT_EQ("1", GetObservedHeader("/echo", "X-Added-By-Extension"));
}

}  // namespace extensions
