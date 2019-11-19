// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_WAKE_ON_WIFI_CONNECTION_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_NET_WAKE_ON_WIFI_CONNECTION_OBSERVER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"
#include "components/gcm_driver/gcm_connection_observer.h"
#include "net/base/ip_endpoint.h"

class Profile;

namespace chromeos {

class NetworkDeviceHandler;

// Simple class that listens for a connection to the GCM server and passes the
// connection information down to shill. Each instance of this class is
// associated with a profile.
class WakeOnWifiConnectionObserver : public gcm::GCMConnectionObserver {
 public:
  WakeOnWifiConnectionObserver(Profile* profile,
                               bool wifi_properties_received,
                               WakeOnWifiManager::WakeOnWifiFeature feature,
                               NetworkDeviceHandler* network_device_handler);
  ~WakeOnWifiConnectionObserver() override;

  // Handles the case when the wifi properties have been received along with
  // attempting to begin listening for wake on wifi packets on the given IP
  // endpoint. The listening will only begin if wake on packet is enabled on the
  // device.
  void HandleWifiDevicePropertiesReady();

  // gcm::GCMConnectionObserver overrides:
  void OnConnected(const net::IPEndPoint& ip_endpoint) override;
  void OnDisconnected() override;

  void set_feature(WakeOnWifiManager::WakeOnWifiFeature feature) {
    feature_ = feature;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WakeOnWifiObserverTest, TestWakeOnWifiPacketAdd);
  FRIEND_TEST_ALL_PREFIXES(WakeOnWifiObserverTest,
                           TestWakeOnWifiPacketRemove);
  FRIEND_TEST_ALL_PREFIXES(WakeOnWifiObserverTest, TestWakeOnWifiNoneAdd);
  FRIEND_TEST_ALL_PREFIXES(WakeOnWifiObserverTest, TestWakeOnWifiNoneRemove);

  void AddWakeOnPacketConnection();
  void RemoveWakeOnPacketConnection();

  Profile* profile_;
  net::IPEndPoint ip_endpoint_;
  bool wifi_properties_received_;
  WakeOnWifiManager::WakeOnWifiFeature feature_;
  NetworkDeviceHandler* network_device_handler_;

  DISALLOW_COPY_AND_ASSIGN(WakeOnWifiConnectionObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_WAKE_ON_WIFI_CONNECTION_OBSERVER_H_
