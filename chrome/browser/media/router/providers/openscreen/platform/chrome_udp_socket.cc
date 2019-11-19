// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/chrome_udp_socket.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/openscreen/src/platform/base/udp_packet.h"

// Open Screen expects us to provide linked implementations of some of its
// static create methods, which have to be in their namespace.
namespace openscreen {
namespace platform {

// static
ErrorOr<UdpSocketUniquePtr> UdpSocket::Create(
    TaskRunner* task_runner,
    Client* client,
    const IPEndpoint& local_endpoint) {
  auto* const manager = SystemNetworkContextManager::GetInstance();
  network::mojom::NetworkContext* const network_context =
      manager ? manager->GetContext() : nullptr;
  if (!network_context) {
    return Error::Code::kInitializationFailure;
  }

  mojo::PendingRemote<network::mojom::UDPSocketListener> listener_remote;
  mojo::PendingReceiver<network::mojom::UDPSocketListener> pending_listener =
      listener_remote.InitWithNewPipeAndPassReceiver();

  mojo::Remote<network::mojom::UDPSocket> socket;
  network_context->CreateUDPSocket(socket.BindNewPipeAndPassReceiver(),
                                   std::move(listener_remote));

  return ErrorOr<UdpSocketUniquePtr>(
      std::make_unique<media_router::ChromeUdpSocket>(
          client, local_endpoint, std::move(socket),
          std::move(pending_listener)));
}

}  // namespace platform
}  // namespace openscreen

namespace media_router {

namespace {

using openscreen::Error;
using openscreen::IPAddress;
using openscreen::IPEndpoint;
using openscreen::platform::TaskRunner;
using openscreen::platform::UdpPacket;
using openscreen::platform::UdpSocket;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("open_screen_message", R"(
        semantics {
          sender: "Open Screen"
          description:
            "Open Screen messages are used by the third_party Open Screen "
            "library, in accordance to the specification defined by the Open "
            "Screen protocol. The protocol is available publicly at: "
            "https://github.com/webscreens/openscreenprotocol"
          trigger:
            "Any message that needs to be sent or received by the Open Screen "
            "library."
          data:
            "Messages defined by the Open Screen Protocol specification."
          destination: OTHER
          destination_other:
            "The connection is made to an Open Screen endpoint on the LAN "
            "selected by the user, i.e. via a dialog."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be disabled, but it would not be sent if user "
            "does not connect to a Open Screen endpoint on the local network."
          policy_exception_justification: "Not implemented."
        })");

const net::IPAddress ToChromeNetAddress(const IPAddress& address) {
  switch (address.version()) {
    case IPAddress::Version::kV4: {
      std::array<uint8_t, IPAddress::kV4Size> bytes_v4;
      address.CopyToV4(bytes_v4.data());
      return net::IPAddress(bytes_v4.data(), bytes_v4.size());
    }
    case IPAddress::Version::kV6: {
      std::array<uint8_t, IPAddress::kV6Size> bytes_v6;
      address.CopyToV6(bytes_v6.data());
      return net::IPAddress(bytes_v6.data(), bytes_v6.size());
    }
  }
}

const net::IPEndPoint ToChromeNetEndpoint(const IPEndpoint& endpoint) {
  return net::IPEndPoint(ToChromeNetAddress(endpoint.address), endpoint.port);
}

IPAddress::Version ToOpenScreenVersion(const net::AddressFamily family) {
  switch (family) {
    case net::AddressFamily::ADDRESS_FAMILY_IPV6:
      return IPAddress::Version::kV6;
    case net::AddressFamily::ADDRESS_FAMILY_IPV4:
      return IPAddress::Version::kV4;

    case net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED:
      NOTREACHED();
      return IPAddress::Version::kV4;
  }
}

const IPEndpoint ToOpenScreenEndpoint(const net::IPEndPoint& endpoint) {
  const IPAddress::Version version = ToOpenScreenVersion(endpoint.GetFamily());
  return IPEndpoint{IPAddress{version, endpoint.address().bytes().data()},
                    endpoint.port()};
}

}  // namespace

ChromeUdpSocket::ChromeUdpSocket(
    Client* client,
    const IPEndpoint& local_endpoint,
    mojo::Remote<network::mojom::UDPSocket> udp_socket,
    mojo::PendingReceiver<network::mojom::UDPSocketListener> pending_listener)
    : client_(client),
      local_endpoint_(local_endpoint),
      udp_socket_(std::move(udp_socket)),
      pending_listener_(std::move(pending_listener)) {}

ChromeUdpSocket::~ChromeUdpSocket() = default;

bool ChromeUdpSocket::IsIPv4() const {
  return local_endpoint_.address.IsV4();
}

bool ChromeUdpSocket::IsIPv6() const {
  return local_endpoint_.address.IsV6();
}

IPEndpoint ChromeUdpSocket::GetLocalEndpoint() const {
  return local_endpoint_;
}

void ChromeUdpSocket::Bind() {
  udp_socket_->Bind(ToChromeNetEndpoint(local_endpoint_),
                    nullptr /* socket_options */,
                    base::BindOnce(&ChromeUdpSocket::BindCallback,
                                   weak_ptr_factory_.GetWeakPtr()));
}

// mojom::UDPSocket doesn't have a concept of network interface indices, so
// this is a noop.
void ChromeUdpSocket::SetMulticastOutboundInterface(
    openscreen::platform::NetworkInterfaceIndex ifindex) {}

// mojom::UDPSocket doesn't have a concept of network interface indices, so
// the ifindex argument is ignored here.
void ChromeUdpSocket::JoinMulticastGroup(
    const IPAddress& address,
    openscreen::platform::NetworkInterfaceIndex ifindex) {
  const auto join_address = ToChromeNetAddress(address);
  udp_socket_->JoinGroup(join_address,
                         base::BindOnce(&ChromeUdpSocket::JoinGroupCallback,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void ChromeUdpSocket::SendMessage(const void* data,
                                  size_t length,
                                  const IPEndpoint& dest) {
  const auto send_to_address = ToChromeNetEndpoint(dest);
  base::span<const uint8_t> data_span(static_cast<const uint8_t*>(data),
                                      length);

  udp_socket_->SendTo(
      send_to_address, data_span,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      base::BindOnce(&ChromeUdpSocket::SendCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

// mojom::UDPSocket doesn't have a concept of DSCP, so this is a noop.
void ChromeUdpSocket::SetDscp(openscreen::platform::UdpSocket::DscpMode state) {
}

void ChromeUdpSocket::OnReceived(
    int32_t net_result,
    const base::Optional<net::IPEndPoint>& source_endpoint,
    base::Optional<base::span<const uint8_t>> data) {
  if (!client_) {
    return;  // Ignore if there's no Client to receive the result.
  }

  if (net_result != net::OK) {
    client_->OnRead(this, Error::Code::kSocketReadFailure);
  } else if (data) {
    // TODO(jophba): fixup when UdpPacket provides a data copy constructor.
    UdpPacket packet;
    packet.reserve(data.value().size_bytes());
    std::copy(data.value().begin(), data.value().end(),
              std::back_inserter(packet));
    packet.set_socket(this);
    if (source_endpoint) {
      packet.set_source(ToOpenScreenEndpoint(source_endpoint.value()));
    }
    client_->OnRead(this, std::move(packet));
  }

  udp_socket_->ReceiveMore(1);
}

void ChromeUdpSocket::BindCallback(
    int32_t result,
    const base::Optional<net::IPEndPoint>& address) {
  if (result != net::OK) {
    if (client_) {
      client_->OnError(this, Error(Error::Code::kSocketBindFailure,
                                   net::ErrorToString(result)));
    }
    return;
  }

  // Enable packet receives only if there is a Client to dispatch them to.
  if (client_) {
    // This is an approximate value for number of packets, and may need to be
    // adjusted when we have real world data.
    constexpr int kNumPacketsReadyFor = 30;
    udp_socket_->ReceiveMore(kNumPacketsReadyFor);
  }

  if (address) {
    local_endpoint_ = ToOpenScreenEndpoint(address.value());
    if (pending_listener_.is_valid()) {
      listener_.Bind(std::move(pending_listener_));
    }
  }
}

void ChromeUdpSocket::JoinGroupCallback(int32_t result) {
  if (result != net::OK && client_) {
    client_->OnError(this, Error(Error::Code::kSocketOptionSettingFailure,
                                 net::ErrorToString(result)));
  }
}

void ChromeUdpSocket::SendCallback(int32_t result) {
  if (result != net::OK && client_) {
    client_->OnSendError(this, Error(Error::Code::kSocketSendFailure,
                                     net::ErrorToString(result)));
  }
}

}  // namespace media_router
