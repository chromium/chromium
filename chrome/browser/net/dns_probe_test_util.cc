// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_test_util.h"

#include <stdint.h>
#include <utility>

#include "chrome/browser/net/dns_probe_runner.h"
#include "net/base/ip_address.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

namespace {

static std::optional<net::AddressList> AddressListForResponse(
    FakeHostResolver::Response response) {
  std::optional<net::AddressList> resolved_addresses;
  switch (response) {
    case FakeHostResolver::kNoResponse:
      break;
    case FakeHostResolver::kEmptyResponse:
      resolved_addresses = net::AddressList();
      break;
    case FakeHostResolver::kOneAddressResponse:
      resolved_addresses =
          net::AddressList(net::IPEndPoint(net::IPAddress(192, 168, 1, 1), 0));
      break;
  }
  return resolved_addresses;
}

}  // namespace

FakeHostResolver::SingleResult::SingleResult(
    int32_t result,
    net::ResolveErrorInfo resolve_error_info,
    Response response)
    : result(result),
      resolve_error_info(resolve_error_info),
      response(response) {
  DCHECK(result == net::OK || result == net::ERR_NAME_NOT_RESOLVED);
}

FakeHostResolver::FakeHostResolver(
    mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver,
    std::vector<SingleResult> result_list)
    : receiver_(this, std::move(resolver_receiver)),
      result_list_(result_list) {}

FakeHostResolver::FakeHostResolver(
    mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver,
    int32_t result,
    net::ResolveErrorInfo resolve_error_info,
    Response response)
    : FakeHostResolver(std::move(resolver_receiver),
                       {SingleResult(result, resolve_error_info, response)}) {}

FakeHostResolver::~FakeHostResolver() = default;

void FakeHostResolver::ResolveHost(
    network::mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    network::mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<network::mojom::ResolveHostClient>
        pending_response_client) {
  EXPECT_TRUE(network_anonymization_key.IsTransient());

  const SingleResult& cur_result = result_list_[next_result_];
  if (next_result_ + 1 < result_list_.size())
    next_result_++;
  mojo::Remote<network::mojom::ResolveHostClient> response_client(
      std::move(pending_response_client));
  response_client->OnComplete(cur_result.result, cur_result.resolve_error_info,
                              AddressListForResponse(cur_result.response),
                              /*endpoint_results_with_metadata=*/std::nullopt);
}

void FakeHostResolver::MdnsListen(
    const net::HostPortPair& host,
    net::DnsQueryType query_type,
    mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
    MdnsListenCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

HangingHostResolver::HangingHostResolver(
    mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver)
    : receiver_(this, std::move(resolver_receiver)) {}

HangingHostResolver::~HangingHostResolver() = default;

void HangingHostResolver::ResolveHost(
    network::mojom::HostResolverHostPtr host,
    const net::NetworkAnonymizationKey& network_anonymization_key,
    network::mojom::ResolveHostParametersPtr optional_parameters,
    mojo::PendingRemote<network::mojom::ResolveHostClient> response_client) {
  EXPECT_TRUE(network_anonymization_key.IsTransient());

  // Intentionally do not call response_client->OnComplete, but hang onto the
  // |response_client| since destroying that also causes the mojo
  // set_connection_error_handler handler to be called.
  response_client_.Bind(std::move(response_client));
}

void HangingHostResolver::MdnsListen(
    const net::HostPortPair& host,
    net::DnsQueryType query_type,
    mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
    MdnsListenCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

FakeHostResolverNetworkContext::FakeHostResolverNetworkContext(
    std::vector<FakeHostResolver::SingleResult> current_config_result_list,
    std::vector<FakeHostResolver::SingleResult> google_config_result_list)
    : current_config_result_list_(std::move(current_config_result_list)),
      google_config_result_list_(std::move(google_config_result_list)) {}

FakeHostResolverNetworkContext::~FakeHostResolverNetworkContext() = default;

void FakeHostResolverNetworkContext::CreateHostResolver(
    const std::optional<net::DnsConfigOverrides>& config_overrides,
    mojo::PendingReceiver<network::mojom::HostResolver> receiver) {
  ASSERT_TRUE(config_overrides);
  if (!config_overrides->nameservers) {
    if (!current_config_resolver_) {
      current_config_resolver_ = std::make_unique<FakeHostResolver>(
          std::move(receiver), current_config_result_list_);
    }
  } else {
    if (!google_config_resolver_) {
      google_config_resolver_ = std::make_unique<FakeHostResolver>(
          std::move(receiver), google_config_result_list_);
    }
  }
}

HangingHostResolverNetworkContext::HangingHostResolverNetworkContext() =
    default;
HangingHostResolverNetworkContext::~HangingHostResolverNetworkContext() =
    default;

void HangingHostResolverNetworkContext::CreateHostResolver(
    const std::optional<net::DnsConfigOverrides>& config_overrides,
    mojo::PendingReceiver<network::mojom::HostResolver> receiver) {
  resolver_ = std::make_unique<HangingHostResolver>(std::move(receiver));
}

FakeDnsConfigChangeManager::FakeDnsConfigChangeManager(
    mojo::PendingReceiver<network::mojom::DnsConfigChangeManager> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeDnsConfigChangeManager::~FakeDnsConfigChangeManager() = default;

void FakeDnsConfigChangeManager::RequestNotifications(
    mojo::PendingRemote<network::mojom::DnsConfigChangeManagerClient> client) {
  ASSERT_FALSE(client_);
  client_.Bind(std::move(client));
}

void FakeDnsConfigChangeManager::SimulateDnsConfigChange() {
  ASSERT_TRUE(client_);
  client_->OnDnsConfigChanged();
}

}  // namespace chrome_browser_net
