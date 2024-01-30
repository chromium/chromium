// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_UDP_PROBER_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_UDP_PROBER_H_

#include <cstdint>
#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/network_context_getter.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace ash::network_diagnostics {

// Uses a UDP socket to send data to a remote destination. After sending data,
// the prober listens for received data. It confirms that data was received but
// does not validate the content, hence no data is parsed. Used by network
// diagnostic routines.
class UdpProber {
 public:
  // Lists the ways a prober may end. The callback passed into the prober's
  // constructor is invoked while exiting.
  enum ProbeExitEnum {
    kDnsFailure,
    kConnectFailure,
    kSendFailure,
    kNetworkErrorOnReceiveFailure,
    kMojoDisconnectFailure,
    kNoDataReceivedFailure,
    kTimeout,
    kSuccess,
  };
  using UdpProbeCompleteCallback =
      base::OnceCallback<void(int result, ProbeExitEnum probe_exit_enum)>;

  virtual ~UdpProber() = default;

  // Creates a UdpProber instance which resolves |host_port_pair| and starts the
  // UDP probe.  See implementation for more details.
  static std::unique_ptr<UdpProber> Start(
      network::NetworkContextGetter network_context_getter,
      net::HostPortPair host_port_pair,
      base::span<const uint8_t> data,
      net::NetworkTrafficAnnotationTag tag,
      base::TimeDelta timeout_after_host_resolution,
      UdpProbeCompleteCallback callback);
};

}  // namespace ash::network_diagnostics

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_DIAGNOSTICS_UDP_PROBER_H_
