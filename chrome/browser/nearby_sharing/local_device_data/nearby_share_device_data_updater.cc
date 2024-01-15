// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/check.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_device_data_updater.h"

NearbyShareDeviceDataUpdater::Request::Request(
    std::optional<std::vector<nearby::sharing::proto::Contact>> contacts,
    std::optional<std::vector<nearby::sharing::proto::PublicCertificate>>
        certificates,
    ResultCallback callback)
    : contacts(std::move(contacts)),
      certificates(std::move(certificates)),
      callback(std::move(callback)) {}

NearbyShareDeviceDataUpdater::Request::Request(
    NearbyShareDeviceDataUpdater::Request&& request) = default;

NearbyShareDeviceDataUpdater::Request&
NearbyShareDeviceDataUpdater::Request::operator=(
    NearbyShareDeviceDataUpdater::Request&& request) = default;

NearbyShareDeviceDataUpdater::Request::~Request() = default;

NearbyShareDeviceDataUpdater::NearbyShareDeviceDataUpdater(
    const std::string& device_id)
    : device_id_(device_id) {}

NearbyShareDeviceDataUpdater::~NearbyShareDeviceDataUpdater() = default;

void NearbyShareDeviceDataUpdater::UpdateDeviceData(
    std::optional<std::vector<nearby::sharing::proto::Contact>> contacts,
    std::optional<std::vector<nearby::sharing::proto::PublicCertificate>>
        certificates,
    ResultCallback callback) {
  pending_requests_.emplace(std::move(contacts), std::move(certificates),
                            std::move(callback));
  ProcessRequestQueue();
}

void NearbyShareDeviceDataUpdater::ProcessRequestQueue() {
  if (is_request_in_progress_ || pending_requests_.empty())
    return;

  is_request_in_progress_ = true;
  HandleNextRequest();
}

void NearbyShareDeviceDataUpdater::FinishAttempt(
    const std::optional<nearby::sharing::proto::UpdateDeviceResponse>&
        response) {
  DCHECK(is_request_in_progress_);
  DCHECK(!pending_requests_.empty());

  Request current_request = std::move(pending_requests_.front());
  pending_requests_.pop();

  std::move(current_request.callback).Run(response);

  is_request_in_progress_ = false;
  ProcessRequestQueue();
}
