// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"

#include "base/callback.h"
#include "base/logging.h"
#include "chromeos/network/network_state.h"

namespace chromeos {

NetworkPortalDetectorTestImpl::NetworkPortalDetectorTestImpl()
    : strategy_id_(PortalDetectorStrategy::STRATEGY_ID_LOGIN_SCREEN) {
}

NetworkPortalDetectorTestImpl::~NetworkPortalDetectorTestImpl() {
  for (auto& observer : observers_)
    observer.OnShutdown();
}

void NetworkPortalDetectorTestImpl::SetDefaultNetworkForTesting(
    const std::string& guid) {
  DVLOG(1) << "SetDefaultNetworkForTesting: " << guid;
  if (guid.empty()) {
    default_network_.reset();
  } else {
    default_network_.reset(new NetworkState("/service/" + guid));
    default_network_->SetGuid(guid);
  }
}

void NetworkPortalDetectorTestImpl::SetDetectionResultsForTesting(
    const std::string& guid,
    CaptivePortalStatus status,
    int response_code) {
  DVLOG(1) << "SetDetectionResultsForTesting: " << guid << " = "
           << NetworkPortalDetector::CaptivePortalStatusString(status);
  if (!guid.empty())
    portal_status_map_[guid] = status;
}

void NetworkPortalDetectorTestImpl::NotifyObserversForTesting() {
  const std::string& guid = default_network_->guid();
  CaptivePortalStatus status = CAPTIVE_PORTAL_STATUS_UNKNOWN;
  if (default_network_ && portal_status_map_.count(guid))
    status = portal_status_map_[guid];
  portal_detection_in_progress_ = false;
  for (auto& observer : observers_)
    observer.OnPortalDetectionCompleted(default_network_.get(), status);
}

std::string NetworkPortalDetectorTestImpl::GetDefaultNetworkGuid() const {
  if (!default_network_)
    return "";

  return default_network_->guid();
}

void NetworkPortalDetectorTestImpl::RegisterPortalDetectionStartCallback(
    base::OnceClosure callback) {
  start_detection_callbacks_.push_back(std::move(callback));
}

void NetworkPortalDetectorTestImpl::AddObserver(Observer* observer) {
  if (observer && !observers_.HasObserver(observer))
    observers_.AddObserver(observer);
}

void NetworkPortalDetectorTestImpl::AddAndFireObserver(Observer* observer) {
  AddObserver(observer);
  if (!observer)
    return;
  if (!default_network_ ||
      !portal_status_map_.count(default_network_->guid())) {
    observer->OnPortalDetectionCompleted(default_network_.get(),
                                         CAPTIVE_PORTAL_STATUS_UNKNOWN);
  } else {
    observer->OnPortalDetectionCompleted(
        default_network_.get(), portal_status_map_[default_network_->guid()]);
  }
}

void NetworkPortalDetectorTestImpl::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

NetworkPortalDetector::CaptivePortalStatus
NetworkPortalDetectorTestImpl::GetCaptivePortalStatus() {
  if (!default_network_)
    return CAPTIVE_PORTAL_STATUS_UNKNOWN;
  auto it = portal_status_map_.find(default_network_->guid());
  if (it == portal_status_map_.end())
    return CAPTIVE_PORTAL_STATUS_UNKNOWN;
  return it->second;
}

bool NetworkPortalDetectorTestImpl::IsEnabled() {
  return true;
}

void NetworkPortalDetectorTestImpl::Enable(bool start_detection) {
}

void NetworkPortalDetectorTestImpl::StartPortalDetection() {
  if (portal_detection_in_progress_)
    return;

  portal_detection_in_progress_ = true;
  std::vector<base::OnceClosure> callbacks =
      std::move(start_detection_callbacks_);
  for (auto& callback : callbacks)
    std::move(callback).Run();

  return;
}

void NetworkPortalDetectorTestImpl::SetStrategy(
    PortalDetectorStrategy::StrategyId id) {
  strategy_id_ = id;
}

}  // namespace chromeos
