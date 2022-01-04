// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "net/base/ip_address.h"
#include "net/socket/udp_socket.h"

namespace net {
class IPEndPoint;
class StringIOBuffer;
class NetLog;
}  // namespace net

namespace media_router {

class DialDeviceData;

// DialService accepts requests to discover devices, sends multiple SSDP
// M-SEARCH requests via UDP multicast, and notifies observers when a
// DIAL-compliant device responds.
//
// The syntax of the M-SEARCH request and response is defined by Section 1.3
// of the uPnP device architecture specification and related documents:
//
// http://upnp.org/specs/arch/UPnP-arch-DeviceArchitecture-v1.1.pdf
//
// Each time Discover() is called, kDialNumRequests M-SEARCH requests are sent
// (with a delay of kDialRequestIntervalMillis in between):
//
// Time    Action
// ----    ------
// T1      Request 1 sent, OnDiscoveryReqest() called
// ...
// Tk      Request kDialNumRequests sent, OnDiscoveryReqest() called
// Tf      OnDiscoveryFinished() called
//
// Any time a valid response is received between T1 and Tf, it is parsed and
// OnDeviceDiscovered() is called with the result.  Tf is set to Tk +
// kDialResponseTimeoutSecs (the response timeout passed in each request).
//
// Calling Discover() again between T1 and Tf has no effect.
//
// All relevant constants are defined in dial_service.cc.
class DialService {
 public:
  enum DialServiceErrorCode {
    DIAL_SERVICE_NO_INTERFACES = 0,
    DIAL_SERVICE_SOCKET_ERROR
  };

  class Client {
   public:
    // Called when a single discovery request was sent.
    virtual void OnDiscoveryRequest() = 0;

    // Called when a device responds to a request.
    virtual void OnDeviceDiscovered(const DialDeviceData& device) = 0;

    // Called when we have all responses from the last discovery request.
    virtual void OnDiscoveryFinished() = 0;

    // Called when an error occurs.
    virtual void OnError(DialServiceErrorCode code) = 0;

   protected:
    virtual ~Client() = default;
  };

  virtual ~DialService() {}

  // Starts a new round of discovery.  Returns |true| if discovery was started
  // successfully or there is already one active. Returns |false| on error.
  virtual bool Discover() = 0;
};

// Implements DialService.
//
// NOTE(mfoltz): It would make this class cleaner to refactor most of the state
// associated with a single discovery cycle into its own |DiscoveryOperation|
// object.  This would also simplify lifetime of the object w.r.t. DialRegistry;
// the Registry would not need to create/destroy the Service on demand.
// DialServiceImpl lives on the IO thread.
class DialServiceImpl : public DialService {
 public:
  DialServiceImpl(DialService::Client& client, net::NetLog* net_log);

  DialServiceImpl(const DialServiceImpl&) = delete;
  DialServiceImpl(DialServiceImpl&&) = delete;
  DialServiceImpl& operator=(const DialServiceImpl&) = delete;
  DialServiceImpl& operator=(DialServiceImpl&&) = delete;

  ~DialServiceImpl() override;

  // DialService implementation
  bool Discover() override;

 private:
  friend void PostSendNetworkList(
      base::WeakPtr<DialServiceImpl> impl,
      const absl::optional<net::NetworkInterfaceList>& networks);

  // Represents a socket binding to a single network interface.
  // DialSocket lives on the IO thread.
  class DialSocket {
   public:
    explicit DialSocket(DialServiceImpl* dial_service);

    DialSocket(const DialSocket&) = delete;
    DialSocket& operator=(const DialSocket&) = delete;

    ~DialSocket();

    // Creates a socket using |net_log| and binds it to |bind_ip_address|.
    bool CreateAndBindSocket(const net::IPAddress& bind_ip_address,
                             net::NetLog* net_log);

    // Sends a single discovery request |send_buffer| to |send_address|
    // over the socket.
    void SendOneRequest(const net::IPEndPoint& send_address,
                        const scoped_refptr<net::StringIOBuffer>& send_buffer);

    // Returns true if the socket is closed.
    bool IsClosed();

   private:
    FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestNotifyOnError);
    FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestOnDeviceDiscovered);
    FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestOnDiscoveryRequest);
    FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestResponseParsing);

    // Checks the result of a socket operation.  The name of the socket
    // operation is given by |operation| and the result of the operation is
    // given by |result|. If the result is an error, closes the socket,
    // calls |on_error_cb_|, and returns |false|.  Returns
    // |true| otherwise. |operation| and |result| are logged.
    bool CheckResult(const char* operation, int result);

    // Closes the socket.
    void Close();

    // Callback invoked for socket writes.
    void OnSocketWrite(int buffer_size, int result);

    // Establishes the callback to read from the socket.  Returns true if
    // successful.
    bool ReadSocket();

    // Callback invoked for socket reads.
    void OnSocketRead(int result);

    // Callback invoked for socket reads.
    void HandleResponse(int bytes_read);

    // Parses a response into a DialDeviceData object. If the DIAL response is
    // invalid or does not contain enough information, then the return
    // value will be false and |device| is not changed.
    bool ParseResponse(const std::string& response,
                       const base::Time& response_time,
                       DialDeviceData* device);

    // The UDP socket.
    std::unique_ptr<net::UDPSocket> socket_;

    // Buffer for socket reads.
    scoped_refptr<net::IOBufferWithSize> recv_buffer_;

    // The source of of the last socket read.
    net::IPEndPoint recv_address_;

    // Marks whether there is an active write callback.
    bool is_writing_;

    // Marks whether there is an active read callback.
    bool is_reading_;

    // Pointer to the DialServiceImpl that owns this socket.
    const raw_ptr<DialServiceImpl> dial_service_;
  };

  // Starts the control flow for one discovery cycle.
  void StartDiscovery();

  // For each network interface in |list|, finds all unqiue IPv4 network
  // interfaces and call |DiscoverOnAddresses()| with their IP addresses.
  void SendNetworkList(const absl::optional<net::NetworkInterfaceList>& list);

  // Calls |BindAndAddSocket()| for each address in |ip_addresses|, calls
  // |SendOneRequest()|, and start the timer to finish discovery if needed.
  // The (Address family, interface index) of each address in |ip_addresses|
  // must be unique. If |ip_address| is empty, calls |FinishDiscovery()|.
  void DiscoverOnAddresses(const net::IPAddressList& ip_addresses);

  // Creates a DialSocket, binds it to |bind_ip_address| and if
  // successful, add the DialSocket to |dial_sockets_|.
  void BindAndAddSocket(const net::IPAddress& bind_ip_address);

  // Creates a DialSocket with callbacks to this object.
  std::unique_ptr<DialSocket> CreateDialSocket();

  // Sends a single discovery request to every socket that are currently open.
  void SendOneRequest();

  // Notify observers that a discovery request was made.
  void NotifyOnDiscoveryRequest();

  // Notify observers a device has been discovered.
  void NotifyOnDeviceDiscovered(const DialDeviceData& device_data);

  // Notify observers that there has been an error with one of the DialSockets.
  void NotifyOnError();

  // Called from finish_timer_ when we are done with the current round of
  // discovery.
  void FinishDiscovery();

  // Returns |true| if there are open sockets.
  bool HasOpenSockets();

  // Unowned reference to the DialService::Client.
  DialService::Client& client_;

  // DialSockets for each network interface whose ip address was
  // successfully bound.
  std::vector<std::unique_ptr<DialSocket>> dial_sockets_;

  // The NetLog for this service.
  const raw_ptr<net::NetLog> net_log_;

  // The multicast address:port for search requests.
  net::IPEndPoint send_address_;

  // Buffer for socket writes.
  scoped_refptr<net::StringIOBuffer> send_buffer_;

  // True when we are currently doing discovery.
  bool discovery_active_;

  // The number of requests that have been sent in the current discovery.
  int num_requests_sent_;

  // The maximum number of requests to send per discovery cycle.
  int max_requests_;

  // Timer for finishing discovery.
  base::OneShotTimer finish_timer_;

  // The delay for |finish_timer_|; how long to wait for discovery to finish.
  // Setting this to zero disables the timer.
  base::TimeDelta finish_delay_;

  // Timer for sending multiple requests at fixed intervals.
  base::RepeatingTimer request_timer_;

  // The delay for |request_timer_|; how long to wait between successive
  // requests.
  base::TimeDelta request_interval_;

  // WeakPtrFactory for WeakPtrs that are invalidated on IO thread.
  base::WeakPtrFactory<DialServiceImpl> weak_ptr_factory_{this};

  friend class DialServiceTest;
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestSendMultipleRequests);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestMultipleNetworkInterfaces);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestNotifyOnError);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestOnDeviceDiscovered);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestOnDiscoveryFinished);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestOnDiscoveryRequest);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, TestResponseParsing);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_H_
