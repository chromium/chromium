// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_IMPL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/router/discovery/dial/dial_service.h"
#include "net/base/ip_address.h"
#include "net/base/network_interfaces.h"
#include "net/socket/udp_socket.h"

namespace net {
class IPEndPoint;
class StringIOBuffer;
class NetLog;
}  // namespace net

namespace media_router {

// Implements DialService using net::UdpSocket.
class DialServiceImpl : public DialService {
 public:
  DialServiceImpl(DialService::Client& client,
                  const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                  net::NetLog* net_log);

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
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const std::optional<net::NetworkInterfaceList>& networks);

  // Represents a socket binding to a single network interface.
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
    FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestNotifyOnError);
    FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestOnDeviceDiscovered);
    FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestOnDiscoveryRequest);
    FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestResponseParsing);

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

    SEQUENCE_CHECKER(sequence_checker_);
  };

  // Starts the control flow for one discovery cycle.
  void StartDiscovery();

  // For each network interface in |list|, finds all unique IPv4 network
  // interfaces and call |DiscoverOnAddresses()| with their IP addresses.
  void SendNetworkList(const std::optional<net::NetworkInterfaceList>& list);

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
  const raw_ref<DialService::Client> client_;

  // Task runner for the DialServiceImpl.  Currently must be bound to the IO
  // thread because of socket use, but this may change in the future.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

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

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DialServiceImpl> weak_ptr_factory_{this};

  friend class DialServiceImplTest;
  FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestSendMultipleRequests);
  FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestMultipleNetworkInterfaces);
  FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestNotifyOnError);
  FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestOnDeviceDiscovered);
  FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestOnDiscoveryFinished);
  FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestOnDiscoveryRequest);
  FRIEND_TEST_ALL_PREFIXES(DialServiceImplTest, TestResponseParsing);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_SERVICE_IMPL_H_
