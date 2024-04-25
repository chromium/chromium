// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_proxying_url_loader_factory.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/header_modification_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::_;

namespace signin {

namespace {

class MockDelegate : public HeaderModificationDelegate {
 public:
  MockDelegate() = default;

  MockDelegate(const MockDelegate&) = delete;
  MockDelegate& operator=(const MockDelegate&) = delete;

  ~MockDelegate() override = default;

  MOCK_METHOD1(ShouldInterceptNavigation, bool(content::WebContents* contents));
  MOCK_METHOD2(ProcessRequest,
               void(ChromeRequestAdapter* request_adapter,
                    const GURL& redirect_url));
  MOCK_METHOD2(ProcessResponse,
               void(ResponseAdapter* response_adapter,
                    const GURL& redirect_url));

  base::WeakPtr<MockDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDelegate> weak_factory_{this};
};

content::WebContents::Getter NullWebContentsGetter() {
  return base::BindRepeating([]() -> content::WebContents* { return nullptr; });
}

}  // namespace

class ChromeSigninProxyingURLLoaderFactoryTest : public testing::Test {
 public:
  ChromeSigninProxyingURLLoaderFactoryTest()
      : test_factory_receiver_(&test_factory_) {}

  ChromeSigninProxyingURLLoaderFactoryTest(
      const ChromeSigninProxyingURLLoaderFactoryTest&) = delete;
  ChromeSigninProxyingURLLoaderFactoryTest& operator=(
      const ChromeSigninProxyingURLLoaderFactoryTest&) = delete;

  ~ChromeSigninProxyingURLLoaderFactoryTest() override {}

  base::WeakPtr<MockDelegate> StartRequest(
      std::unique_ptr<network::ResourceRequest> request) {
    loader_ = network::SimpleURLLoader::Create(std::move(request),
                                               TRAFFIC_ANNOTATION_FOR_TESTS);

    auto delegate = std::make_unique<MockDelegate>();
    base::WeakPtr<MockDelegate> delegate_weak = delegate->GetWeakPtr();

    network::URLLoaderFactoryBuilder factory_builder;
    proxying_factory_ = std::make_unique<ProxyingURLLoaderFactory>(
        std::move(delegate), net::IsolationInfo(), NullWebContentsGetter(),
        factory_builder,
        base::BindOnce(&ChromeSigninProxyingURLLoaderFactoryTest::OnDisconnect,
                       base::Unretained(this)));

    mojo::Remote<network::mojom::URLLoaderFactory> factory_remote(
        std::move(factory_builder)
            .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
                test_factory_receiver_.BindNewPipeAndPassRemote()));

    loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        factory_remote.get(),
        base::BindOnce(
            &ChromeSigninProxyingURLLoaderFactoryTest::OnDownloadComplete,
            base::Unretained(this)));

    return delegate_weak;
  }

  void CloseFactoryReceiver() { test_factory_receiver_.reset(); }

  network::TestURLLoaderFactory* factory() { return &test_factory_; }
  network::SimpleURLLoader* loader() { return loader_.get(); }
  std::string* response_body() { return response_body_.get(); }

  void OnDownloadComplete(std::unique_ptr<std::string> body) {
    response_body_ = std::move(body);
  }

 private:
  void OnDisconnect(ProxyingURLLoaderFactory* factory) {
    EXPECT_EQ(factory, proxying_factory_.get());
    proxying_factory_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::unique_ptr<ProxyingURLLoaderFactory> proxying_factory_;
  network::TestURLLoaderFactory test_factory_;
  mojo::Receiver<network::mojom::URLLoaderFactory> test_factory_receiver_;
  std::unique_ptr<std::string> response_body_;
};

TEST_F(ChromeSigninProxyingURLLoaderFactoryTest, NoModification) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://google.com/");

  factory()->AddResponse("https://google.com/", "Hello.");
  base::WeakPtr<MockDelegate> delegate = StartRequest(std::move(request));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::OK, loader()->NetError());
  ASSERT_TRUE(response_body());
  EXPECT_EQ("Hello.", *response_body());
}

TEST_F(ChromeSigninProxyingURLLoaderFactoryTest, ModifyHeaders) {
  const GURL kTestURL("https://google.com/index.html");
  const GURL kTestReferrer("https://chrome.com/referrer.html");
  const GURL kTestRedirectURL("https://youtube.com/index.html");

  // Set up the request.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = kTestURL;
  request->referrer = kTestReferrer;
  request->destination = network::mojom::RequestDestination::kDocument;
  request->is_outermost_main_frame = true;
  request->headers.SetHeader("X-Request-1", "Foo");

  base::WeakPtr<MockDelegate> delegate = StartRequest(std::move(request));

  // The first destruction callback added by ProcessRequest is expected to be
  // called. The second (added after a redirect) will not be.
  base::MockCallback<base::OnceClosure> destruction_callback;
  EXPECT_CALL(destruction_callback, Run()).Times(1);
  base::MockCallback<base::OnceClosure> ignored_destruction_callback;
  EXPECT_CALL(ignored_destruction_callback, Run()).Times(0);

  // The delegate will be called twice to process a request, first when the
  // request is started and again when the request is redirected.
  EXPECT_CALL(*delegate, ProcessRequest(_, _))
      .WillOnce(
          Invoke([&](ChromeRequestAdapter* adapter, const GURL& redirect_url) {
            EXPECT_EQ(kTestURL, adapter->GetUrl());
            EXPECT_EQ(network::mojom::RequestDestination::kDocument,
                      adapter->GetRequestDestination());
            EXPECT_TRUE(adapter->IsOutermostMainFrame());
            EXPECT_EQ(kTestReferrer, adapter->GetReferrer());

            EXPECT_TRUE(adapter->HasHeader("X-Request-1"));
            adapter->RemoveRequestHeaderByName("X-Request-1");
            EXPECT_FALSE(adapter->HasHeader("X-Request-1"));

            adapter->SetExtraHeaderByName("X-Request-2", "Bar");
            EXPECT_TRUE(adapter->HasHeader("X-Request-2"));

            EXPECT_EQ(GURL(), redirect_url);

            adapter->SetDestructionCallback(destruction_callback.Get());
          }))
      .WillOnce(
          Invoke([&](ChromeRequestAdapter* adapter, const GURL& redirect_url) {
            EXPECT_EQ(network::mojom::RequestDestination::kDocument,
                      adapter->GetRequestDestination());
            EXPECT_TRUE(adapter->IsOutermostMainFrame());

            // Changes to the URL and referrer take effect after the redirect
            // is followed.
            EXPECT_EQ(kTestURL, adapter->GetUrl());
            EXPECT_EQ(kTestReferrer, adapter->GetReferrer());

            // X-Request-1 and X-Request-2 were modified in the previous call to
            // ProcessRequest(). These changes should still be present.
            EXPECT_FALSE(adapter->HasHeader("X-Request-1"));
            EXPECT_TRUE(adapter->HasHeader("X-Request-2"));

            adapter->RemoveRequestHeaderByName("X-Request-2");
            EXPECT_FALSE(adapter->HasHeader("X-Request-2"));

            adapter->SetExtraHeaderByName("X-Request-3", "Baz");
            EXPECT_TRUE(adapter->HasHeader("X-Request-3"));

            EXPECT_EQ(kTestRedirectURL, redirect_url);

            adapter->SetDestructionCallback(ignored_destruction_callback.Get());
          }));

  const void* const kResponseUserDataKey = &kResponseUserDataKey;
  std::unique_ptr<base::SupportsUserData::Data> response_user_data =
      std::make_unique<base::SupportsUserData::Data>();
  base::SupportsUserData::Data* response_user_data_ptr =
      response_user_data.get();

  // The delegate will also be called twice to process a response, first when
  // the redirect is received and again for the redirect response.
  EXPECT_CALL(*delegate, ProcessResponse(_, _))
      .WillOnce(Invoke([&](ResponseAdapter* adapter, const GURL& redirect_url) {
        EXPECT_EQ(kTestURL, adapter->GetUrl());
        EXPECT_TRUE(adapter->IsOutermostMainFrame());

        adapter->SetUserData(kResponseUserDataKey,
                             std::move(response_user_data));
        EXPECT_EQ(response_user_data_ptr,
                  adapter->GetUserData(kResponseUserDataKey));

        const net::HttpResponseHeaders* headers = adapter->GetHeaders();
        EXPECT_TRUE(headers->HasHeader("X-Response-1"));
        EXPECT_TRUE(headers->HasHeader("X-Response-2"));
        adapter->RemoveHeader("X-Response-2");

        EXPECT_EQ(kTestRedirectURL, redirect_url);
      }))
      .WillOnce(Invoke([&](ResponseAdapter* adapter, const GURL& redirect_url) {
        EXPECT_EQ(kTestRedirectURL, adapter->GetUrl());
        EXPECT_TRUE(adapter->IsOutermostMainFrame());

        EXPECT_EQ(response_user_data_ptr,
                  adapter->GetUserData(kResponseUserDataKey));

        const net::HttpResponseHeaders* headers = adapter->GetHeaders();
        // This is a new response and so previous headers should not carry over.
        EXPECT_FALSE(headers->HasHeader("X-Response-1"));
        EXPECT_FALSE(headers->HasHeader("X-Response-2"));

        EXPECT_TRUE(headers->HasHeader("X-Response-3"));
        EXPECT_TRUE(headers->HasHeader("X-Response-4"));
        adapter->RemoveHeader("X-Response-3");

        EXPECT_EQ(GURL(), redirect_url);
      }));

  // Set up a redirect and final response.
  {
    net::RedirectInfo redirect_info;
    redirect_info.new_url = kTestRedirectURL;
    // An HTTPS to HTTPS redirect such as this wouldn't normally change the
    // referrer but we do for testing purposes.
    redirect_info.new_referrer = kTestURL.spec();

    auto redirect_head = network::mojom::URLResponseHead::New();
    redirect_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    redirect_head->headers->SetHeader("X-Response-1", "Foo");
    redirect_head->headers->SetHeader("X-Response-2", "Bar");

    auto response_head = network::mojom::URLResponseHead::New();
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    response_head->headers->SetHeader("X-Response-3", "Foo");
    response_head->headers->SetHeader("X-Response-4", "Bar");
    std::string body("Hello.");
    network::URLLoaderCompletionStatus status;
    status.decoded_body_length = body.size();

    network::TestURLLoaderFactory::Redirects redirects;
    redirects.push_back({redirect_info, std::move(redirect_head)});

    factory()->AddResponse(kTestURL, std::move(response_head), body, status,
                           std::move(redirects));
  }

  // Wait for the request to complete and check the response.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(net::OK, loader()->NetError());
  const network::mojom::URLResponseHead* response_head =
      loader()->ResponseInfo();
  ASSERT_TRUE(response_head && response_head->headers);
  EXPECT_FALSE(response_head->headers->HasHeader("X-Response-3"));
  EXPECT_TRUE(response_head->headers->HasHeader("X-Response-4"));
  ASSERT_TRUE(response_body());
  EXPECT_EQ("Hello.", *response_body());

  // NOTE: TestURLLoaderFactory currently does not expose modifications to
  // request headers and so we cannot verify that the modifications have been
  // passed to the target URLLoader.
}

TEST_F(ChromeSigninProxyingURLLoaderFactoryTest, TargetFactoryFailure) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_target_factory_remote;
  auto target_factory_receiver =
      pending_target_factory_remote.InitWithNewPipeAndPassReceiver();

  // Without a target factory the proxy will process no requests.
  auto delegate = std::make_unique<MockDelegate>();
  EXPECT_CALL(*delegate, ProcessRequest(_, _)).Times(0);

  network::URLLoaderFactoryBuilder factory_builder;

  auto proxying_factory = std::make_unique<ProxyingURLLoaderFactory>(
      std::move(delegate), net::IsolationInfo(), NullWebContentsGetter(),
      factory_builder, base::DoNothing());

  mojo::Remote<network::mojom::URLLoaderFactory> factory_remote(
      std::move(factory_builder)
          .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
              std::move(pending_target_factory_remote)));

  // Close |target_factory_receiver| instead of binding it to a
  // URLLoaderFactory. Spin the message loop so that the connection error
  // handler can run.
  target_factory_receiver = mojo::NullReceiver();
  base::RunLoop().RunUntilIdle();

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://google.com");
  auto loader = network::SimpleURLLoader::Create(std::move(request),
                                                 TRAFFIC_ANNOTATION_FOR_TESTS);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      factory_remote.get(),
      base::BindOnce(
          &ChromeSigninProxyingURLLoaderFactoryTest::OnDownloadComplete,
          base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(response_body());
  EXPECT_EQ(net::ERR_FAILED, loader->NetError());
}

TEST_F(ChromeSigninProxyingURLLoaderFactoryTest, RequestKeepAlive) {
  // Start the request.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GURL("https://google.com");
  base::WeakPtr<MockDelegate> delegate = StartRequest(std::move(request));
  base::RunLoop().RunUntilIdle();

  // Close the factory receiver and spin the message loop again to allow the
  // connection error handler to be called.
  CloseFactoryReceiver();
  base::RunLoop().RunUntilIdle();

  // The ProxyingURLLoaderFactory should not have been destroyed yet because
  // there is still an in progress request that has not been completed.
  EXPECT_TRUE(delegate);

  // Complete the request.
  factory()->AddResponse("https://google.com", "Hello.");
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(delegate);
  EXPECT_EQ(net::OK, loader()->NetError());
  ASSERT_TRUE(response_body());
  EXPECT_EQ("Hello.", *response_body());
}

}  // namespace signin
