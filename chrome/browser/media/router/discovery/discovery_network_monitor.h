// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_H_

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/router/discovery/discovery_network_info.h"
#include "net/base/ip_address.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace media_router {

// Tracks the set of active network interfaces that can be used for local
// discovery.  If the list of interfaces changes, then
// DiscoveryNetworkMonitor::Observer is called with the instance of the monitor.
// Only one instance of this will be created per browser process.
//
// This class is not thread-safe, except for adding and removing observers.
// Most of the work done by the monitor is done on the IO thread, which includes
// updating the current network ID.  Therefore |GetNetworkId| should only be
// called from the IO thread.  All observers will be notified of network changes
// on the thread from which they registered.
class DiscoveryNetworkMonitor
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  using NetworkInfoFunction = std::vector<DiscoveryNetworkInfo> (*)();
  using NetworkIdCallback = base::OnceCallback<void(const std::string&)>;
  class Observer {
   public:
    // Called when the network ID has changed.  Called with the new network ID.
    virtual void OnNetworksChanged(const std::string&) = 0;

   protected:
    ~Observer() = default;
  };

  // Constants for the special states of network Id.
  // Note: The extra const stops MSVC from thinking this can't be
  // constexpr.
  static constexpr char const kNetworkIdDisconnected[] = "__disconnected__";
  static constexpr char const kNetworkIdUnknown[] = "__unknown__";

  static DiscoveryNetworkMonitor* GetInstance();

  static std::unique_ptr<DiscoveryNetworkMonitor> CreateInstanceForTest(
      NetworkInfoFunction strategy);

  DiscoveryNetworkMonitor(const DiscoveryNetworkMonitor&) = delete;
  DiscoveryNetworkMonitor& operator=(const DiscoveryNetworkMonitor&) = delete;

  void AddObserver(Observer* const observer);
  void RemoveObserver(Observer* const observer);

  // Forces a query of the current network state.  |callback| will be called
  // after the refresh.  This can be called from any thread and |callback| will
  // be executed on the calling thread with the updated network ID value.
  void Refresh(NetworkIdCallback callback);

  // Gets the current value of |netework_id_| without querying for the current
  // network state.  This should only return a stale result if no network
  // notifications have happened yet *and* Refresh hasn't been called yet.
  void GetNetworkId(NetworkIdCallback callback);

 private:
  friend class CastMediaSinkServiceImplTest;
  friend class DiscoveryNetworkMonitorTest;
  friend class AccessCodeCastSinkServiceTest;
  friend struct std::default_delete<DiscoveryNetworkMonitor>;
  friend struct base::LazyInstanceTraitsBase<DiscoveryNetworkMonitor>;

  DiscoveryNetworkMonitor();
  explicit DiscoveryNetworkMonitor(NetworkInfoFunction strategy);
  ~DiscoveryNetworkMonitor() override;

  void SetNetworkInfoFunctionForTest(NetworkInfoFunction strategy);

  // network::NetworkConnectionTracker::NetworkConnectionObserver overrides.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  std::string GetNetworkIdOnSequence() const;
  std::string UpdateNetworkInfo();

  // A hashed representation of the set of networks to which we are connected.
  // This may also be |kNetworkIdDisconnected| if no interfaces are connected or
  // |kNetworkIdUnknown| if we can't determine the set of networks.  This should
  // only be accessed from |task_runner_|'s sequence.
  std::string network_id_;

  // The list of observers which have registered interest in when |network_id_|
  // changes.
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;

  // The SequencedTaskRunner which controls access to |network_id_| and allows
  // blocking IO for network information queries.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Function used to get information about the networks to which we are
  // connected.
  NetworkInfoFunction network_info_function_;

  // SequenceChecker for |task_runner_|.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DISCOVERY_NETWORK_MONITOR_H_
