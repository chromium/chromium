// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/proxy_resolution_service_provider.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "chromeos/ash/components/dbus/system_proxy/system_proxy_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/proxy_resolution/proxy_info.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "services/network/test/test_network_context.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

// The parsed result from ProxyResolutionServiceProvider's D-Bus result (error
// is a string).
struct ResolveProxyResult {
  std::string error;
  std::string proxy_info;
};

// A mock result to configure what the NetworkContext should return (error is an
// integer).
struct LookupProxyForURLMockResult {
  net::Error error = net::ERR_UNEXPECTED;
  std::string proxy_info_pac_string;
};

// Mock NetworkContext that allows controlling the result of
// LookUpProxyForURL().
class MockNetworkContext : public network::TestNetworkContext {
 public:
  MockNetworkContext() {}

  MockNetworkContext(const MockNetworkContext&) = delete;
  MockNetworkContext& operator=(const MockNetworkContext&) = delete;

  ~MockNetworkContext() override {}

  // network::mojom::NetworkContext implementation:
  void LookUpProxyForURL(
      const GURL& url,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      mojo::PendingRemote<::network::mojom::ProxyLookupClient>
          proxy_lookup_client) override {
    last_url_ = url;
    last_network_anonymization_key_ = network_anonymization_key;

    mojo::Remote<::network::mojom::ProxyLookupClient>
        proxy_lookup_client_remote(std::move(proxy_lookup_client));
    if (lookup_proxy_result_.error == net::OK) {
      net::ProxyInfo proxy_info;
      proxy_info.UsePacString(lookup_proxy_result_.proxy_info_pac_string);
      proxy_lookup_client_remote->OnProxyLookupComplete(net::OK, proxy_info);
    } else {
      proxy_lookup_client_remote->OnProxyLookupComplete(
          lookup_proxy_result_.error, std::nullopt);
    }
  }

  void SetNextProxyResult(LookupProxyForURLMockResult mock_result) {
    lookup_proxy_result_ = mock_result;
  }

  const GURL& last_url() const { return last_url_; }
  const net::NetworkAnonymizationKey& last_network_anonymization_key() const {
    return last_network_anonymization_key_;
  }

 private:
  GURL last_url_;
  net::NetworkAnonymizationKey last_network_anonymization_key_;

  LookupProxyForURLMockResult lookup_proxy_result_;

  ScopedStubInstallAttributes test_install_attributes_{
      StubInstallAttributes::CreateCloudManaged("fake-domain", "fake-id")};
};

}  // namespace

class ProxyResolutionServiceProviderTest : public testing::Test {
 public:
  ProxyResolutionServiceProviderTest() {
    service_provider_ = std::make_unique<ProxyResolutionServiceProvider>();
    service_provider_->set_network_context_for_test(&mock_network_context_);

    test_helper_.SetUp(chromeos::kNetworkProxyServiceName,
                       dbus::ObjectPath(chromeos::kNetworkProxyServicePath),
                       chromeos::kNetworkProxyServiceInterface,
                       chromeos::kNetworkProxyServiceResolveProxyMethod,
                       service_provider_.get());
  }

  ProxyResolutionServiceProviderTest(
      const ProxyResolutionServiceProviderTest&) = delete;
  ProxyResolutionServiceProviderTest& operator=(
      const ProxyResolutionServiceProviderTest&) = delete;

  ~ProxyResolutionServiceProviderTest() override {
    test_helper_.TearDown();
  }

 protected:
  // Makes a D-Bus call to |service_provider_|'s ResolveProxy method and sets
  // the parsed response in |result|.
  void CallMethod(const std::string& source_url, ResolveProxyResult* result) {
    dbus::MethodCall method_call(
        chromeos::kNetworkProxyServiceInterface,
        chromeos::kNetworkProxyServiceResolveProxyMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source_url);

    std::unique_ptr<dbus::Response> response =
        test_helper_.CallMethod(&method_call);

    // Parse the |dbus::Response|.
    ASSERT_TRUE(response);
    dbus::MessageReader reader(response.get());
    std::string proxy_info, error;
    EXPECT_TRUE(reader.PopString(&result->proxy_info));
    EXPECT_TRUE(reader.PopString(&result->error));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  MockNetworkContext mock_network_context_;

  std::unique_ptr<ProxyResolutionServiceProvider> service_provider_;
  ServiceProviderTestHelper test_helper_;
};

// Tests the normal success case. The proxy resolver returns a single proxy.
TEST_F(ProxyResolutionServiceProviderTest, Success) {
  mock_network_context_.SetNextProxyResult({net::OK, "PROXY localhost:8080"});

  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result);

  // The response should contain the proxy info and an empty error.
  EXPECT_EQ("PROXY localhost:8080", result.proxy_info);
  EXPECT_EQ("", result.error);
}

// Tests the case where the proxy resolver fails with
// ERR_MANDATORY_PROXY_CONFIGURATION_FAILED.
TEST_F(ProxyResolutionServiceProviderTest, ResolverFailed) {
  mock_network_context_.SetNextProxyResult(
      {net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED, "PROXY localhost:8080"});

  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result);

  // The response should contain empty proxy info and a "mandatory proxy config
  // failed" error (which the error from the resolver will be mapped to).
  EXPECT_EQ("DIRECT", result.proxy_info);
  EXPECT_EQ(net::ErrorToString(net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED),
            result.error);
}

// Tests calling the proxy resolution provider with an invalid URL.
TEST_F(ProxyResolutionServiceProviderTest, BadURL) {
  ResolveProxyResult result;
  CallMethod(":bad-url", &result);

  // The response should contain empty proxy info and a "mandatory proxy config
  // failed" error (which the error from the resolver will be mapped to).
  EXPECT_EQ("DIRECT", result.proxy_info);
  EXPECT_EQ("Invalid URL", result.error);
}

// Tests the failure case where a NetworkContext cannot be retrieved. This could
// happen at certain points during startup/shutdown while the primary profile is
// null.
TEST_F(ProxyResolutionServiceProviderTest, NullNetworkContext) {
  service_provider_->set_network_context_for_test(nullptr);

  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result);

  // The response should contain a failure.
  EXPECT_EQ("DIRECT", result.proxy_info);
  EXPECT_EQ("No NetworkContext", result.error);
}

// Make sure requests use an opaque transient NetworkAnonymizationKey.
TEST_F(ProxyResolutionServiceProviderTest,
       UniqueTransientNetworkIsolationKeys) {
  const GURL kUrl("https://foo.test/food");
  const char kProxyResult[] = "PROXY proxy.test:8080";
  mock_network_context_.SetNextProxyResult({net::OK, kProxyResult});

  ResolveProxyResult result;
  CallMethod(kUrl.spec(), &result);
  EXPECT_EQ(kProxyResult, result.proxy_info);
  EXPECT_EQ("", result.error);
  EXPECT_EQ(kUrl, mock_network_context_.last_url());
  EXPECT_TRUE(
      mock_network_context_.last_network_anonymization_key().IsTransient());
}

// Tests the behaviour of system-proxy when enabled via the feature flag
// `features::kSystemProxyForSystemServices` and via the device policy
// SystemProxySettings.
class ProxyResolutionServiceWithSystemProxyTest
    : public ProxyResolutionServiceProviderTest {
 public:
  ProxyResolutionServiceWithSystemProxyTest()
      : ProxyResolutionServiceProviderTest(),
        local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~ProxyResolutionServiceWithSystemProxyTest() override = default;

  // testing::Test
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSystemProxyForSystemServices);
    ProxyResolutionServiceProviderTest::SetUp();

    SystemProxyClient::InitializeFake();
    SystemProxyManager::Initialize(local_state_.Get());
    SystemProxyManager::Get()->SetSystemServicesProxyUrlForTest(
        "system-proxy:3128");
  }

  void TearDown() override {
    SystemProxyManager::Shutdown();
    SystemProxyClient::Shutdown();
  }

 protected:
  // Makes a D-Bus call to |service_provider_|'s ResolveProxy method and sets
  // the parsed response in |result|.
  void CallMethod(const std::string& source_url,
                  ResolveProxyResult* result,
                  chromeos::SystemProxyOverride system_proxy_override) {
    dbus::MethodCall method_call(
        chromeos::kNetworkProxyServiceInterface,
        chromeos::kNetworkProxyServiceResolveProxyMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source_url);
    writer.AppendInt32(system_proxy_override);

    std::unique_ptr<dbus::Response> response =
        test_helper_.CallMethod(&method_call);

    // Parse the |dbus::Response|.
    ASSERT_TRUE(response);
    dbus::MessageReader reader(response.get());
    std::string proxy_info, error;
    EXPECT_TRUE(reader.PopString(&result->proxy_info));
    EXPECT_TRUE(reader.PopString(&result->error));
  }

 protected:
  ScopedTestingLocalState local_state_;

 private:
  NetworkHandlerTestHelper network_handler_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// When system-proxy is enabled via the flag and the caller explicitly opt into
// using system-proxy for authentication, the network address of system-proxy
// should be added first in the PAC style list of proxies.
TEST_F(ProxyResolutionServiceWithSystemProxyTest, FlagOptIn) {
  mock_network_context_.SetNextProxyResult({net::OK, "PROXY localhost:8080"});

  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result,
             chromeos::SystemProxyOverride::kOptIn);

  // The response should contain the system-proxy address and an empty error.
  EXPECT_EQ("PROXY system-proxy:3128; PROXY localhost:8080", result.proxy_info);
  EXPECT_EQ("", result.error);
}

// If there's no proxy configured in Chrome, the address of system-proxy should
// not be returned by the proxy resolution service.
TEST_F(ProxyResolutionServiceWithSystemProxyTest, DirectProxy) {
  mock_network_context_.SetNextProxyResult({net::OK, "DIRECT"});
  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result,
             chromeos::SystemProxyOverride::kOptIn);

  EXPECT_EQ("DIRECT", result.proxy_info);
  EXPECT_EQ("", result.error);
}

// When system-proxy is enabled via the flag and the caller doesn't explicitly
// opt into using system-proxy for authentication, the address of system-proxy
// should not be returned by the proxy resolution service.
TEST_F(ProxyResolutionServiceWithSystemProxyTest, FlagDefault) {
  mock_network_context_.SetNextProxyResult({net::OK, "PROXY localhost:8080"});
  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result,
             chromeos::SystemProxyOverride::kDefault);

  EXPECT_EQ("PROXY localhost:8080", result.proxy_info);
  EXPECT_EQ("", result.error);
}

// When system-proxy is enabled via policy and the caller doesn't
// explicitly opt into using system-proxy for authentication, the network
// address of system-proxy should still be added first in the PAC style list of
// proxies.
TEST_F(ProxyResolutionServiceWithSystemProxyTest, PolicyDefault) {
  mock_network_context_.SetNextProxyResult({net::OK, "PROXY localhost:8080"});
  // Enable system-proxy via policy.
  SystemProxyManager::Get()->SetSystemProxyEnabledForTest(true);
  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result,
             chromeos::SystemProxyOverride::kDefault);

  EXPECT_EQ("PROXY system-proxy:3128; PROXY localhost:8080", result.proxy_info);
  EXPECT_EQ("", result.error);
}

// When system-proxy is enabled via policy and the caller explicitly opts out of
// using system-proxy for authentication, the network address of system-proxy
// should be not be returned.
TEST_F(ProxyResolutionServiceWithSystemProxyTest, PolicyOptOut) {
  mock_network_context_.SetNextProxyResult({net::OK, "PROXY localhost:8080"});
  // Enable system-proxy via policy.
  SystemProxyManager::Get()->SetSystemProxyEnabledForTest(true);
  ResolveProxyResult result;
  CallMethod("http://www.gmail.com/", &result,
             chromeos::SystemProxyOverride::kOptOut);

  EXPECT_EQ("PROXY localhost:8080", result.proxy_info);
  EXPECT_EQ("", result.error);
}

}  // namespace ash
