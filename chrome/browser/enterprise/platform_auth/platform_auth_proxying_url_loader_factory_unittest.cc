// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_proxying_url_loader_factory.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "components/download/public/common/download_url_parameters.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/mock_url_loader_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_auth {

class PlatformAuthProxyingURLLoaderFactoryTest : public testing::Test {
 protected:
  PlatformAuthProxyingURLLoaderFactoryTest()
      : test_factory_receiver_(&test_factory_) {
    test_request_.url = GURL("https://foobar.example.com");
    test_request_.method = "GET";
  }

  void CreateProxyingFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      base::OnceCallback<void()> dtor_callback,
      base::OnceCallback<void(const network::ResourceRequest&)>
          request_intercepted_callback) {
    auto* res = new ProxyingURLLoaderFactory(std::move(receiver),
                                             std::move(target_factory));
    res->SetDestructionCallbackForTesting(std::move(dtor_callback));
    res->SetRequestInterceptedCallbackForTesting(
        std::move(request_intercepted_callback));
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> SetupFactoryChain(
      base::OnceCallback<void()> dtor_callback,
      base::OnceCallback<void(const network::ResourceRequest&)>
          request_intercepted_callback = {}) {
    // 1. Create the input pipe (Client -> Proxy).
    mojo::PendingRemote<network::mojom::URLLoaderFactory> proxy_remote;
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> proxy_receiver =
        proxy_remote.InitWithNewPipeAndPassReceiver();

    // 2. Create the output pipe (Proxy -> Target).
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
    auto target_receiver =
        target_factory_remote.InitWithNewPipeAndPassReceiver();

    // 3. Bind the target to our test backend.
    test_factory_receiver_.Bind(std::move(target_receiver));

    CreateProxyingFactory(
        std::move(proxy_receiver), std::move(target_factory_remote),
        std::move(dtor_callback), std::move(request_intercepted_callback));

    return proxy_remote;
  }

  // Checks whether requests sent to given client are properly propagated to the
  // target factory.
  void CheckClient(mojo::Remote<network::mojom::URLLoaderFactory>& client) {
    CHECK(client.is_bound());
    base::RunLoop wait_for_request_loop;
    test_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          wait_for_request_loop.Quit();
        }));
    mojo::PendingRemote<network::mojom::URLLoader> url_loader_pending_remote;

    network::MockURLLoaderClient mock_client;
    mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(
        &mock_client);

    client->CreateLoaderAndStart(
        url_loader_pending_remote.InitWithNewPipeAndPassReceiver(), 0,
        network::mojom::kURLLoadOptionNone, test_request_,
        client_receiver.BindNewPipeAndPassRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    wait_for_request_loop.Run();
  }

  bool FactoryHasInterceptors(
      scoped_refptr<network::SharedURLLoaderFactory> resulting_factory,
      scoped_refptr<network::SharedURLLoaderFactory> terminal_factory) {
    // If no interceptors were added then the factory builder does not wrap the
    // terminal factory and simply returns it as the final product.
    return resulting_factory.get() != terminal_factory.get();
  }

  mojo::Receiver<network::mojom::URLLoaderFactory> test_factory_receiver_;
  network::TestURLLoaderFactory test_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  network::ResourceRequest test_request_;
};

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest,
       DestroysItselfAfterTargetFactoryDisconnects) {
  base::test::TestFuture<void> proxy_destroyed_future;
  mojo::Remote<network::mojom::URLLoaderFactory> client(
      SetupFactoryChain(base::BindOnce(proxy_destroyed_future.GetCallback())));

  // Check that the factory works.
  CheckClient(client);

  // Trigger destruction by breaking the target connection.
  test_factory_receiver_.reset();

  // Spin the event loop until destructor in the proxying url loader factory is
  // ran.
  EXPECT_TRUE(proxy_destroyed_future.Wait());
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest,
       DestroysItselfAfterAllClientsDisconnect) {
  base::test::TestFuture<void> proxy_destroyed_future;

  // Setup the chain with two clients.
  mojo::Remote<network::mojom::URLLoaderFactory> first_client(
      SetupFactoryChain(base::BindOnce(proxy_destroyed_future.GetCallback())));

  mojo::PendingRemote<network::mojom::URLLoaderFactory> another_pending_remote;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> clone_receiver =
      another_pending_remote.InitWithNewPipeAndPassReceiver();

  first_client->Clone(std::move(clone_receiver));
  mojo::Remote<network::mojom::URLLoaderFactory> second_client(
      std::move(another_pending_remote));

  // Let everything setup.
  base::test::RunUntil(
      [&]() { return first_client.is_bound() && second_client.is_bound(); });

  // Make sure that the chain works.
  CheckClient(first_client);
  CheckClient(second_client);

  // Disconnect the first client.
  first_client.reset();
  base::test::RunUntil([&]() { return !first_client.is_bound(); });

  // Make sure that the second client still works.
  CheckClient(second_client);

  // The proxy shouldn't destroy yet, there is still a second client connected.
  EXPECT_FALSE(proxy_destroyed_future.IsReady());

  // Disconnect the second client and wait for proxy's destruction.
  second_client.reset();
  EXPECT_TRUE(proxy_destroyed_future.Wait());
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_ProxiesAppropriateRequests) {
  PlatformAuthProviderManager::GetInstance().SetEnabled(true, {});

  network::TestURLLoaderFactory terminal_factory;
  scoped_refptr<network::SharedURLLoaderFactory> terminal_shared_factory =
      terminal_factory.GetSafeWeakWrapper();

  network::URLLoaderFactoryBuilder factory_builder;

  ProxyingURLLoaderFactory::MaybeProxyRequest(
      url::Origin::Create(GURL("https://foobar.example.com/")),
      ChromeContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      factory_builder);

  scoped_refptr<network::SharedURLLoaderFactory> resulting_factory =
      std::move(factory_builder).Finish(terminal_shared_factory);

  EXPECT_TRUE(
      FactoryHasInterceptors(resulting_factory, terminal_shared_factory));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest, MaybeProxyRequest_NoHTTPS) {
  PlatformAuthProviderManager::GetInstance().SetEnabled(true, {});

  network::TestURLLoaderFactory terminal_factory;
  scoped_refptr<network::SharedURLLoaderFactory> terminal_shared_factory =
      terminal_factory.GetSafeWeakWrapper();

  network::URLLoaderFactoryBuilder factory_builder;

  ProxyingURLLoaderFactory::MaybeProxyRequest(
      url::Origin::Create(GURL("file://foobar.example.com/")),
      ChromeContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      factory_builder);

  scoped_refptr<network::SharedURLLoaderFactory> resulting_factory =
      std::move(factory_builder).Finish(terminal_shared_factory);

  EXPECT_FALSE(
      FactoryHasInterceptors(resulting_factory, terminal_shared_factory));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_NoSubResource) {
  PlatformAuthProviderManager::GetInstance().SetEnabled(true, {});

  network::TestURLLoaderFactory terminal_factory;
  scoped_refptr<network::SharedURLLoaderFactory> terminal_shared_factory =
      terminal_factory.GetSafeWeakWrapper();

  network::URLLoaderFactoryBuilder factory_builder;

  ProxyingURLLoaderFactory::MaybeProxyRequest(
      url::Origin::Create(GURL("https://foobar.example.com/")),
      ChromeContentBrowserClient::URLLoaderFactoryType::kNavigation,
      factory_builder);

  scoped_refptr<network::SharedURLLoaderFactory> resulting_factory =
      std::move(factory_builder).Finish(terminal_shared_factory);

  EXPECT_FALSE(
      FactoryHasInterceptors(resulting_factory, terminal_shared_factory));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_PlatformAuthManagerDisabled) {
  PlatformAuthProviderManager::GetInstance().SetEnabled(false, {});

  network::TestURLLoaderFactory terminal_factory;
  scoped_refptr<network::SharedURLLoaderFactory> terminal_shared_factory =
      terminal_factory.GetSafeWeakWrapper();

  network::URLLoaderFactoryBuilder factory_builder;

  ProxyingURLLoaderFactory::MaybeProxyRequest(
      url::Origin::Create(GURL("https://foobar.example.com/")),
      ChromeContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      factory_builder);

  scoped_refptr<network::SharedURLLoaderFactory> resulting_factory =
      std::move(factory_builder).Finish(terminal_shared_factory);

  EXPECT_FALSE(
      FactoryHasInterceptors(resulting_factory, terminal_shared_factory));
}

struct InterceptTestParams {
  const std::string_view url;
  bool should_intercept;
  std::string method = "POST";
};

std::ostream& operator<<(std::ostream& os, const InterceptTestParams& param) {
  os << "{\nurl: " << param.url << "\n";
  os << "should_intercept: " << (param.should_intercept ? "true" : "false")
     << "\n}\n";
  return os;
}

class PlatformAuthProxyingURLLoaderFactoryInterceptTest
    : public PlatformAuthProxyingURLLoaderFactoryTest,
      public testing::WithParamInterface<InterceptTestParams> {};

TEST_P(PlatformAuthProxyingURLLoaderFactoryInterceptTest,
       ShouldInterceptMatchingURLs) {
  const InterceptTestParams& params = GetParam();
  base::test::TestFuture<void> request_intercepted_future;
  const GURL url = GURL(params.url);

  base::OnceCallback<void(const network::ResourceRequest& request)>
      request_itercepted_callback = base::BindOnce(
          [](const GURL url, base::OnceCallback<void()> future_callback,
             const network::ResourceRequest& request) {
            EXPECT_EQ(url, request.url);
            std::move(future_callback).Run();
          },
          url, request_intercepted_future.GetCallback());

  mojo::Remote<network::mojom::URLLoaderFactory> client(
      SetupFactoryChain({}, std::move(request_itercepted_callback)));
  mojo::PendingRemote<network::mojom::URLLoader> url_loader_pending_remote;
  network::MockURLLoaderClient mock_client;
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(&mock_client);
  network::ResourceRequest request;
  request.url = url;
  request.method = params.method;

  client->CreateLoaderAndStart(
      url_loader_pending_remote.InitWithNewPipeAndPassReceiver(), 0,
      network::mojom::kURLLoadOptionNone, request,
      client_receiver.BindNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  if (params.should_intercept) {
    // The request was indeed intercepted.
    EXPECT_TRUE(request_intercepted_future.Wait());
    // The request was not received by the network bound factory.
    EXPECT_EQ(test_factory_.NumPending(), 0);
  } else {
    // The request was not intercepted.
    EXPECT_FALSE(request_intercepted_future.IsReady());
    // The request has been received by the network bound factory.
    EXPECT_EQ(test_factory_.NumPending(), 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PlatformAuthProxyingURLLoaderFactoryInterceptTest,
    testing::Values(
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify",
             true}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify/",
             true}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/abc/verify",
             true}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/abc/verify/",
             true}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/user-id_123.test/verify",
             true}),
        InterceptTestParams(
            {"https://foobar.example.com:443/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify",
             true}),
        InterceptTestParams(
            {"https://foobar.example.com:443/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify",
             true}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify",
             false, "GET"}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify/",
             false, "GET"}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/abc/verify",
             false, "GET"}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/abc/verify/",
             false, "GET"}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/user-id_123.test/verify",
             false, "GET"}),
        InterceptTestParams(
            {"https://foobar.example.com:443/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify",
             false, "GET"}),
        InterceptTestParams(
            {"https://foobar.example.com:443/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify",
             false, "GET"}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify?foo=bar&x=1",
             false}),
        InterceptTestParams({"http://foobar.example.com/idp/idx/authenticators/"
                             "sso_extension/transactions/123/verify",
                             false}),
        InterceptTestParams({"http://foobar.example.com/idp/idx/authenticators/"
                             "sso_extension/transactions/verify",
                             false}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/a/b/verify",
             false}),
        InterceptTestParams(
            {"chrome://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/a/b/verify",
             false}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions//verify",
             false}),
        InterceptTestParams({"https://foobar.example.com/idp/idx/authenticator/"
                             "sso_extension/transactions/123/verify",
                             false}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verified",
             false}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify/extra",
             false}),
        InterceptTestParams(
            {"https://foobar.example.com/IDP/IDX/AUTHENTICATORS/"
             "SSO_EXTENSION/TRANSACTIONS/123/VERIFY",
             false}),
        InterceptTestParams({"wss://foobar.example.com/idp/idx/authenticators/"
                             "sso_extension/transactions/123/verify",
                             false}),
        InterceptTestParams({"file:///idp/idx/authenticators/"
                             "sso_extension/transactions/123/verify",
                             false}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify#section",
             false}),
        InterceptTestParams(
            {"https://this/is/a/test/"
             "request?param=value&another_param=another_value#some-section",
             false}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify",
             false, "NOT_A_METHOD"}),
        InterceptTestParams(
            {"https://foobar.example.com/idp/idx/authenticators/"
             "sso_extension/transactions/123/verify",
             false, ""}),
        InterceptTestParams({"https://foobar.example.com", false})));

}  // namespace enterprise_auth
