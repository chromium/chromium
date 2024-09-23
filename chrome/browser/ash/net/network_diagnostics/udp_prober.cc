// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/udp_prober.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/raw_span.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_host_resolver.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/udp_socket.mojom.h"
#include "url/gurl.h"

namespace ash::network_diagnostics {

// Implements the UdpProber class.
class UdpProberImpl final : public network::mojom::UDPSocketListener,
                            public UdpProber {
 public:
  using ConnectCallback = base::OnceCallback<
      void(int result, const std::optional<net::IPEndPoint>& local_addr_out)>;
  using SendCallback = base::OnceCallback<void(int result)>;

  // Establishes a UDP connection and sends |data| to |host_port_pair|. The
  // traffic sent by the prober is described by |tag|. Since there is no
  // guarantee the host specified by |host_port_pair| will respond to a UDP
  // request, the prober will timeout with a failure after
  // |timeout_after_host_resolution|. As such, the prober will run no longer
  // than the time it takes to perform host resolution +
  // |timeout_after_host_resolution|. Note that the constructor will not invoke
  // |callback|, which is passed into UdpProberImpl during construction. This
  // ensures the UdpProberImpl instance is constructed before |callback| is
  // invoked.  The UdpProberImpl must be created on the UI thread and will
  // invoke |callback| on the UI thread.  |network_context_getter| will be
  // invoked on the UI thread.
  UdpProberImpl(network::NetworkContextGetter network_context_getter,
                net::HostPortPair host_port_pair,
                base::span<const uint8_t> data,
                net::NetworkTrafficAnnotationTag tag,
                base::TimeDelta timeout_after_host_resolution,
                UdpProbeCompleteCallback callback);
  UdpProberImpl(const UdpProberImpl&) = delete;
  UdpProberImpl& operator=(const UdpProberImpl&) = delete;
  ~UdpProberImpl() override;

 private:
  // Processes the results of the DNS resolution done by |host_resolver_|.
  void OnHostResolutionComplete(
      int result,
      const net::ResolveErrorInfo&,
      const std::optional<net::AddressList>& resolved_addresses,
      const std::optional<net::HostResolverEndpointResults>&);

  // On success, the UDP socket is connected to the destination and is ready to
  // send data. On failure, the UdpProberImpl exits with a failure.
  void OnConnectComplete(int result,
                         const std::optional<net::IPEndPoint>& local_addr_out);

  // On success, the UDP socket is ready to receive data. So long as the
  // received data is not empty, it is considered valid. The content itself is
  // not verified.
  void OnSendComplete(int result);

  // network::mojom::UDPSocketListener:
  void OnReceived(int32_t result,
                  const std::optional<net::IPEndPoint>& src_ip,
                  std::optional<base::span<const uint8_t>> data) override;

  // Signals the end of the probe. Manages the clean up and returns a response
  // to the caller.
  void OnDone(int result, ProbeExitEnum probe_exit_enum);

  // Handles disconnects on the UDPSocket remote and UDPSocketListener receiver.
  void OnDisconnect();

  // Gets the active profile-specific network context.
  network::NetworkContextGetter network_context_getter_;
  // Contains the hostname and port.
  net::HostPortPair host_port_pair_;
  // Data to be sent to the destination.
  base::raw_span<const uint8_t> data_;
  // Network annotation tag describing the socket traffic.
  net::NetworkTrafficAnnotationTag tag_;
  // Represents the time after host resolution.
  base::TimeDelta timeout_after_host_resolution_;
  // Times the prober.
  base::OneShotTimer timer_;
  // Host resolver used for DNS lookup.
  std::unique_ptr<network::SimpleHostResolver> host_resolver_;
  // Stores the callback invoked once probe is complete or interrupted.
  UdpProbeCompleteCallback callback_;
  // Holds the UDPSocket remote.
  mojo::Remote<network::mojom::UDPSocket> udp_socket_remote_;
  // Listens to the response from hostname specified by |url_|.
  mojo::Receiver<network::mojom::UDPSocketListener>
      udp_socket_listener_receiver_{this};

  // Must be the last member so that any callbacks taking a weak pointer to this
  // instance are invalidated first during object destruction.
  base::WeakPtrFactory<UdpProberImpl> weak_factory_{this};
};

UdpProberImpl::UdpProberImpl(
    network::NetworkContextGetter network_context_getter,
    net::HostPortPair host_port_pair,
    base::span<const uint8_t> data,
    net::NetworkTrafficAnnotationTag tag,
    base::TimeDelta timeout_after_host_resolution,
    UdpProbeCompleteCallback callback)
    : network_context_getter_(std::move(network_context_getter)),
      host_port_pair_(std::move(host_port_pair)),
      data_(std::move(data)),
      tag_(tag),
      timeout_after_host_resolution_(timeout_after_host_resolution),
      callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!data_.empty());
  DCHECK(callback_);
  DCHECK(!host_port_pair_.IsEmpty());

  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  DCHECK(network_context);

  host_resolver_ = network::SimpleHostResolver::Create(network_context);

  // Resolver host parameter source must be unset or set to ANY in order for DNS
  // queries with BuiltInDnsClientEnabled policy disabled to work (b/353448388).
  network::mojom::ResolveHostParametersPtr parameters =
      network::mojom::ResolveHostParameters::New();
  parameters->dns_query_type = net::DnsQueryType::A;
  parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  // Unretained(this) is safe here because the callback is invoked directly by
  // |host_resolver_| which is owned by |this|.
  host_resolver_->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(host_port_pair_),
      net::NetworkAnonymizationKey::CreateTransient(), std::move(parameters),
      base::BindOnce(&UdpProberImpl::OnHostResolutionComplete,
                     base::Unretained(this)));
}

UdpProberImpl::~UdpProberImpl() = default;

void UdpProberImpl::OnHostResolutionComplete(
    int result,
    const net::ResolveErrorInfo&,
    const std::optional<net::AddressList>& resolved_addresses,
    const std::optional<net::HostResolverEndpointResults>&) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != net::OK) {
    CHECK(!resolved_addresses);
    OnDone(result, ProbeExitEnum::kDnsFailure);
    return;
  }
  CHECK(resolved_addresses);

  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  CHECK(network_context);

  auto pending_receiver = udp_socket_remote_.BindNewPipeAndPassReceiver();
  udp_socket_remote_.set_disconnect_handler(
      base::BindOnce(&UdpProberImpl::OnDisconnect, weak_factory_.GetWeakPtr()));
  DCHECK(udp_socket_remote_.is_bound());

  auto pending_remote =
      udp_socket_listener_receiver_.BindNewPipeAndPassRemote();
  udp_socket_listener_receiver_.set_disconnect_handler(
      base::BindOnce(&UdpProberImpl::OnDisconnect, weak_factory_.GetWeakPtr()));

  network_context->CreateUDPSocket(std::move(pending_receiver),
                                   std::move(pending_remote));
  timer_.Start(
      FROM_HERE, timeout_after_host_resolution_,
      base::BindOnce(&UdpProberImpl::OnDone, weak_factory_.GetWeakPtr(),
                     net::ERR_TIMED_OUT, ProbeExitEnum::kTimeout));
  udp_socket_remote_->Connect(resolved_addresses->front(), nullptr,
                              base::BindOnce(&UdpProberImpl::OnConnectComplete,
                                             weak_factory_.GetWeakPtr()));
}

void UdpProberImpl::OnConnectComplete(
    int result,
    const std::optional<net::IPEndPoint>& local_addr_out) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != net::OK) {
    OnDone(result, ProbeExitEnum::kConnectFailure);
    return;
  }
  udp_socket_remote_->Send(std::move(data_),
                           net::MutableNetworkTrafficAnnotationTag(tag_),
                           base::BindOnce(&UdpProberImpl::OnSendComplete,
                                          weak_factory_.GetWeakPtr()));
}

void UdpProberImpl::OnSendComplete(int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != net::OK) {
    OnDone(result, ProbeExitEnum::kSendFailure);
    return;
  }
  udp_socket_remote_->ReceiveMore(/*num_additional_datagrams=*/1);
}

void UdpProberImpl::OnReceived(int32_t result,
                               const std::optional<net::IPEndPoint>& src_ip,
                               std::optional<base::span<const uint8_t>> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != net::OK) {
    OnDone(result, ProbeExitEnum::kNetworkErrorOnReceiveFailure);
    return;
  }

  // The UdpProberImpl instance is only interested in validating whether
  // data can be received from the destination host.
  if (!data.has_value() || data.value().empty()) {
    // Note that net::ERR_FAILED is reported even if |result| is net::OK
    // when no data is received.
    OnDone(net::ERR_FAILED, ProbeExitEnum::kNoDataReceivedFailure);
    return;
  }
  OnDone(net::OK, ProbeExitEnum::kSuccess);
}

void UdpProberImpl::OnDone(int result, ProbeExitEnum probe_exit_enum) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Invalidate pending callbacks.
  weak_factory_.InvalidateWeakPtrs();
  // Destroy the socket connection.
  udp_socket_listener_receiver_.reset();
  udp_socket_remote_.reset();
  // Reset the host resolver.
  host_resolver_.reset();

  std::move(callback_).Run(result, probe_exit_enum);
}

void UdpProberImpl::OnDisconnect() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnDone(net::ERR_FAILED, ProbeExitEnum::kMojoDisconnectFailure);
}

// static
std::unique_ptr<UdpProber> UdpProber::Start(
    network::NetworkContextGetter network_context_getter,
    net::HostPortPair host_port_pair,
    base::span<const uint8_t> data,
    net::NetworkTrafficAnnotationTag tag,
    base::TimeDelta timeout_after_host_resolution,
    UdpProbeCompleteCallback callback) {
  return std::make_unique<UdpProberImpl>(
      std::move(network_context_getter), host_port_pair, std::move(data), tag,
      timeout_after_host_resolution, std::move(callback));
}

}  // namespace ash::network_diagnostics
