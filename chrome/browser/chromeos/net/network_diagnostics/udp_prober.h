// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_UDP_PROBER_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_UDP_PROBER_H_

#include <cstdint>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/net/network_diagnostics/host_resolver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "url/gurl.h"

namespace chromeos {
namespace network_diagnostics {

// Uses a UDP socket to send data to a remote destination. After sending data,
// the prober listens for received data. It confirms that data was received but
// does not validate the content, hence no data is parsed. Used by network
// diagnostic routines.
class UdpProber : public network::mojom::UDPSocketListener {
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

  using NetworkContextGetter =
      base::RepeatingCallback<network::mojom::NetworkContext*()>;
  using ConnectCallback = base::OnceCallback<
      void(int result, const base::Optional<net::IPEndPoint>& local_addr_out)>;
  using SendCallback = base::OnceCallback<void(int result)>;
  using UdpProbeCompleteCallback =
      base::OnceCallback<void(int result, ProbeExitEnum probe_exit_enum)>;

  // Establishes a UDP connection and sends |data| to |host_port_pair|. The
  // traffic sent by the prober is described by |tag|. Since there is no
  // guarantee the host specified by |host_port_pair| will respond to a UDP
  // request, the prober will timeout with a failure after
  // |timeout_after_host_resolution|. As such, the prober will run no longer
  // than the time it takes to perform host resolution +
  // |timeout_after_host_resolution|. Note that the constructor will not invoke
  // |callback|, which is passed into UdpProber during construction. This
  // ensures the UdpProber instance is constructed before |callback| is invoked.
  // The UdpProber must be created on the UI thread and will invoke |callback|
  // on the UI thread.  |network_context_getter| will be invoked on the UI
  // thread.
  UdpProber(NetworkContextGetter network_context_getter,
            net::HostPortPair host_port_pair,
            base::span<const uint8_t> data,
            net::NetworkTrafficAnnotationTag tag,
            base::TimeDelta timeout_after_host_resolution,
            UdpProbeCompleteCallback callback);
  UdpProber(const UdpProber&) = delete;
  UdpProber& operator=(const UdpProber&) = delete;
  ~UdpProber() override;

 private:
  // Processes the results of the DNS resolution done by |host_resolver_|.
  void OnHostResolutionComplete(
      HostResolver::ResolutionResult& resolution_result);

  // On success, the UDP socket is connected to the destination and is ready to
  // send data. On failure, the UdpProber exits with a failure.
  void OnConnectComplete(int result,
                         const base::Optional<net::IPEndPoint>& local_addr_out);

  // On success, the UDP socket is ready to receive data. So long as the
  // received data is not empty, it is considered valid. The content itself is
  // not verified.
  void OnSendComplete(int result);

  // network::mojom::UDPSocketListener:
  void OnReceived(int32_t result,
                  const base::Optional<net::IPEndPoint>& src_ip,
                  base::Optional<base::span<const uint8_t>> data) override;

  // Signals the end of the probe. Manages the clean up and returns a response
  // to the caller.
  void OnDone(int result, ProbeExitEnum probe_exit_enum);

  // Handles disconnects on the UDPSocket remote and UDPSocketListener receiver.
  void OnDisconnect();

  // Gets the active profile-specific network context.
  NetworkContextGetter network_context_getter_;
  // Contains the hostname and port.
  net::HostPortPair host_port_pair_;
  // Data to be sent to the destination.
  base::span<const uint8_t> data_;
  // Network annotation tag describing the socket traffic.
  net::NetworkTrafficAnnotationTag tag_;
  // Represents the time after host resolution.
  base::TimeDelta timeout_after_host_resolution_;
  // Times the prober.
  base::OneShotTimer timer_;
  // Host resolver used for DNS lookup.
  std::unique_ptr<HostResolver> host_resolver_;
  // Stores the callback invoked once probe is complete or interrupted.
  UdpProbeCompleteCallback callback_;
  // Holds the UDPSocket remote.
  mojo::Remote<network::mojom::UDPSocket> udp_socket_remote_;
  // Listens to the response from hostname specified by |url_|.
  mojo::Receiver<network::mojom::UDPSocketListener>
      udp_socket_listener_receiver_{this};

  // Must be the last member so that any callbacks taking a weak pointer to this
  // instance are invalidated first during object destruction.
  base::WeakPtrFactory<UdpProber> weak_factory_{this};
};

}  // namespace network_diagnostics
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_DIAGNOSTICS_UDP_PROBER_H_
