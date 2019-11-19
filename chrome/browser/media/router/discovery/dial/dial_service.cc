// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_service.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/address_family.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "base/task_runner_util.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using net::HttpResponseHeaders;
using net::HttpUtil;
using net::IOBufferWithSize;
using net::IPAddress;
using net::NetworkInterface;
using net::NetworkInterfaceList;
using net::StringIOBuffer;
using net::UDPSocket;

namespace media_router {

#if !defined(OS_CHROMEOS)
void PostSendNetworkList(
    base::WeakPtr<DialServiceImpl> impl,
    const base::Optional<net::NetworkInterfaceList>& networks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&DialServiceImpl::SendNetworkList,
                                std::move(impl), networks));
}
#endif  // !defined(OS_CHROMEOS)

namespace {

// The total number of requests to make per discovery cycle.
const int kDialMaxRequests = 4;

// The interval to wait between successive requests.
const int kDialRequestIntervalMillis = 1000;

// The maximum delay a device may wait before responding (MX).
const int kDialMaxResponseDelaySecs = 1;

// The maximum time a response is expected after a M-SEARCH request.
const int kDialResponseTimeoutSecs = 2;

// The multicast IP address for discovery.
const char kDialRequestAddress[] = "239.255.255.250";

// The UDP port number for discovery.
const uint16_t kDialRequestPort = 1900;

// The DIAL service type as part of the search request.
const char kDialSearchType[] = "urn:dial-multiscreen-org:service:dial:1";

// SSDP headers parsed from the response.
const char kSsdpLocationHeader[] = "LOCATION";
const char kSsdpCacheControlHeader[] = "CACHE-CONTROL";
const char kSsdpConfigIdHeader[] = "CONFIGID.UPNP.ORG";
const char kSsdpUsnHeader[] = "USN";

// The receive buffer size, in bytes.
const int kDialRecvBufferSize = 1500;

// Gets a specific header from |headers| and puts it in |value|.
bool GetHeader(HttpResponseHeaders* headers,
               const char* name,
               std::string* value) {
  return headers->EnumerateHeader(nullptr, std::string(name), value);
}

// Returns the request string.
std::string BuildRequest() {
  // Extra line at the end to make UPnP lib happy.
  std::string request(base::StringPrintf(
      "M-SEARCH * HTTP/1.1\r\n"
      "HOST: %s:%u\r\n"
      "MAN: \"ssdp:discover\"\r\n"
      "MX: %d\r\n"
      "ST: %s\r\n"
      "USER-AGENT: %s/%s %s\r\n"
      "\r\n",
      kDialRequestAddress, kDialRequestPort, kDialMaxResponseDelaySecs,
      kDialSearchType, version_info::GetProductName().c_str(),
      version_info::GetVersionNumber().c_str(),
      version_info::GetOSType().c_str()));
  // 1500 is a good MTU value for most Ethernet LANs.
  DCHECK_LE(request.size(), 1500U);
  return request;
}

#if defined(OS_CHROMEOS)
// Finds the IP address of the preferred interface of network type |type|
// to bind the socket and inserts the address into |bind_address_list|. This
// ChromeOS version can prioritize wifi and ethernet interfaces.
void InsertBestBindAddressChromeOS(const chromeos::NetworkTypePattern& type,
                                   net::IPAddressList* bind_address_list) {
  const chromeos::NetworkState* state = chromeos::NetworkHandler::Get()
                                            ->network_state_handler()
                                            ->ConnectedNetworkByType(type);
  if (!state)
    return;
  std::string state_ip_address = state->GetIpAddress();
  IPAddress bind_ip_address;
  if (bind_ip_address.AssignFromIPLiteral(state_ip_address) &&
      bind_ip_address.IsIPv4()) {
    VLOG(2) << "Found " << state->type() << ", " << state->name() << ": "
            << state_ip_address;
    bind_address_list->push_back(bind_ip_address);
  }
}

net::IPAddressList GetBestBindAddressOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  net::IPAddressList bind_address_list;
  if (chromeos::NetworkHandler::IsInitialized()) {
    InsertBestBindAddressChromeOS(chromeos::NetworkTypePattern::Ethernet(),
                                  &bind_address_list);
    InsertBestBindAddressChromeOS(chromeos::NetworkTypePattern::WiFi(),
                                  &bind_address_list);
  } else {
    VLOG(1) << "InsertBestBindAddressChromeOSOnUIThread called with "
               "uninitialized NetworkHandler";
  }
  return bind_address_list;
}
#else
// This function and PostSendNetworkList together handle DialServiceImpl's use
// of the network service, while keeping all of DialServiceImpl running on the
// IO thread.  DialServiceImpl has a legacy threading model, where it was
// designed to be called from the UI thread and run on the IO thread.  Although
// a WeakPtr is desired for safety when posting tasks, they are not
// thread/sequence-safe.  DialServiceImpl's simple use of the network service,
// however, doesn't actually require that any of its state be accessed on the UI
// thread.  Therefore, the UI thread functions can be free functions which just
// pass-through an IO thread WeakPtr which will be used when passing the network
// service result back to the IO thread.  This model will change when the
// network service is fully launched and this code is updated.
void GetNetworkListOnUIThread(base::WeakPtr<DialServiceImpl> impl) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(&PostSendNetworkList, std::move(impl)));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

DialServiceImpl::DialSocket::DialSocket(DialServiceImpl* dial_service)
    : is_writing_(false), is_reading_(false), dial_service_(dial_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(dial_service_);
}

DialServiceImpl::DialSocket::~DialSocket() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool DialServiceImpl::DialSocket::CreateAndBindSocket(
    const IPAddress& bind_ip_address,
    net::NetLog* net_log) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!socket_);
  DCHECK(bind_ip_address.IsIPv4());

  socket_ = std::make_unique<UDPSocket>(net::DatagramSocket::RANDOM_BIND,
                                        net_log, net::NetLogSource());

  // 0 means bind a random port
  net::IPEndPoint address(bind_ip_address, 0);

  if (socket_->Open(address.GetFamily()) != net::OK ||
      socket_->SetBroadcast(true) != net::OK ||
      !CheckResult("Bind", socket_->Bind(address))) {
    socket_.reset();
    return false;
  }

  recv_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kDialRecvBufferSize);
  return ReadSocket();
}

void DialServiceImpl::DialSocket::SendOneRequest(
    const net::IPEndPoint& send_address,
    const scoped_refptr<net::StringIOBuffer>& send_buffer) {
  if (!socket_) {
    VLOG(1) << "Socket not connected.";
    return;
  }

  if (is_writing_) {
    VLOG(1) << "Already writing.";
    return;
  }

  is_writing_ = true;
  int result =
      socket_->SendTo(send_buffer.get(), send_buffer->size(), send_address,
                      base::Bind(&DialServiceImpl::DialSocket::OnSocketWrite,
                                 base::Unretained(this), send_buffer->size()));
  bool result_ok = CheckResult("SendTo", result);
  if (result_ok && result > 0) {
    // Synchronous write.
    OnSocketWrite(send_buffer->size(), result);
  }
}

bool DialServiceImpl::DialSocket::IsClosed() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return !socket_;
}

bool DialServiceImpl::DialSocket::CheckResult(const char* operation,
                                              int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(2) << "Operation " << operation << " result " << result;
  if (result < net::OK && result != net::ERR_IO_PENDING) {
    Close();
    std::string error_str(net::ErrorToString(result));
    VLOG(1) << "dial socket error: " << error_str;
    dial_service_->NotifyOnError();
    return false;
  }
  return true;
}

void DialServiceImpl::DialSocket::Close() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  is_reading_ = false;
  is_writing_ = false;
  socket_.reset();
}

void DialServiceImpl::DialSocket::OnSocketWrite(int send_buffer_size,
                                                int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  is_writing_ = false;
  if (!CheckResult("OnSocketWrite", result))
    return;
  if (result != send_buffer_size) {
    VLOG(1) << "Sent " << result << " chars, expected " << send_buffer_size
            << " chars";
  }
  dial_service_->NotifyOnDiscoveryRequest();
}

bool DialServiceImpl::DialSocket::ReadSocket() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!socket_) {
    VLOG(1) << "Socket not connected.";
    return false;
  }

  if (is_reading_) {
    VLOG(1) << "Already reading.";
    return false;
  }

  int result = net::OK;
  bool result_ok = true;
  do {
    is_reading_ = true;
    result = socket_->RecvFrom(
        recv_buffer_.get(), kDialRecvBufferSize, &recv_address_,
        base::Bind(&DialServiceImpl::DialSocket::OnSocketRead,
                   base::Unretained(this)));
    result_ok = CheckResult("RecvFrom", result);
    if (result != net::ERR_IO_PENDING)
      is_reading_ = false;
    if (result_ok && result > 0) {
      // Synchronous read.
      HandleResponse(result);
    }
  } while (result_ok && result != net::OK && result != net::ERR_IO_PENDING);
  return result_ok;
}

void DialServiceImpl::DialSocket::OnSocketRead(int result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  is_reading_ = false;
  if (!CheckResult("OnSocketRead", result))
    return;
  if (result > 0)
    HandleResponse(result);

  // Await next response.
  ReadSocket();
}

void DialServiceImpl::DialSocket::HandleResponse(int bytes_read) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(bytes_read, 0);
  if (bytes_read > kDialRecvBufferSize) {
    VLOG(1) << bytes_read << " > " << kDialRecvBufferSize << "!?";
    return;
  }
  VLOG(2) << "Read " << bytes_read << " bytes from "
          << recv_address_.ToString();

  std::string response(recv_buffer_->data(), bytes_read);
  Time response_time = Time::Now();

  // Attempt to parse response, notify observers if successful.
  DialDeviceData parsed_device;
  if (ParseResponse(response, response_time, &parsed_device))
    dial_service_->NotifyOnDeviceDiscovered(parsed_device);
}

// static
bool DialServiceImpl::DialSocket::ParseResponse(const std::string& response,
                                                const base::Time& response_time,
                                                DialDeviceData* device) {
  size_t headers_end =
      HttpUtil::LocateEndOfHeaders(response.c_str(), response.size());
  if (headers_end == 0 || headers_end == std::string::npos) {
    VLOG(1) << "Headers invalid or empty, ignoring: " << response;
    return false;
  }
  std::string raw_headers = HttpUtil::AssembleRawHeaders(
      base::StringPiece(response.c_str(), headers_end));
  VLOG(3) << "raw_headers: " << raw_headers << "\n";
  auto headers = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  std::string device_url_str;
  if (!GetHeader(headers.get(), kSsdpLocationHeader, &device_url_str) ||
      device_url_str.empty()) {
    VLOG(1) << "No LOCATION header found.";
    return false;
  }

  GURL device_url(device_url_str);
  if (!DialDeviceData::IsDeviceDescriptionUrl(device_url)) {
    VLOG(1) << "URL " << device_url_str << " not valid.";
    return false;
  }

  std::string device_id;
  if (!GetHeader(headers.get(), kSsdpUsnHeader, &device_id) ||
      device_id.empty()) {
    VLOG(1) << "No USN header found.";
    return false;
  }

  device->set_device_id(device_id);
  device->set_device_description_url(device_url);
  device->set_response_time(response_time);

  // TODO(mfoltz): Parse the max-age value from the cache control header.
  // http://crbug.com/165289
  std::string cache_control;
  GetHeader(headers.get(), kSsdpCacheControlHeader, &cache_control);

  std::string config_id;
  int config_id_int;
  if (GetHeader(headers.get(), kSsdpConfigIdHeader, &config_id) &&
      base::StringToInt(config_id, &config_id_int)) {
    device->set_config_id(config_id_int);
  } else {
    VLOG(1) << "Malformed or missing " << kSsdpConfigIdHeader << ": "
            << config_id;
  }

  return true;
}

DialServiceImpl::DialServiceImpl(net::NetLog* net_log)
    : net_log_(net_log),
      discovery_active_(false),
      num_requests_sent_(0),
      max_requests_(kDialMaxRequests),
      finish_delay_(TimeDelta::FromMilliseconds((kDialMaxRequests - 1) *
                                                kDialRequestIntervalMillis) +
                    TimeDelta::FromSeconds(kDialResponseTimeoutSecs)),
      request_interval_(
          TimeDelta::FromMilliseconds(kDialRequestIntervalMillis)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  IPAddress address;
  bool success = address.AssignFromIPLiteral(kDialRequestAddress);
  DCHECK(success);
  send_address_ = net::IPEndPoint(address, kDialRequestPort);
  send_buffer_ = base::MakeRefCounted<StringIOBuffer>(BuildRequest());
}

DialServiceImpl::~DialServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void DialServiceImpl::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observer_list_.AddObserver(observer);
}

void DialServiceImpl::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  observer_list_.RemoveObserver(observer);
}

bool DialServiceImpl::HasObserver(const Observer* observer) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return observer_list_.HasObserver(observer);
}

bool DialServiceImpl::Discover() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (discovery_active_) {
    VLOG(2) << "Discovery is already active - returning.";
    return false;
  }
  discovery_active_ = true;

  VLOG(2) << "Discovery started.";

  StartDiscovery();
  return true;
}

void DialServiceImpl::StartDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(discovery_active_);
  if (HasOpenSockets()) {
    VLOG(2) << "Calling StartDiscovery() with open sockets. Returning.";
    return;
  }

  auto task_runner = base::CreateSingleThreadTaskRunner({BrowserThread::UI});

#if defined(OS_CHROMEOS)
  task_tracker_.PostTaskAndReplyWithResult(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&GetBestBindAddressOnUIThread),
      base::BindOnce(&DialServiceImpl::DiscoverOnAddresses,
                     base::Unretained(this)));
#else
  task_tracker_.PostTask(task_runner.get(), FROM_HERE,
                         base::BindOnce(&GetNetworkListOnUIThread,
                                        weak_ptr_factory_.GetWeakPtr()));
#endif
}

void DialServiceImpl::SendNetworkList(
    const base::Optional<NetworkInterfaceList>& networks) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  using InterfaceIndexAddressFamily = std::pair<uint32_t, net::AddressFamily>;
  std::set<InterfaceIndexAddressFamily> interface_index_addr_family_seen;
  net::IPAddressList ip_addresses;

  if (networks.has_value()) {
    // Binds a socket to each IPv4 network interface found. Note that
    // there may be duplicates in |networks|, so address family + interface
    // index is used to identify unique interfaces.
    // TODO(mfoltz): Support IPV6 multicast.  http://crbug.com/165286
    for (const auto& network : *networks) {
      net::AddressFamily addr_family = net::GetAddressFamily(network.address);
      VLOG(2) << "Found " << network.name << ", " << network.address.ToString()
              << ", address family: " << addr_family;
      if (addr_family == net::ADDRESS_FAMILY_IPV4) {
        InterfaceIndexAddressFamily interface_index_addr_family =
            std::make_pair(network.interface_index, addr_family);
        bool inserted =
            interface_index_addr_family_seen.insert(interface_index_addr_family)
                .second;
        // We have not seen this interface before, so add its IP address to the
        // discovery list.
        if (inserted) {
          VLOG(2) << "Encountered "
                  << "interface index: " << network.interface_index << ", "
                  << "address family: " << addr_family
                  << " for the first time, "
                  << "adding IP address " << network.address.ToString()
                  << " to list.";
          ip_addresses.push_back(network.address);
        } else {
          VLOG(2) << "Already encountered "
                  << "interface index: " << network.interface_index << ", "
                  << "address family: " << addr_family
                  << " before, not adding.";
        }
      }
    }
  } else {
    VLOG(1) << "Could not retrieve network list!";
  }

  DiscoverOnAddresses(ip_addresses);
}

void DialServiceImpl::DiscoverOnAddresses(
    const net::IPAddressList& ip_addresses) {
  if (ip_addresses.empty()) {
    VLOG(1) << "Could not find a valid interface to bind. Finishing discovery";
    FinishDiscovery();
    return;
  }

  // Schedule a timer to finish the discovery process (and close the sockets).
  if (finish_delay_ > TimeDelta::FromSeconds(0)) {
    VLOG(2) << "Starting timer to finish discovery.";
    finish_timer_.Start(FROM_HERE, finish_delay_, this,
                        &DialServiceImpl::FinishDiscovery);
  }

  for (const auto& address : ip_addresses) {
    BindAndAddSocket(address);
  }

  SendOneRequest();
}

void DialServiceImpl::BindAndAddSocket(const IPAddress& bind_ip_address) {
  std::unique_ptr<DialServiceImpl::DialSocket> dial_socket(CreateDialSocket());
  if (dial_socket->CreateAndBindSocket(bind_ip_address, net_log_))
    dial_sockets_.push_back(std::move(dial_socket));
}

std::unique_ptr<DialServiceImpl::DialSocket>
DialServiceImpl::CreateDialSocket() {
  return std::make_unique<DialServiceImpl::DialSocket>(this);
}

void DialServiceImpl::SendOneRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (num_requests_sent_ == max_requests_) {
    VLOG(2) << "Reached max requests; stopping request timer.";
    request_timer_.Stop();
    return;
  }
  num_requests_sent_++;
  VLOG(2) << "Sending request " << num_requests_sent_ << "/" << max_requests_;
  for (const auto& socket : dial_sockets_) {
    if (!socket->IsClosed())
      socket->SendOneRequest(send_address_, send_buffer_);
  }
}

void DialServiceImpl::NotifyOnDiscoveryRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // If discovery is inactive, no reason to notify observers.
  if (!discovery_active_) {
    VLOG(2) << "Request sent after discovery finished.  Ignoring.";
    return;
  }

  VLOG(2) << "Notifying observers of discovery request";
  for (auto& observer : observer_list_)
    observer.OnDiscoveryRequest(this);
  // If we need to send additional requests, schedule a timer to do so.
  if (num_requests_sent_ < max_requests_ && num_requests_sent_ == 1) {
    VLOG(2) << "Scheduling timer to send additional requests";
    // TODO(imcheng): Move this to SendOneRequest() once the implications are
    // understood.
    request_timer_.Start(FROM_HERE, request_interval_, this,
                         &DialServiceImpl::SendOneRequest);
  }
}

void DialServiceImpl::NotifyOnDeviceDiscovered(
    const DialDeviceData& device_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!discovery_active_) {
    VLOG(2) << "Got response after discovery finished.  Ignoring.";
    return;
  }
  for (auto& observer : observer_list_)
    observer.OnDeviceDiscovered(this, device_data);
}

void DialServiceImpl::NotifyOnError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(imcheng): Modify upstream so that the device list is not cleared
  // when it could still potentially discover devices on other sockets.
  for (auto& observer : observer_list_) {
    observer.OnError(this, HasOpenSockets() ? DIAL_SERVICE_SOCKET_ERROR
                                            : DIAL_SERVICE_NO_INTERFACES);
  }
}

void DialServiceImpl::FinishDiscovery() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(discovery_active_);
  VLOG(2) << "Discovery finished.";
  // Close all open sockets.
  dial_sockets_.clear();
  finish_timer_.Stop();
  request_timer_.Stop();
  discovery_active_ = false;
  num_requests_sent_ = 0;
  for (auto& observer : observer_list_)
    observer.OnDiscoveryFinished(this);
}

bool DialServiceImpl::HasOpenSockets() {
  for (const auto& socket : dial_sockets_) {
    if (!socket->IsClosed())
      return true;
  }
  return false;
}

}  // namespace media_router
