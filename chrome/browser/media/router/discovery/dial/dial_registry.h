// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_REGISTRY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_REGISTRY_H_

#include <stddef.h>

#include <map>
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
#include "services/network/public/cpp/network_connection_tracker.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

namespace net {
class NetLog;
}

namespace media_router {

// Keeps track of devices that have responded to discovery requests and notifies
// the client with the current device list. It is indirectly owned by a
// singleton that is never freed. All APIs should be called on the sequence
// bound to |task_runner_|.
class DialRegistry
    : public DialService::Client,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  using DeviceList = std::vector<DialDeviceData>;

  enum DialErrorCode {
    DIAL_NO_LISTENERS = 0,  // Deprecated
    DIAL_NO_INTERFACES,
    DIAL_NETWORK_DISCONNECTED,
    DIAL_CELLULAR_NETWORK,
    DIAL_SOCKET_ERROR,
    DIAL_UNKNOWN
  };

  class Client {
   public:
    // Called when the list of DIAL devices has changed.  Will be called
    // multiple times.
    virtual void OnDialDeviceList(const DeviceList& devices) = 0;
    // Called when an error has occurred.
    virtual void OnDialError(DialErrorCode type) = 0;

   protected:
    virtual ~Client() = default;
  };

  DialRegistry(DialRegistry::Client& client,
               const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  DialRegistry(const DialRegistry&) = delete;
  DialRegistry(DialRegistry&&) = delete;
  DialRegistry& operator=(const DialRegistry&) = delete;
  DialRegistry& operator=(DialRegistry&&) = delete;
  ~DialRegistry() override;

  // Sets the NetLog object used for logging. If the registry already has a
  // NetLog, does nothing. The NetLog should live at least as long as the IO
  // Thread.
  void SetNetLog(net::NetLog* net_log);

  // Waits for a suitable network interface to be up and then starts periodic
  // discovery.  Must be called before DiscoverNow() or
  // StartPeriodicDiscovery().
  void Start();

  // Starts a discovery cycle immediately.
  bool DiscoverNow();

  // Starts and stops periodic background discovery.
  void StartPeriodicDiscovery();
  void StopPeriodicDiscovery();

  // Returns the URL of the device description for the device identified by
  // |label|, or an empty GURL if no such device exists.
  GURL GetDeviceDescriptionURL(const std::string& label) const;

  // Adds a device directly to the registry as if it was discovered.  For tests
  // only.  Note that if discovery is actually started, this device will be
  // removed by PruneExpiredDevices().
  void AddDeviceForTest(const DialDeviceData& device_data);

  // Allows tests to swap in a fake clock.
  void SetClockForTest(base::Clock* clock);

 protected:
  // Returns a new instance of the DIAL service.  Overridden by tests.
  virtual std::unique_ptr<DialService> CreateDialService();
  virtual void ClearDialService();

  // The DIAL service. Periodic discovery is active when this is not NULL.
  std::unique_ptr<DialService> dial_;

 private:
  using DeviceByIdMap = std::map<std::string, std::unique_ptr<DialDeviceData>>;
  using DeviceByLabelMap =
      std::map<std::string, raw_ptr<DialDeviceData, CtnExperimental>>;

  friend class MockDialRegistry;
  friend class TestDialRegistry;

  // Called when we've gotten the NetworkConnectionTracker from the UI thread.
  void SetNetworkConnectionTracker(network::NetworkConnectionTracker* tracker);

  // DialService::Client:
  void OnDiscoveryRequest() override;
  void OnDeviceDiscovered(const DialDeviceData& device) override;
  void OnDiscoveryFinished() override;
  void OnError(DialService::DialServiceErrorCode code) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Check whether we are in a state ready to discover and dispatch error
  // notifications if not.
  bool ReadyToDiscover();

  // Purge our whole registry. We may need to do this occasionally, e.g. when
  // the network status changes.  Increments the registry generation.
  void Clear();

  // The repeating timer schedules discoveries with this method.
  void DoDiscovery();

  // Attempts to add a newly discovered device to the registry.  Returns true if
  // successful.
  bool MaybeAddDevice(std::unique_ptr<DialDeviceData> device_data);

  // Remove devices from the registry that have expired, i.e. not responded
  // after some time.  Returns true if the registry was modified.
  bool PruneExpiredDevices();

  // Returns true if the device has expired and should be removed from the
  // active set.
  bool IsDeviceExpired(const DialDeviceData& device) const;

  // Notify the client with the current device list if the list has changed.
  void MaybeSendDeviceList();

  // Returns the next label to use for a newly-seen device.
  std::string NextLabel();

  // Unowned reference to the DialRegistry::Client.
  const raw_ref<Client> client_;

  // Task runner for the DialRegistry.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Incremented each time we modify the registry of active devices.
  int registry_generation_;

  // The registry generation associated with the last time we sent an event.
  // Used to suppress events with duplicate device lists.
  int last_event_registry_generation_;

  // Counter to generate device labels.
  int label_count_;

  // Registry parameters
  const base::TimeDelta refresh_interval_delta_;
  const base::TimeDelta expiration_delta_;
  const size_t max_devices_;

  // A map used to track known devices by their device_id.
  DeviceByIdMap device_by_id_map_;

  // A map used to track known devices sorted by label.  We iterate over this to
  // construct the device list sent to API clients.
  DeviceByLabelMap device_by_label_map_;

  // Timer used to manage periodic discovery requests.
  std::unique_ptr<base::RepeatingTimer> repeating_timer_;

  // Set just after construction.
  raw_ptr<net::NetLog> net_log_ = nullptr;

  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_ =
      nullptr;

  raw_ptr<base::Clock> clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  friend class DialMediaSinkServiceImplTest;
  friend class DialRegistryTest;
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestAddRemoveListeners);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestNoDevicesDiscovered);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestDevicesDiscovered);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest,
                           TestDevicesDiscoveredWithTwoListeners);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestDeviceExpires);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestExpiredDeviceIsRediscovered);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest,
                           TestRemovingListenerDoesNotClearList);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestNetworkEventConnectionLost);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest,
                           TestNetworkEventConnectionRestored);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_REGISTRY_H_
