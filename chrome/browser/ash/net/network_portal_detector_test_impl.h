// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_
#define CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"

namespace ash {

class NetworkPortalDetectorTestImpl : public NetworkPortalDetector {
 public:
  NetworkPortalDetectorTestImpl();

  NetworkPortalDetectorTestImpl(const NetworkPortalDetectorTestImpl&) = delete;
  NetworkPortalDetectorTestImpl& operator=(
      const NetworkPortalDetectorTestImpl&) = delete;

  ~NetworkPortalDetectorTestImpl() override;

  void SetDefaultNetworkForTesting(const std::string& guid);
  void SetDetectionResultsForTesting(const std::string& guid,
                                     CaptivePortalStatus status,
                                     int response_code);
  void NotifyObserversForTesting();

  // Returns the GUID of the network the detector considers to be default.
  std::string GetDefaultNetworkGuid() const;

  // Registers a callback that will be run when portal detection is requested by
  // StartPortalDetection().
  void RegisterPortalDetectionStartCallback(base::OnceClosure callback);

  // NetworkPortalDetector implementation:
  void AddObserver(Observer* observer) override;
  void AddAndFireObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  CaptivePortalStatus GetCaptivePortalStatus() override;
  bool IsEnabled() override;
  void Enable(bool start_detection) override;
  void StartPortalDetection() override;
  void SetStrategy(PortalDetectorStrategy::StrategyId id) override;

  PortalDetectorStrategy::StrategyId strategy_id() const {
    return strategy_id_;
  }

  bool portal_detection_in_progress() const {
    return portal_detection_in_progress_;
  }

 private:
  base::ObserverList<Observer>::Unchecked observers_;
  std::unique_ptr<NetworkState> default_network_;
  std::map<std::string, CaptivePortalStatus> portal_status_map_;
  PortalDetectorStrategy::StrategyId strategy_id_;

  // Set when StartPortalDetection() is called - it will be reset when observers
  // are notified using NotifyObserversForTesting().
  bool portal_detection_in_progress_ = false;

  std::vector<base::OnceClosure> start_detection_callbacks_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace chromeos {
using ::ash::NetworkPortalDetectorTestImpl;
}

#endif  // CHROME_BROWSER_ASH_NET_NETWORK_PORTAL_DETECTOR_TEST_IMPL_H_
