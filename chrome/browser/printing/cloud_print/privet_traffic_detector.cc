// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_traffic_detector.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/sys_byteorder.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/util.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace {

const int kMaxRestartAttempts = 10;

void OnGetNetworkList(
    base::OnceCallback<void(net::NetworkInterfaceList)> callback,
    const base::Optional<net::NetworkInterfaceList>& networks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!networks.has_value())
    return;

  net::NetworkInterfaceList ip4_networks;
  for (const auto& network : networks.value()) {
    net::AddressFamily address_family = net::GetAddressFamily(network.address);
    if (address_family == net::ADDRESS_FAMILY_IPV4 &&
        network.prefix_length >= 24) {
      ip4_networks.push_back(network);
    }
  }

  net::IPAddress localhost_prefix(127, 0, 0, 0);
  ip4_networks.push_back(net::NetworkInterface(
      "lo", "lo", 0, net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
      localhost_prefix, 8, net::IP_ADDRESS_ATTRIBUTE_NONE));

  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(std::move(callback), std::move(ip4_networks)));
}

void GetNetworkListOnUIThread(
    base::OnceCallback<void(net::NetworkInterfaceList)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(&OnGetNetworkList, std::move(callback)));
}

void CreateUDPSocketOnUIThread(
    content::BrowserContext* profile,
    mojo::PendingReceiver<network::mojom::UDPSocket> receiver,
    mojo::PendingRemote<network::mojom::UDPSocketListener> listener_remote) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network::mojom::NetworkContext* network_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetNetworkContext();
  network_context->CreateUDPSocket(std::move(receiver),
                                   std::move(listener_remote));
}

}  // namespace

namespace cloud_print {

PrivetTrafficDetector::PrivetTrafficDetector(
    content::BrowserContext* profile,
    base::RepeatingClosure on_traffic_detected)
    : helper_(new Helper(profile, on_traffic_detected)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(&PrivetTrafficDetector::Helper::ScheduleRestart,
                                base::Unretained(helper_)));
}

PrivetTrafficDetector::~PrivetTrafficDetector() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  base::DeleteSoon(FROM_HERE, {content::BrowserThread::IO}, helper_);
}

void PrivetTrafficDetector::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&PrivetTrafficDetector::Helper::HandleConnectionChanged,
                     base::Unretained(helper_), type));
}

PrivetTrafficDetector::Helper::Helper(
    content::BrowserContext* profile,
    base::RepeatingClosure on_traffic_detected)
    : profile_(profile),
      on_traffic_detected_(on_traffic_detected),
      restart_attempts_(kMaxRestartAttempts) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

PrivetTrafficDetector::Helper::~Helper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void PrivetTrafficDetector::Helper::HandleConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  restart_attempts_ = kMaxRestartAttempts;
  if (type != network::mojom::ConnectionType::CONNECTION_NONE) {
    ScheduleRestart();
  }
}

void PrivetTrafficDetector::Helper::ScheduleRestart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  ResetConnection();
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &GetNetworkListOnUIThread,
          base::BindOnce(&Helper::Restart, weak_ptr_factory_.GetWeakPtr())),
      base::TimeDelta::FromSeconds(3));
}

void PrivetTrafficDetector::Helper::Restart(
    net::NetworkInterfaceList networks) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  networks_ = std::move(networks);
  Bind();
}

void PrivetTrafficDetector::Helper::Bind() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&CreateUDPSocketOnUIThread, profile_,
                                socket_.BindNewPipeAndPassReceiver(),
                                listener_receiver_.BindNewPipeAndPassRemote()));

  network::mojom::UDPSocketOptionsPtr socket_options =
      network::mojom::UDPSocketOptions::New();
  socket_options->allow_address_sharing_for_multicast = true;
  socket_options->multicast_loopback_mode = false;

  socket_->Bind(
      net::dns_util::GetMdnsReceiveEndPoint(net::ADDRESS_FAMILY_IPV4),
      std::move(socket_options),
      base::BindOnce(
          &Helper::OnBindComplete, weak_ptr_factory_.GetWeakPtr(),
          net::dns_util::GetMdnsGroupEndPoint(net::ADDRESS_FAMILY_IPV4)));
}

void PrivetTrafficDetector::Helper::OnBindComplete(
    net::IPEndPoint multicast_group_addr,
    int rv,
    const base::Optional<net::IPEndPoint>& ip_endpoint) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (rv == net::OK) {
    socket_->JoinGroup(multicast_group_addr.address(),
                       base::BindOnce(&Helper::OnJoinGroupComplete,
                                      weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (restart_attempts_-- > 0)
    ScheduleRestart();
}

bool PrivetTrafficDetector::Helper::IsSourceAcceptable() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  for (const auto& network : networks_) {
    if (net::IPAddressMatchesPrefix(recv_addr_.address(), network.address,
                                    network.prefix_length)) {
      return true;
    }
  }
  return false;
}

bool PrivetTrafficDetector::Helper::IsPrivetPacket(
    base::span<const uint8_t> data) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (data.size() <= sizeof(net::dns_protocol::Header) ||
      !IsSourceAcceptable()) {
    return false;
  }

  const char* buffer_begin = reinterpret_cast<const char*>(data.data());
  const char* buffer_end = buffer_begin + data.size();
  const net::dns_protocol::Header* header =
      reinterpret_cast<const net::dns_protocol::Header*>(buffer_begin);
  // Check if response packet.
  if (!(header->flags & base::HostToNet16(net::dns_protocol::kFlagResponse)))
    return false;

  static const char kPrivetDeviceTypeDnsString[] = "\x07_privet";
  const char* substring_begin = kPrivetDeviceTypeDnsString;
  const char* substring_end =
      substring_begin + base::size(kPrivetDeviceTypeDnsString) - 1;
  // Check for expected substring, any Privet device must include this.
  return std::search(buffer_begin, buffer_end, substring_begin,
                     substring_end) != buffer_end;
}

void PrivetTrafficDetector::Helper::OnJoinGroupComplete(int rv) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (rv == net::OK) {
    // Reset on success.
    restart_attempts_ = kMaxRestartAttempts;
    socket_->ReceiveMoreWithBufferSize(1, net::dns_protocol::kMaxMulticastSize);
    return;
  }

  if (restart_attempts_-- > 0)
    ScheduleRestart();
}

void PrivetTrafficDetector::Helper::ResetConnection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  socket_.reset();
  listener_receiver_.reset();
}

void PrivetTrafficDetector::Helper::OnReceived(
    int32_t result,
    const base::Optional<net::IPEndPoint>& src_addr,
    base::Optional<base::span<const uint8_t>> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (result != net::OK)
    return;

  // |data| and |src_addr| are guaranteed to be non-null when |result| is
  // net::OK
  recv_addr_ = src_addr.value();
  if (IsPrivetPacket(data.value())) {
    ResetConnection();
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   on_traffic_detected_);
  } else {
    socket_->ReceiveMoreWithBufferSize(1, net::dns_protocol::kMaxMulticastSize);
  }
}

}  // namespace cloud_print
