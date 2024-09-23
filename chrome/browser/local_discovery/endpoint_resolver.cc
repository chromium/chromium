// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/endpoint_resolver.h"

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace local_discovery {

EndpointResolver::EndpointResolver() {
  service_discovery_client_ = ServiceDiscoverySharedClient::GetInstance();
}

EndpointResolver::~EndpointResolver() {}

void EndpointResolver::Start(const net::HostPortPair& address,
                             ResultCallback callback) {
#if BUILDFLAG(IS_MAC)
  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(address.host())) {
    NOTREACHED_IN_MIGRATION() << address.ToString();
    // Unexpected, but could be a reason for crbug.com/513505
    base::debug::DumpWithoutCrashing();
    return std::move(callback).Run(net::IPEndPoint());
  }

  // OSX already has IP there.
  std::move(callback).Run(net::IPEndPoint(ip_address, address.port()));
#else   // BUILDFLAG(IS_MAC)
  net::AddressFamily address_family = net::ADDRESS_FAMILY_UNSPECIFIED;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrivetIPv6Only)) {
    address_family = net::ADDRESS_FAMILY_IPV6;
  }

  domain_resolver_ = service_discovery_client_->CreateLocalDomainResolver(
      address.host(), address_family,
      base::BindOnce(&EndpointResolver::DomainResolveComplete,
                     base::Unretained(this), address.port(),
                     std::move(callback)));
  domain_resolver_->Start();
#endif  // BUILDFLAG(IS_MAC)
}

void EndpointResolver::DomainResolveComplete(
    uint16_t port,
    ResultCallback callback,
    bool success,
    const net::IPAddress& address_ipv4,
    const net::IPAddress& address_ipv6) {
  if (!success)
    return std::move(callback).Run(net::IPEndPoint());

  net::IPAddress address = address_ipv4;
  if (!address.IsValid())
    address = address_ipv6;

  DCHECK(address.IsValid());

  std::move(callback).Run(net::IPEndPoint(address, port));
}

}  // namespace local_discovery
