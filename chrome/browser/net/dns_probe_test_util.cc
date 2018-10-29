// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_test_util.h"

#include <stdint.h>

#include "chrome/browser/net/dns_probe_runner.h"
#include "net/base/ip_address.h"
#include "net/dns/dns_config.h"
#include "net/dns/dns_protocol.h"

using net::DnsClient;
using net::DnsConfig;
using net::MockDnsClientRule;
using net::MockDnsClientRuleList;

namespace chrome_browser_net {

std::unique_ptr<DnsClient> CreateMockDnsClientForProbes(
    MockDnsClientRule::ResultType result) {
  DnsConfig config;
  net::IPAddress dns_ip(192, 168, 1, 1);
  const uint16_t kDnsPort = net::dns_protocol::kDefaultPort;
  config.nameservers.push_back(net::IPEndPoint(dns_ip, kDnsPort));

  const uint16_t kTypeA = net::dns_protocol::kTypeA;
  MockDnsClientRuleList rules;
  rules.push_back(MockDnsClientRule(DnsProbeRunner::kKnownGoodHostname, kTypeA,
                                    MockDnsClientRule::Result(result), false));

  return std::unique_ptr<DnsClient>(new net::MockDnsClient(config, rules));
}

}  // namespace chrome_browser_net
