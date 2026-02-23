// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_proxying_url_loader_factory.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/enterprise/platform_auth/platform_auth_features.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
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
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/mock_url_loader_client.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_auth {

namespace {

constexpr char kRequestInitiator[] = "https://barfoo.example.com";

}

class PlatformAuthProxyingURLLoaderFactoryTest : public testing::Test {
 protected:
  PlatformAuthProxyingURLLoaderFactoryTest()
      : test_factory_receiver_(&test_factory_) {
    test_request_.request_initiator =
        url::Origin::Create(GURL(kRequestInitiator));
    test_request_.url = GURL("https://foobar.example.com");
    test_request_.method = "GET";
  }

  void SetUp() override {
    PlatformAuthProviderManager::GetInstance().SetEnabled(true, {});
  }

  void CreateProxyingFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      base::OnceCallback<void()> dtor_callback,
      base::OnceCallback<void(const network::ResourceRequest&)>
          request_intercepted_callback,
      const url::Origin& request_initiator) {
    base::flat_set<std::string> configured_hosts_cache;
    const auto& configured_hosts =
        TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->GetList(
            prefs::kExtensibleEnterpriseSSOConfiguredHosts);
    for (const auto& host : configured_hosts) {
      configured_hosts_cache.insert(host.GetString());
    }
    auto* res = new ProxyingURLLoaderFactory(
        std::move(receiver), std::move(target_factory),
        std::move(configured_hosts_cache), request_initiator);
    res->SetDestructionCallbackForTesting(std::move(dtor_callback));
    res->SetRequestInterceptedCallbackForTesting(
        std::move(request_intercepted_callback));
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> SetupFactoryChain(
      base::OnceCallback<void()> dtor_callback,
      base::OnceCallback<void(const network::ResourceRequest&)>
          request_intercepted_callback,
      const url::Origin& request_initiator) {
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
        std::move(dtor_callback), std::move(request_intercepted_callback),
        request_initiator);

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

 private:
  content::BrowserTaskEnvironment task_environment_;

 protected:
  mojo::Receiver<network::mojom::URLLoaderFactory> test_factory_receiver_;
  network::TestURLLoaderFactory test_factory_;
  network::ResourceRequest test_request_;
  TestingProfile testing_profile_;
};

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest,
       DestroysItselfAfterTargetFactoryDisconnects) {
  base::test::TestFuture<void> proxy_destroyed_future;
  mojo::Remote<network::mojom::URLLoaderFactory> client(
      SetupFactoryChain(base::BindOnce(proxy_destroyed_future.GetCallback()),
                        {}, test_request_.request_initiator.value()));
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
      SetupFactoryChain(base::BindOnce(proxy_destroyed_future.GetCallback()),
                        {}, test_request_.request_initiator.value()));

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
  base::ListValue hosts;
  hosts.Append("foobar.example.com");
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetList(
      prefs::kExtensibleEnterpriseSSOConfiguredHosts, std::move(hosts));

  network::TestURLLoaderFactory terminal_factory;
  scoped_refptr<network::SharedURLLoaderFactory> terminal_shared_factory =
      terminal_factory.GetSafeWeakWrapper();

  network::URLLoaderFactoryBuilder factory_builder;

  ProxyingURLLoaderFactory::MaybeProxyRequest(
      url::Origin::Create(GURL("https://foobar.example.com/")),
      ChromeContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      &testing_profile_, factory_builder);

  scoped_refptr<network::SharedURLLoaderFactory> resulting_factory =
      std::move(factory_builder).Finish(terminal_shared_factory);

  EXPECT_TRUE(
      FactoryHasInterceptors(resulting_factory, terminal_shared_factory));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest, MaybeProxyRequest_NoHTTPS) {
  network::TestURLLoaderFactory terminal_factory;
  scoped_refptr<network::SharedURLLoaderFactory> terminal_shared_factory =
      terminal_factory.GetSafeWeakWrapper();

  network::URLLoaderFactoryBuilder factory_builder;

  ProxyingURLLoaderFactory::MaybeProxyRequest(
      url::Origin::Create(GURL("file://foobar.example.com/")),
      ChromeContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      &testing_profile_, factory_builder);

  scoped_refptr<network::SharedURLLoaderFactory> resulting_factory =
      std::move(factory_builder).Finish(terminal_shared_factory);

  EXPECT_FALSE(
      FactoryHasInterceptors(resulting_factory, terminal_shared_factory));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_NoSubResource) {
  network::TestURLLoaderFactory terminal_factory;
  scoped_refptr<network::SharedURLLoaderFactory> terminal_shared_factory =
      terminal_factory.GetSafeWeakWrapper();

  network::URLLoaderFactoryBuilder factory_builder;

  ProxyingURLLoaderFactory::MaybeProxyRequest(
      url::Origin::Create(GURL("https://foobar.example.com/")),
      ChromeContentBrowserClient::URLLoaderFactoryType::kNavigation,
      &testing_profile_, factory_builder);

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
      &testing_profile_, factory_builder);

  scoped_refptr<network::SharedURLLoaderFactory> resulting_factory =
      std::move(factory_builder).Finish(terminal_shared_factory);

  EXPECT_FALSE(
      FactoryHasInterceptors(resulting_factory, terminal_shared_factory));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryTest,
       MaybeProxyRequest_OktaIdpBlocked) {
  base::ListValue enabled_idps;
  TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetList(
      prefs::kExtensibleEnterpriseSSOEnabledIdps, std::move(enabled_idps));

  network::TestURLLoaderFactory terminal_factory;
  scoped_refptr<network::SharedURLLoaderFactory> terminal_shared_factory =
      terminal_factory.GetSafeWeakWrapper();

  network::URLLoaderFactoryBuilder factory_builder;

  ProxyingURLLoaderFactory::MaybeProxyRequest(
      url::Origin::Create(GURL("https://foobar.example.com/")),
      ChromeContentBrowserClient::URLLoaderFactoryType::kDocumentSubResource,
      &testing_profile_, factory_builder);

  scoped_refptr<network::SharedURLLoaderFactory> resulting_factory =
      std::move(factory_builder).Finish(terminal_shared_factory);

  EXPECT_FALSE(
      FactoryHasInterceptors(resulting_factory, terminal_shared_factory));
}

class PlatformAuthProxyingURLLoaderFactoryInterceptTest
    : public PlatformAuthProxyingURLLoaderFactoryTest {
 protected:
  void TestInterception(const network::ResourceRequest& request,
                        bool interception_expected,
                        const url::Origin& request_initiator) {
    // This test is meant to verify pattern matching, so the host must be
    // allowed.
    base::ListValue hosts;
    hosts.Append(request.url.host());
    TestingBrowserProcess::GetGlobal()->GetTestingLocalState()->SetList(
        prefs::kExtensibleEnterpriseSSOConfiguredHosts, std::move(hosts));

    base::test::TestFuture<const network::ResourceRequest&>
        request_intercepted_future;

    mojo::Remote<network::mojom::URLLoaderFactory> client(SetupFactoryChain(
        {}, request_intercepted_future.GetCallback(), request_initiator));
    mojo::PendingRemote<network::mojom::URLLoader> url_loader_pending_remote;
    network::MockURLLoaderClient mock_client;
    mojo::Receiver<network::mojom::URLLoaderClient> client_receiver(
        &mock_client);

    client->CreateLoaderAndStart(
        url_loader_pending_remote.InitWithNewPipeAndPassReceiver(), 0,
        network::mojom::kURLLoadOptionNone, request,
        client_receiver.BindNewPipeAndPassRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    if (interception_expected) {
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
};

TEST_F(PlatformAuthProxyingURLLoaderFactoryInterceptTest,
       InterceptsValidRequest) {
  const GURL url = GURL(
      "https://foobar.example.com/idp/idx/authenticators/"
      "sso_extension/transactions/123/verify");
  network::ResourceRequest request;
  request.url = url;
  request.method = "POST";
  request.request_initiator = url::Origin::Create(url);
  request.mode = network::mojom::RequestMode::kCors;
  request.headers.SetHeader("User-Agent", "header value");
  request.headers.SetHeader("X-Okta-User-Agent-Extended", "header value");
  request.headers.SetHeader("Accept", "header value");
  TestInterception(request, true, url::Origin::Create(url));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryInterceptTest,
       ChecksRequestInitiatorMissing) {
  const GURL url = GURL(
      "https://foobar.example.com/idp/idx/authenticators/"
      "sso_extension/transactions/123/verify");
  network::ResourceRequest request;
  request.url = url;
  request.method = "POST";
  request.request_initiator = std::nullopt;
  request.mode = network::mojom::RequestMode::kCors;
  TestInterception(request, false, url::Origin::Create(url));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryInterceptTest,
       ChecksRequestInitiatorInvalid) {
  const GURL url = GURL(
      "https://foobar.example.com/idp/idx/authenticators/"
      "sso_extension/transactions/123/verify");
  network::ResourceRequest request;
  request.url = url;
  request.method = "POST";
  request.request_initiator =
      url::Origin::Create(GURL("https://not.origin.com"));
  request.mode = network::mojom::RequestMode::kCors;
  TestInterception(request, false, url::Origin::Create(url));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryInterceptTest, ChecksSameOrigin) {
  const GURL url = GURL(
      "https://foobar.example.com/idp/idx/authenticators/"
      "sso_extension/transactions/123/verify");
  const GURL origin = GURL("https://not.same.origin.com");
  network::ResourceRequest request;
  request.url = url;
  request.method = "POST";
  request.request_initiator = url::Origin::Create(origin);
  request.mode = network::mojom::RequestMode::kCors;
  TestInterception(request, false, url::Origin::Create(origin));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryInterceptTest, ChecksRequestMode) {
  const GURL url = GURL(
      "https://foobar.example.com/idp/idx/authenticators/"
      "sso_extension/transactions/123/verify");
  network::ResourceRequest request;
  request.url = url;
  request.method = "POST";
  request.request_initiator = url::Origin::Create(GURL(url));
  request.mode = network::mojom::RequestMode::kNoCors;
  TestInterception(request, false, url::Origin::Create(url));
}

TEST_F(PlatformAuthProxyingURLLoaderFactoryInterceptTest,
       RejectsForbiddenHeaders) {
  const GURL url = GURL(
      "https://foobar.example.com/idp/idx/authenticators/"
      "sso_extension/transactions/123/verify");
  network::ResourceRequest request;
  request.url = url;
  request.method = "POST";
  request.request_initiator = url::Origin::Create(GURL(url));
  request.mode = network::mojom::RequestMode::kCors;
  // access-control-request-headers is a forbidden request header.
  request.headers.SetHeader("access-control-request-headers", "Content-Type");
  TestInterception(request, false, url::Origin::Create(url));
}

struct InterceptTestParams {
  const std::string_view url;
  bool should_intercept;
  std::string method = "POST";
};

class PlatformAuthProxyingURLLoaderFactoryRequestMatchingTest
    : public PlatformAuthProxyingURLLoaderFactoryInterceptTest,
      public testing::WithParamInterface<InterceptTestParams> {};

TEST_P(PlatformAuthProxyingURLLoaderFactoryRequestMatchingTest,
       ShouldInterceptMatchingURLs) {
  const InterceptTestParams& params = GetParam();
  const GURL url = GURL(params.url);

  network::ResourceRequest request;
  request.url = url;
  request.method = params.method;
  request.request_initiator = url::Origin::Create(GURL(url));
  request.mode = network::mojom::RequestMode::kCors;
  TestInterception(request, params.should_intercept, url::Origin::Create(url));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PlatformAuthProxyingURLLoaderFactoryRequestMatchingTest,
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
