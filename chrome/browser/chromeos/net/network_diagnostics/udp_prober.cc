// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/udp_prober.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace chromeos {
namespace network_diagnostics {

UdpProber::UdpProber(NetworkContextGetter network_context_getter,
                     net::HostPortPair host_port_pair,
                     base::span<const uint8_t> data,
                     net::NetworkTrafficAnnotationTag tag,
                     UdpProbeCompleteCallback callback)
    : network_context_getter_(std::move(network_context_getter)),
      host_port_pair_(host_port_pair),
      data_(std::move(data)),
      tag_(tag),
      callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!data_.empty());
  DCHECK(callback_);
  DCHECK(!host_port_pair_.IsEmpty());

  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  DCHECK(network_context);

  host_resolver_ = std::make_unique<HostResolver>(
      host_port_pair_, network_context,
      base::BindOnce(&UdpProber::OnHostResolutionComplete,
                     weak_factory_.GetWeakPtr()));
}

UdpProber::~UdpProber() = default;

void UdpProber::OnHostResolutionComplete(
    HostResolver::ResolutionResult& resolution_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool success = resolution_result.result == net::OK &&
                 !resolution_result.resolved_addresses->empty() &&
                 resolution_result.resolved_addresses.has_value();
  if (!success) {
    OnDone(resolution_result.result, ProbeExitEnum::kDnsFailure);
    return;
  }

  network::mojom::NetworkContext* network_context =
      network_context_getter_.Run();
  DCHECK(network_context);

  auto pending_receiver = udp_socket_remote_.BindNewPipeAndPassReceiver();
  udp_socket_remote_.set_disconnect_handler(
      base::BindOnce(&UdpProber::OnDisconnect, weak_factory_.GetWeakPtr()));

  auto pending_remote =
      udp_socket_listener_receiver_.BindNewPipeAndPassRemote();
  udp_socket_listener_receiver_.set_disconnect_handler(
      base::BindOnce(&UdpProber::OnDisconnect, weak_factory_.GetWeakPtr()));

  network_context->CreateUDPSocket(std::move(pending_receiver),
                                   std::move(pending_remote));
  udp_socket_remote_->Connect(
      resolution_result.resolved_addresses.value().front(), nullptr,
      base::BindOnce(&UdpProber::OnConnectComplete,
                     weak_factory_.GetWeakPtr()));
}

void UdpProber::OnConnectComplete(
    int result,
    const base::Optional<net::IPEndPoint>& local_addr_out) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != net::OK) {
    OnDone(result, ProbeExitEnum::kConnectFailure);
    return;
  }
  udp_socket_remote_->Send(
      std::move(data_), net::MutableNetworkTrafficAnnotationTag(tag_),
      base::BindOnce(&UdpProber::OnSendComplete, weak_factory_.GetWeakPtr()));
}

void UdpProber::OnSendComplete(int result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != net::OK) {
    OnDone(result, ProbeExitEnum::kSendFailure);
    return;
  }
  udp_socket_remote_->ReceiveMore(/*num_additional_datagrams=*/1);
}

void UdpProber::OnReceived(int32_t result,
                           const base::Optional<net::IPEndPoint>& src_ip,
                           base::Optional<base::span<const uint8_t>> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != net::OK) {
    OnDone(result, ProbeExitEnum::kNetworkErrorOnReceiveFailure);
    return;
  }

  // The UdpProber instance is only interested in validating whether
  // data can be received from the destination host.
  if (!data.has_value() || data.value().empty()) {
    // Note that net::ERR_FAILED is reported even if |result| is net::OK
    // when no data is received.
    OnDone(net::ERR_FAILED, ProbeExitEnum::kNoDataReceivedFailure);
    return;
  }
  OnDone(net::OK, ProbeExitEnum::kSuccess);
}

void UdpProber::OnDone(int result, ProbeExitEnum probe_exit_enum) {
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

void UdpProber::OnDisconnect() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnDone(net::ERR_FAILED, ProbeExitEnum::kMojoDisconnectFailure);
}

}  // namespace network_diagnostics
}  // namespace chromeos
