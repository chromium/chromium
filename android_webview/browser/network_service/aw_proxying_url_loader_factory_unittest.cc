// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxying_url_loader_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_origin_matched_header.h"
#include "android_webview/browser/network_service/aw_browser_context_io_thread_handle.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace android_webview {

class AwProxyingURLLoaderFactoryTest : public testing::Test {
 public:
  AwProxyingURLLoaderFactoryTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_url_loader_factory_(true) {}

 protected:
  int FollowRedirect(const GURL& original_url, const GURL& redirect_url) {
    mojo::Remote<network::mojom::URLLoaderFactory> proxy_factory_remote;
    mojo::Receiver<network::mojom::URLLoaderFactory> target_factory_receiver(
        &test_url_loader_factory_);

    auto factory = std::make_unique<AwProxyingURLLoaderFactory>(
        /*cookie_manager=*/std::nullopt, &cookie_access_policy_,
        /*isolation_info=*/std::nullopt,
        /*key=*/std::nullopt, content::FrameTreeNodeId(),
        proxy_factory_remote.BindNewPipeAndPassReceiver(),
        target_factory_receiver.BindNewPipeAndPassRemote(),
        /*intercept_only=*/false,
        /*security_options=*/std::nullopt,
        /*origin_matched_headers=*/
        std::vector<scoped_refptr<AwOriginMatchedHeader>>(),
        /*browser_context_handle=*/nullptr,
        /*navigation_id=*/std::nullopt);

    network::ResourceRequest request;
    request.url = original_url;
    request.method = "GET";

    mojo::Remote<network::mojom::URLLoader> loader_remote;
    network::TestURLLoaderClient client;

    proxy_factory_remote->CreateLoaderAndStart(
        loader_remote.BindNewPipeAndPassReceiver(),
        /*request_id=*/1,
        /*options=*/0, request, client.CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    if (test_url_loader_factory_.NumPending() != 1) {
      ADD_FAILURE() << "Expected 1 pending request";
      return net::ERR_FAILED;
    }
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_url_loader_factory_.GetPendingRequest(0);
    if (!pending_request) {
      ADD_FAILURE() << "Pending request is null";
      return net::ERR_FAILED;
    }

    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_url = redirect_url;
    redirect_info.new_method = "GET";

    network::mojom::URLResponseHeadPtr response_head =
        network::mojom::URLResponseHead::New();

    pending_request->client->OnReceiveRedirect(redirect_info,
                                               std::move(response_head));

    client.RunUntilRedirectReceived();
    EXPECT_TRUE(client.has_received_redirect());
    EXPECT_EQ(redirect_url, client.redirect_info().new_url);

    network::HttpRequestHeadersUpdateParams update_params;
    loader_remote->FollowRedirect(std::move(update_params), redirect_url);

    bool condition_met = base::test::RunUntil([&]() {
      return client.has_received_completion() ||
             (pending_request->test_url_loader &&
              !pending_request->test_url_loader->follow_redirect_params()
                   .empty());
    });
    EXPECT_TRUE(condition_met);

    if (pending_request->test_url_loader &&
        !pending_request->test_url_loader->follow_redirect_params().empty()) {
      pending_request->client->OnReceiveResponse(
          network::mojom::URLResponseHead::New(),
          mojo::ScopedDataPipeConsumerHandle(), std::nullopt);
      pending_request->client->OnComplete(
          network::URLLoaderCompletionStatus(net::OK));
    }

    client.RunUntilComplete();
    return client.completion_status().error_code;
  }

  content::BrowserTaskEnvironment task_environment_;
  AwCookieAccessPolicy cookie_access_policy_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(AwProxyingURLLoaderFactoryTest, BlocksUnsafeRedirectToContentUrl) {
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT,
            FollowRedirect(GURL("http://example.com"),
                           GURL("content://com.example.provider/file")));
}

TEST_F(AwProxyingURLLoaderFactoryTest, BlocksUnsafeRedirectToFile) {
  EXPECT_EQ(net::ERR_UNSAFE_REDIRECT,
            FollowRedirect(GURL("http://example.com"),
                           GURL("file:///android_asset/test.html")));
}

TEST_F(AwProxyingURLLoaderFactoryTest, AllowsSafeRedirectToFile) {
  EXPECT_EQ(net::OK,
            FollowRedirect(GURL("file:///foo/bar"), GURL("file:///foo/baz")));
}

TEST_F(AwProxyingURLLoaderFactoryTest, AllowsSafeRedirect) {
  EXPECT_EQ(net::OK, FollowRedirect(GURL("http://example.com"),
                                    GURL("http://otherexample.com")));
}

}  // namespace android_webview
