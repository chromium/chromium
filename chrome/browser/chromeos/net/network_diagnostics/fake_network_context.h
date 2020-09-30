// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_

#include <deque>
#include <memory>

#include "chrome/browser/chromeos/net/network_diagnostics/fake_host_resolver.h"
#include "services/network/test/test_network_context.h"

namespace chromeos {
namespace network_diagnostics {

// Used in unit tests, the FakeNetworkContext class simulates the behavior of a
// network context.
class FakeNetworkContext : public network::TestNetworkContext {
 public:
  FakeNetworkContext();
  ~FakeNetworkContext() override;

  // network::TestNetworkContext:
  void CreateHostResolver(
      const base::Optional<net::DnsConfigOverrides>& config_overrides,
      mojo::PendingReceiver<network::mojom::HostResolver> receiver) override;

  // Sets the fake dns results.
  void set_fake_dns_results(
      std::deque<FakeHostResolver::DnsResult*> fake_dns_results) {
    fake_dns_results_ = std::move(fake_dns_results);
  }

 private:
  std::unique_ptr<FakeHostResolver> resolver_;
  std::deque<FakeHostResolver::DnsResult*> fake_dns_results_;
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_FAKE_NETWORK_CONTEXT_H_
