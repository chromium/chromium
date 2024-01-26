// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_
#define CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/public/resolve_error_info.h"
#include "services/network/public/mojom/host_resolver.mojom-forward.h"
#include "services/network/test/test_network_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_browser_net {

class FakeHostResolver : public network::mojom::HostResolver {
 public:
  enum Response {
    kNoResponse = 0,
    kEmptyResponse = 1,
    kOneAddressResponse = 2,
  };

  struct SingleResult {
    SingleResult(int32_t result,
                 net::ResolveErrorInfo resolve_error_info,
                 Response response);

    int32_t result;
    net::ResolveErrorInfo resolve_error_info;
    Response response;
  };

  FakeHostResolver(
      mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver,
      std::vector<SingleResult> result_list);

  FakeHostResolver(
      mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver,
      int32_t result,
      net::ResolveErrorInfo resolve_error_info,
      Response response);

  ~FakeHostResolver() override;

  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient>
          pending_response_client) override;

  void MdnsListen(
      const net::HostPortPair& host,
      net::DnsQueryType query_type,
      mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override;

 private:
  mojo::Receiver<network::mojom::HostResolver> receiver_;
  std::vector<SingleResult> result_list_;
  size_t next_result_ = 0;
};

class HangingHostResolver : public network::mojom::HostResolver {
 public:
  explicit HangingHostResolver(
      mojo::PendingReceiver<network::mojom::HostResolver> resolver_receiver);
  ~HangingHostResolver() override;

  void ResolveHost(
      network::mojom::HostResolverHostPtr host,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      network::mojom::ResolveHostParametersPtr optional_parameters,
      mojo::PendingRemote<network::mojom::ResolveHostClient> response_client)
      override;

  void MdnsListen(
      const net::HostPortPair& host,
      net::DnsQueryType query_type,
      mojo::PendingRemote<network::mojom::MdnsListenClient> response_client,
      MdnsListenCallback callback) override;

 private:
  mojo::Receiver<network::mojom::HostResolver> receiver_;
  mojo::Remote<network::mojom::ResolveHostClient> response_client_;
};

class FakeHostResolverNetworkContext : public network::TestNetworkContext {
 public:
  FakeHostResolverNetworkContext(
      std::vector<FakeHostResolver::SingleResult> current_config_result_list,
      std::vector<FakeHostResolver::SingleResult> google_config_result_list);
  ~FakeHostResolverNetworkContext() override;

  void CreateHostResolver(
      const std::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override;

 private:
  std::vector<FakeHostResolver::SingleResult> current_config_result_list_;
  std::vector<FakeHostResolver::SingleResult> google_config_result_list_;
  std::unique_ptr<FakeHostResolver> current_config_resolver_;
  std::unique_ptr<FakeHostResolver> google_config_resolver_;
};

class HangingHostResolverNetworkContext : public network::TestNetworkContext {
 public:
  HangingHostResolverNetworkContext();
  ~HangingHostResolverNetworkContext() override;

  void CreateHostResolver(
      const std::optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override;

 private:
  std::unique_ptr<HangingHostResolver> resolver_;
};

class FakeDnsConfigChangeManager
    : public network::mojom::DnsConfigChangeManager {
 public:
  explicit FakeDnsConfigChangeManager(
      mojo::PendingReceiver<network::mojom::DnsConfigChangeManager> receiver);
  ~FakeDnsConfigChangeManager() override;

  // mojom::DnsConfigChangeManager implementation:
  void RequestNotifications(
      mojo::PendingRemote<network::mojom::DnsConfigChangeManagerClient> client)
      override;

  void SimulateDnsConfigChange();

 private:
  mojo::Receiver<network::mojom::DnsConfigChangeManager> receiver_;
  mojo::Remote<network::mojom::DnsConfigChangeManagerClient> client_;
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_
