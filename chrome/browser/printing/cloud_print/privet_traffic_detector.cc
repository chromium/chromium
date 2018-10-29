// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/privet_traffic_detector.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/sys_byteorder.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/address_family.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/mdns_client.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace {

const int kMaxRestartAttempts = 10;

void GetNetworkListInBackground(
    base::OnceCallback<void(net::NetworkInterfaceList)> callback) {
  net::NetworkInterfaceList networks;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        base::BlockingType::MAY_BLOCK);
    if (!GetNetworkList(&networks, net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES))
      return;
  }

  net::NetworkInterfaceList ip4_networks;
  for (const auto& network : networks) {
    net::AddressFamily address_family = net::GetAddressFamily(network.address);
    if (address_family == net::ADDRESS_FAMILY_IPV4 &&
        network.prefix_length >= 24) {
      ip4_networks.push_back(network);
    }
  }

  net::IPAddress localhost_prefix(127, 0, 0, 0);
  ip4_networks.push_back(
      net::NetworkInterface("lo",
                            "lo",
                            0,
                            net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
                            localhost_prefix,
                            8,
                            net::IP_ADDRESS_ATTRIBUTE_NONE));
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(std::move(callback), std::move(ip4_networks)));
}

void CreateUDPSocketOnUIThread(
    content::BrowserContext* profile,
    network::mojom::UDPSocketRequest request,
    network::mojom::UDPSocketReceiverPtr receiver_ptr) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network::mojom::NetworkContext* network_context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetNetworkContext();
  network_context->CreateUDPSocket(std::move(request), std::move(receiver_ptr));
}

}  // namespace

namespace cloud_print {

PrivetTrafficDetector::PrivetTrafficDetector(
    content::BrowserContext* profile,
    const base::RepeatingClosure& on_traffic_detected)
    : helper_(new Helper(profile, on_traffic_detected)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&PrivetTrafficDetector::Helper::ScheduleRestart,
                     base::Unretained(helper_)));
}

PrivetTrafficDetector::~PrivetTrafficDetector() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                     helper_);
}

void PrivetTrafficDetector::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&PrivetTrafficDetector::Helper::HandleConnectionChanged,
                     base::Unretained(helper_), type));
}

PrivetTrafficDetector::Helper::Helper(
    content::BrowserContext* profile,
    const base::RepeatingClosure& on_traffic_detected)
    : profile_(profile),
      on_traffic_detected_(on_traffic_detected),
      restart_attempts_(kMaxRestartAttempts),
      receiver_binding_(this),
      weak_ptr_factory_(this) {
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
  base::PostDelayedTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(
          &GetNetworkListInBackground,
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
  if (!start_time_.is_null()) {
    base::TimeDelta time_delta = base::Time::Now() - start_time_;
    UMA_HISTOGRAM_LONG_TIMES("LocalDiscovery.DetectorRestartTime", time_delta);
  }
  start_time_ = base::Time::Now();

  network::mojom::UDPSocketReceiverPtr receiver_ptr;
  network::mojom::UDPSocketReceiverRequest receiver_request =
      mojo::MakeRequest(&receiver_ptr);
  receiver_binding_.Bind(std::move(receiver_request));
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&CreateUDPSocketOnUIThread, profile_,
                     mojo::MakeRequest(&socket_), std::move(receiver_ptr)));

  network::mojom::UDPSocketOptionsPtr socket_options =
      network::mojom::UDPSocketOptions::New();
  socket_options->allow_address_sharing_for_multicast = true;
  socket_options->multicast_loopback_mode = false;

  socket_->Bind(
      net::GetMDnsReceiveEndPoint(net::ADDRESS_FAMILY_IPV4),
      std::move(socket_options),
      base::BindOnce(&Helper::OnBindComplete, weak_ptr_factory_.GetWeakPtr(),
                     net::GetMDnsGroupEndPoint(net::ADDRESS_FAMILY_IPV4)));
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
  const char* substring_end = substring_begin +
                              arraysize(kPrivetDeviceTypeDnsString) - 1;
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
  receiver_binding_.Close();
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
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             on_traffic_detected_);
    base::TimeDelta time_delta = base::Time::Now() - start_time_;
    UMA_HISTOGRAM_LONG_TIMES("LocalDiscovery.DetectorTriggerTime", time_delta);
  } else {
    socket_->ReceiveMoreWithBufferSize(1, net::dns_protocol::kMaxMulticastSize);
  }
}

}  // namespace cloud_print
