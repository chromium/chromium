// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_service_impl.h"

#include <stdint.h>

#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

using base::Time;
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void PostSendNetworkList(
    base::WeakPtr<DialServiceImpl> impl,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const std::optional<net::NetworkInterfaceList>& networks) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DialServiceImpl::SendNetworkList,
                                       std::move(impl), networks));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
constexpr std::string_view kSsdpLocationHeader = "LOCATION";
constexpr std::string_view kSsdpCacheControlHeader = "CACHE-CONTROL";
constexpr std::string_view kSsdpConfigIdHeader = "CONFIGID.UPNP.ORG";
constexpr std::string_view kSsdpUsnHeader = "USN";
constexpr std::string_view kSsdpMaxAgeDirective = "max-age";
constexpr int kSsdpMaxMaxAge = 3600;
constexpr int kSsdpMaxConfigId = (2 << 24) - 1;

// The receive buffer size, in bytes.
const int kDialRecvBufferSize = 1500;

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
      kDialSearchType, version_info::GetProductName().data(),
      version_info::GetVersionNumber().data(),
      version_info::GetOSType().data()));
  // 1500 is a good MTU value for most Ethernet LANs.
  DCHECK_LE(request.size(), 1500U);
  return request;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Finds the IP address of the preferred interface of network type |type|
// to bind the socket and inserts the address into |bind_address_list|. This
// ChromeOS version can prioritize wifi and ethernet interfaces.
void InsertBestBindAddressChromeOS(const ash::NetworkTypePattern& type,
                                   net::IPAddressList* bind_address_list) {
  const ash::NetworkState* state = ash::NetworkHandler::Get()
                                       ->network_state_handler()
                                       ->ConnectedNetworkByType(type);
  if (!state)
    return;
  std::string state_ip_address = state->GetIpAddress();
  IPAddress bind_ip_address;
  if (bind_ip_address.AssignFromIPLiteral(state_ip_address) &&
      bind_ip_address.IsIPv4()) {
    bind_address_list->push_back(bind_ip_address);
  }
}

net::IPAddressList GetBestBindAddressOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  net::IPAddressList bind_address_list;
  if (ash::NetworkHandler::IsInitialized()) {
    InsertBestBindAddressChromeOS(ash::NetworkTypePattern::Ethernet(),
                                  &bind_address_list);
    InsertBestBindAddressChromeOS(ash::NetworkTypePattern::WiFi(),
                                  &bind_address_list);
  }
  return bind_address_list;
}
#else
// This function and PostSendNetworkList together handle DialServiceImpl's use
// of the network service, while keeping all of DialServiceImpl running on the
// sequence associated with task_runner_ (currently the IO thread).
// DialServiceImpl has a legacy threading model, where it was designed to be
// called from the UI thread and run on a different thread.  Although a WeakPtr
// is desired for safety when posting tasks, they are not thread/sequence-safe.
// DialServiceImpl's simple use of the network service, however, doesn't
// actually require that any of its state be accessed on the UI thread.
// Therefore, the UI thread functions can be free functions which just
// pass-through an thread WeakPtr which will be used when passing the network
// service result back to the calling thread.  This model will change when the
// network service is fully launched and this code is updated.
void GetNetworkListOnUIThread(
    base::WeakPtr<DialServiceImpl> impl,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(&PostSendNetworkList, std::move(impl), task_runner));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

DialServiceImpl::DialSocket::DialSocket(DialServiceImpl* dial_service)
    : is_writing_(false), is_reading_(false), dial_service_(dial_service) {
  DCHECK(dial_service_);
}

DialServiceImpl::DialSocket::~DialSocket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool DialServiceImpl::DialSocket::CreateAndBindSocket(
    const IPAddress& bind_ip_address,
    net::NetLog* net_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
    return;
  }

  if (is_writing_) {
    return;
  }

  is_writing_ = true;
  int result = socket_->SendTo(
      send_buffer.get(), send_buffer->size(), send_address,
      base::BindOnce(&DialServiceImpl::DialSocket::OnSocketWrite,
                     base::Unretained(this), send_buffer->size()));
  bool result_ok = CheckResult("SendTo", result);
  if (result_ok && result > 0) {
    // Synchronous write.
    OnSocketWrite(send_buffer->size(), result);
  }
}

bool DialServiceImpl::DialSocket::IsClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !socket_;
}

bool DialServiceImpl::DialSocket::CheckResult(const char* operation,
                                              int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result < net::OK && result != net::ERR_IO_PENDING) {
    Close();
    std::string error_str(net::ErrorToString(result));
    dial_service_->NotifyOnError();
    return false;
  }
  return true;
}

void DialServiceImpl::DialSocket::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_reading_ = false;
  is_writing_ = false;
  socket_.reset();
}

void DialServiceImpl::DialSocket::OnSocketWrite(int send_buffer_size,
                                                int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_writing_ = false;
  if (!CheckResult("OnSocketWrite", result))
    return;
  dial_service_->NotifyOnDiscoveryRequest();
}

bool DialServiceImpl::DialSocket::ReadSocket() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_)
    return false;

  if (is_reading_)
    return false;

  int result = net::OK;
  bool result_ok = true;
  do {
    is_reading_ = true;
    result = socket_->RecvFrom(
        recv_buffer_.get(), kDialRecvBufferSize, &recv_address_,
        base::BindOnce(&DialServiceImpl::DialSocket::OnSocketRead,
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_reading_ = false;
  if (!CheckResult("OnSocketRead", result))
    return;
  if (result > 0)
    HandleResponse(result);

  // Await next response.
  ReadSocket();
}

void DialServiceImpl::DialSocket::HandleResponse(int bytes_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(bytes_read, 0);
  if (bytes_read > kDialRecvBufferSize) {
    return;
  }

  std::string response(recv_buffer_->data(), bytes_read);
  Time response_time = Time::Now();

  // Attempt to parse response, notify client if successful.
  DialDeviceData parsed_device;
  if (ParseResponse(response, response_time, &parsed_device))
    dial_service_->NotifyOnDeviceDiscovered(parsed_device);
}

bool DialServiceImpl::DialSocket::ParseResponse(const std::string& response,
                                                const base::Time& response_time,
                                                DialDeviceData* device) {
  device->set_ip_address(recv_address_.address());
  device->set_response_time(response_time);

  size_t headers_end =
      HttpUtil::LocateEndOfHeaders(base::as_byte_span(response));
  if (headers_end == 0 || headers_end == std::string::npos) {
    return false;
  }
  std::string raw_headers = HttpUtil::AssembleRawHeaders(
      std::string_view(response.c_str(), headers_end));
  auto headers = base::MakeRefCounted<HttpResponseHeaders>(raw_headers);

  std::optional<std::string_view> device_url_str =
      headers->EnumerateHeader(/*iter=*/nullptr, kSsdpLocationHeader);
  if (!device_url_str || device_url_str->empty()) {
    return false;
  }

  GURL device_url(*device_url_str);
  if (device->IsValidUrl(device_url)) {
    device->set_device_description_url(device_url);
  } else {
    return false;
  }

  std::optional<std::string_view> device_id =
      headers->EnumerateHeader(/*iter=*/nullptr, kSsdpUsnHeader);
  if (!device_id || device_id->empty()) {
    return false;
  }
  device->set_device_id(*device_id);

  std::optional<std::string_view> cache_control =
      headers->EnumerateHeader(/*iter=*/nullptr, kSsdpCacheControlHeader);
  if (cache_control && !cache_control->empty()) {
    std::vector<std::string_view> cache_control_directives =
        base::SplitStringPiece(*cache_control, "=",
                               base::WhitespaceHandling::TRIM_WHITESPACE,
                               base::SplitResult::SPLIT_WANT_NONEMPTY);
    if (cache_control_directives.size() == 2 &&
        base::EqualsCaseInsensitiveASCII(cache_control_directives[0],
                                         kSsdpMaxAgeDirective)) {
      int max_age = 0;
      if (base::StringToInt(cache_control_directives[1], &max_age) &&
          max_age > 0) {
        device->set_max_age(std::min(max_age, kSsdpMaxMaxAge));
      }
    }
  }

  std::optional<std::string_view> config_id =
      headers->EnumerateHeader(/*iter=*/nullptr, kSsdpConfigIdHeader);
  int config_id_int;
  if (config_id && !config_id->empty() &&
      base::StringToInt(*config_id, &config_id_int) && config_id_int > 0 &&
      config_id_int <= kSsdpMaxConfigId) {
    device->set_config_id(config_id_int);
  }
  return true;
}

DialServiceImpl::DialServiceImpl(
    DialService::Client& client,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    net::NetLog* net_log)
    : client_(client),
      task_runner_(task_runner),
      net_log_(net_log),
      discovery_active_(false),
      num_requests_sent_(0),
      max_requests_(kDialMaxRequests),
      finish_delay_(base::Milliseconds((kDialMaxRequests - 1) *
                                       kDialRequestIntervalMillis) +
                    base::Seconds(kDialResponseTimeoutSecs)),
      request_interval_(base::Milliseconds(kDialRequestIntervalMillis)) {
  IPAddress address;
  bool success = address.AssignFromIPLiteral(kDialRequestAddress);
  DCHECK(success);
  send_address_ = net::IPEndPoint(address, kDialRequestPort);
  send_buffer_ = base::MakeRefCounted<StringIOBuffer>(BuildRequest());
}

DialServiceImpl::~DialServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool DialServiceImpl::Discover() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (discovery_active_) {
    return false;
  }
  discovery_active_ = true;

  StartDiscovery();
  return true;
}

void DialServiceImpl::StartDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(discovery_active_);
  if (HasOpenSockets()) {
    return;
  }

  auto ui_task_runner = content::GetUIThreadTaskRunner({});

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ui_task_runner->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetBestBindAddressOnUIThread),
      base::BindOnce(&DialServiceImpl::DiscoverOnAddresses,
                     weak_ptr_factory_.GetWeakPtr()));
#else
  ui_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&GetNetworkListOnUIThread,
                                weak_ptr_factory_.GetWeakPtr(), task_runner_));
#endif
}

void DialServiceImpl::SendNetworkList(
    const std::optional<NetworkInterfaceList>& networks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
      if (addr_family == net::ADDRESS_FAMILY_IPV4) {
        InterfaceIndexAddressFamily interface_index_addr_family =
            std::make_pair(network.interface_index, addr_family);
        bool inserted =
            interface_index_addr_family_seen.insert(interface_index_addr_family)
                .second;
        // We have not seen this interface before, so add its IP address to the
        // discovery list.
        if (inserted) {
          ip_addresses.push_back(network.address);
        }
      }
    }
  } else {
  }

  DiscoverOnAddresses(ip_addresses);
}

void DialServiceImpl::DiscoverOnAddresses(
    const net::IPAddressList& ip_addresses) {
  if (ip_addresses.empty()) {
    FinishDiscovery();
    return;
  }

  // Schedule a timer to finish the discovery process (and close the sockets).
  if (finish_delay_ > base::Seconds(0)) {
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (num_requests_sent_ == max_requests_) {
    request_timer_.Stop();
    return;
  }
  num_requests_sent_++;
  for (const auto& socket : dial_sockets_) {
    if (!socket->IsClosed())
      socket->SendOneRequest(send_address_, send_buffer_);
  }
}

void DialServiceImpl::NotifyOnDiscoveryRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If discovery is inactive, no reason to notify client.
  if (!discovery_active_) {
    return;
  }

  client_->OnDiscoveryRequest();
  // If we need to send additional requests, schedule a timer to do so.
  if (num_requests_sent_ < max_requests_ && num_requests_sent_ == 1) {
    // TODO(imcheng): Move this to SendOneRequest() once the implications are
    // understood.
    request_timer_.Start(FROM_HERE, request_interval_, this,
                         &DialServiceImpl::SendOneRequest);
  }
}

void DialServiceImpl::NotifyOnDeviceDiscovered(
    const DialDeviceData& device_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!discovery_active_) {
    return;
  }
  client_->OnDeviceDiscovered(device_data);
}

void DialServiceImpl::NotifyOnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnError(HasOpenSockets() ? DIAL_SERVICE_SOCKET_ERROR
                                    : DIAL_SERVICE_NO_INTERFACES);
}

void DialServiceImpl::FinishDiscovery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(discovery_active_);
  // Close all open sockets.
  dial_sockets_.clear();
  finish_timer_.Stop();
  request_timer_.Stop();
  discovery_active_ = false;
  num_requests_sent_ = 0;
  client_->OnDiscoveryFinished();
}

bool DialServiceImpl::HasOpenSockets() {
  for (const auto& socket : dial_sockets_) {
    if (!socket->IsClosed())
      return true;
  }
  return false;
}

}  // namespace media_router
