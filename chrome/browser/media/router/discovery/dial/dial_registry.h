// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
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
// the observer with an updated, complete set of active devices.  The registry's
// observer (i.e., the Dial API) owns the registry instance.
// DialRegistry lives on the IO thread.
class DialRegistry
    : public DialService::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  using DeviceList = std::vector<DialDeviceData>;

  enum DialErrorCode {
    DIAL_NO_LISTENERS = 0,
    DIAL_NO_INTERFACES,
    DIAL_NETWORK_DISCONNECTED,
    DIAL_CELLULAR_NETWORK,
    DIAL_SOCKET_ERROR,
    DIAL_UNKNOWN
  };

  class Observer {
   public:
    // Methods invoked on the IO thread when a new device is discovered, an
    // update is triggered by dial.discoverNow or an error occured.
    virtual void OnDialDeviceEvent(const DeviceList& devices) = 0;
    virtual void OnDialError(DialErrorCode type) = 0;

   protected:
    virtual ~Observer() {}
  };

  static DialRegistry* GetInstance();

  // Sets the NetLog object used for logging. Should be called right after
  // GetInstance(). If the registry already has a NetLog, does nothing. The
  // NetLog should live at least as long as the IO Thread.
  void SetNetLog(net::NetLog* net_log);

  // Called by the DIAL API when event listeners are added or removed. The dial
  // service is started after the first listener is added and stopped after the
  // last listener is removed.
  virtual void OnListenerAdded();
  virtual void OnListenerRemoved();

  // pass a reference of |observer| to allow it to notify on DIAL device events.
  // This class does not take ownership of observer.
  virtual void RegisterObserver(Observer* observer);
  virtual void UnregisterObserver(Observer* observer);

  // Called by the DIAL API to try to kickoff a discovery if there is not one
  // already active.
  bool DiscoverNow();

  // Starts and stops periodic discovery.  Periodic discovery is done when there
  // are registered event listeners.
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
  using DeviceByLabelMap = std::map<std::string, DialDeviceData*>;

  friend class MockDialRegistry;
  friend class TestDialRegistry;
  friend struct base::DefaultSingletonTraits<DialRegistry>;

  DialRegistry();
  ~DialRegistry() override;

  // Called when we've gotten the NetworkConnectionTracker from the UI thread.
  void SetNetworkConnectionTracker(network::NetworkConnectionTracker* tracker);

  // DialService::Observer:
  void OnDiscoveryRequest(DialService* service) override;
  void OnDeviceDiscovered(DialService* service,
                          const DialDeviceData& device) override;
  void OnDiscoveryFinished(DialService* service) override;
  void OnError(DialService* service,
               const DialService::DialServiceErrorCode& code) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Notify all observers about DialDeviceEvent or DialError.
  void OnDialDeviceEvent(const DeviceList& devices);
  void OnDialError(DialErrorCode type);


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

  // Notify listeners with the current device list if the list has changed.
  void MaybeSendEvent();

  // Notify listeners with the current device list.
  void SendEvent();

  // Returns the next label to use for a newly-seen device.
  std::string NextLabel();

  // The current number of event listeners attached to this registry.
  int num_listeners_;

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

  // Timer used to manage periodic discovery requests. Timer is created and
  // destroyed on IO thread.
  std::unique_ptr<base::RepeatingTimer> repeating_timer_;

  // Interface from which the DIAL API is notified of DIAL device events. the
  // DIAL API owns this DIAL registry.
  base::ObserverList<Observer>::Unchecked observers_;

  // Set just after construction, only used on the IO thread.
  net::NetLog* net_log_ = nullptr;

  network::NetworkConnectionTracker* network_connection_tracker_ = nullptr;

  base::Clock* clock_;

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
  DISALLOW_COPY_AND_ASSIGN(DialRegistry);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_REGISTRY_H_
