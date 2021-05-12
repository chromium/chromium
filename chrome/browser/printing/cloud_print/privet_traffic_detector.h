// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_TRAFFIC_DETECTOR_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_TRAFFIC_DETECTOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/ip_endpoint.h"
#include "net/base/network_interfaces.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/mojom/udp_socket.mojom.h"

namespace content {
class BrowserContext;
}

namespace cloud_print {

// Detects mDns traffic that looks like the "Privet" protocol. This can produce
// false positives results, but the main task of the class is to avoid running a
// full mDns listener if user doesn't have devices.
// When potential "Privet" traffic has been detected, fire a callback and stop
// listening for traffic.
// When the network changes, restarts itself to start listening for traffic
// again on the new network(s).
// The class lives on the UI thread, with a helper that lives on the IO thread.
class PrivetTrafficDetector
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Called on the UI thread.
  PrivetTrafficDetector(content::BrowserContext* profile,
                        base::RepeatingClosure on_traffic_detected);
  ~PrivetTrafficDetector() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  // Constructed by PrivetTrafficDetector on the UI thread. but lives on the IO
  // thread and destroyed on the IO thread.
  class Helper : public network::mojom::UDPSocketListener {
   public:
    Helper(content::BrowserContext* profile,
           base::RepeatingClosure on_traffic_detected);
    ~Helper() override;

    // network::mojom::UDPSocketListener:
    void OnReceived(int32_t result,
                    const base::Optional<net::IPEndPoint>& src_addr,
                    base::Optional<base::span<const uint8_t>> data) override;

    void HandleConnectionChanged(network::mojom::ConnectionType type);
    void ScheduleRestart();

   private:
    void Restart(net::NetworkInterfaceList networks);
    void Bind();
    void OnBindComplete(net::IPEndPoint multicast_addr,
                        int rv,
                        const base::Optional<net::IPEndPoint>& ip_address);
    bool IsSourceAcceptable() const;
    bool IsPrivetPacket(base::span<const uint8_t> data) const;
    void OnJoinGroupComplete(int rv);
    void ResetConnection();

    // Initialized on the UI thread, but only accessed on the IO thread for the
    // purpose of passing it back to the UI thread. Safe because it is const.
    content::BrowserContext* const profile_;

    // Initialized on the UI thread, but only accessed on the IO thread.
    base::RepeatingClosure on_traffic_detected_;
    int restart_attempts_;

    // Only accessed on the IO thread.
    net::NetworkInterfaceList networks_;
    net::IPEndPoint recv_addr_;
    mojo::Remote<network::mojom::UDPSocket> socket_;

    // Implementation of socket listener callback.
    // Initialized on the UI thread, but only accessed on the IO thread.
    mojo::Receiver<network::mojom::UDPSocketListener> listener_receiver_{this};

    base::WeakPtrFactory<Helper> weak_ptr_factory_{this};

    DISALLOW_COPY_AND_ASSIGN(Helper);
  };

  Helper* const helper_;

  DISALLOW_COPY_AND_ASSIGN(PrivetTrafficDetector);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_PRIVET_TRAFFIC_DETECTOR_H_
