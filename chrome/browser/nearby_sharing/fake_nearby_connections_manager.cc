// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/fake_nearby_connections_manager.h"

#include "base/containers/contains.h"
#include "base/threading/thread_restrictions.h"

FakeNearbyConnectionsManager::FakeNearbyConnectionsManager() = default;

FakeNearbyConnectionsManager::~FakeNearbyConnectionsManager() = default;

void FakeNearbyConnectionsManager::Shutdown() {
  DCHECK(!IsAdvertising());
  DCHECK(!IsDiscovering());
  is_shutdown_ = true;
}

void FakeNearbyConnectionsManager::StartAdvertising(
    std::vector<uint8_t> endpoint_info,
    IncomingConnectionListener* listener,
    PowerLevel power_level,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  DCHECK(!IsAdvertising());
  is_shutdown_ = false;
  advertising_listener_ = listener;
  advertising_data_usage_ = data_usage;
  advertising_power_level_ = power_level;
  advertising_endpoint_info_ = std::move(endpoint_info);
  if (capture_next_start_advertising_callback_) {
    pending_start_advertising_callback_ = std::move(callback);
    capture_next_start_advertising_callback_ = false;
  } else {
    std::move(callback).Run(
        NearbyConnectionsManager::ConnectionsStatus::kSuccess);
  }
}

void FakeNearbyConnectionsManager::StopAdvertising(
    ConnectionsCallback callback) {
  DCHECK(IsAdvertising());
  DCHECK(!is_shutdown());
  advertising_listener_ = nullptr;
  advertising_data_usage_ = DataUsage::kUnknown;
  advertising_power_level_ = PowerLevel::kUnknown;
  advertising_endpoint_info_.reset();
  if (capture_next_stop_advertising_callback_) {
    pending_stop_advertising_callback_ = std::move(callback);
    capture_next_stop_advertising_callback_ = false;
  } else {
    std::move(callback).Run(
        NearbyConnectionsManager::ConnectionsStatus::kSuccess);
  }
}

void FakeNearbyConnectionsManager::StartDiscovery(
    DiscoveryListener* listener,
    DataUsage data_usage,
    ConnectionsCallback callback) {
  is_shutdown_ = false;
  discovery_listener_ = listener;
  std::move(callback).Run(
      NearbyConnectionsManager::ConnectionsStatus::kSuccess);
}

void FakeNearbyConnectionsManager::StopDiscovery() {
  DCHECK(IsDiscovering());
  DCHECK(!is_shutdown());
  discovery_listener_ = nullptr;
  // TODO(alexchau): Implement.
}

void FakeNearbyConnectionsManager::Connect(
    std::vector<uint8_t> endpoint_info,
    const std::string& endpoint_id,
    absl::optional<std::vector<uint8_t>> bluetooth_mac_address,
    DataUsage data_usage,
    NearbyConnectionCallback callback) {
  DCHECK(!is_shutdown());
  connected_data_usage_ = data_usage;
  connection_endpoint_infos_.emplace(endpoint_id, std::move(endpoint_info));
  std::move(callback).Run(connection_);
}

void FakeNearbyConnectionsManager::Disconnect(const std::string& endpoint_id) {
  DCHECK(!is_shutdown());
  connection_endpoint_infos_.erase(endpoint_id);
}

void FakeNearbyConnectionsManager::Send(
    const std::string& endpoint_id,
    PayloadPtr payload,
    base::WeakPtr<PayloadStatusListener> listener) {
  DCHECK(!is_shutdown());
  if (send_payload_callback_)
    send_payload_callback_.Run(std::move(payload), listener);
}

void FakeNearbyConnectionsManager::RegisterPayloadStatusListener(
    int64_t payload_id,
    base::WeakPtr<PayloadStatusListener> listener) {
  DCHECK(!is_shutdown());

  payload_status_listeners_[payload_id] = listener;
}

void FakeNearbyConnectionsManager::RegisterPayloadPath(
    int64_t payload_id,
    const base::FilePath& file_path,
    ConnectionsCallback callback) {
  DCHECK(!is_shutdown());

  registered_payload_paths_[payload_id] = file_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::File file(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                   base::File::Flags::FLAG_READ |
                                   base::File::Flags::FLAG_WRITE);
  }

  auto it = payload_path_status_.find(payload_id);
  if (it == payload_path_status_.end()) {
    std::move(callback).Run(
        location::nearby::connections::mojom::Status::kPayloadUnknown);
    return;
  }

  std::move(callback).Run(it->second);
}

FakeNearbyConnectionsManager::Payload*
FakeNearbyConnectionsManager::GetIncomingPayload(int64_t payload_id) {
  DCHECK(!is_shutdown());
  auto it = incoming_payloads_.find(payload_id);
  if (it == incoming_payloads_.end())
    return nullptr;

  return it->second.get();
}

void FakeNearbyConnectionsManager::Cancel(int64_t payload_id) {
  DCHECK(!is_shutdown());
  base::WeakPtr<PayloadStatusListener> listener =
      GetRegisteredPayloadStatusListener(payload_id);
  if (listener) {
    listener->OnStatusUpdate(
        location::nearby::connections::mojom::PayloadTransferUpdate::New(
            payload_id,
            location::nearby::connections::mojom::PayloadStatus::kCanceled,
            /*total_bytes=*/0,
            /*bytes_transferred=*/0),
        /*upgraded_medium=*/absl::nullopt);
    payload_status_listeners_.erase(payload_id);
  }

  canceled_payload_ids_.insert(payload_id);
}

void FakeNearbyConnectionsManager::ClearIncomingPayloads() {
  base::ScopedAllowBlockingForTesting allow_blocking;

  incoming_payloads_.clear();
  payload_status_listeners_.clear();
}

absl::optional<std::vector<uint8_t>>
FakeNearbyConnectionsManager::GetRawAuthenticationToken(
    const std::string& endpoint_id) {
  DCHECK(!is_shutdown());

  auto iter = endpoint_auth_tokens_.find(endpoint_id);
  if (iter != endpoint_auth_tokens_.end())
    return iter->second;

  return absl::nullopt;
}

void FakeNearbyConnectionsManager::SetRawAuthenticationToken(
    const std::string& endpoint_id,
    std::vector<uint8_t> token) {
  endpoint_auth_tokens_[endpoint_id] = std::move(token);
}

void FakeNearbyConnectionsManager::UpgradeBandwidth(
    const std::string& endpoint_id) {
  upgrade_bandwidth_endpoint_ids_.insert(endpoint_id);
}

void FakeNearbyConnectionsManager::OnEndpointFound(
    const std::string& endpoint_id,
    location::nearby::connections::mojom::DiscoveredEndpointInfoPtr info) {
  if (!discovery_listener_)
    return;

  discovery_listener_->OnEndpointDiscovered(endpoint_id, info->endpoint_info);
}

void FakeNearbyConnectionsManager::OnEndpointLost(
    const std::string& endpoint_id) {
  if (!discovery_listener_)
    return;

  discovery_listener_->OnEndpointLost(endpoint_id);
}

bool FakeNearbyConnectionsManager::IsAdvertising() const {
  return advertising_listener_ != nullptr;
}

bool FakeNearbyConnectionsManager::IsDiscovering() const {
  return discovery_listener_ != nullptr;
}

bool FakeNearbyConnectionsManager::DidUpgradeBandwidth(
    const std::string& endpoint_id) const {
  return upgrade_bandwidth_endpoint_ids_.find(endpoint_id) !=
         upgrade_bandwidth_endpoint_ids_.end();
}

void FakeNearbyConnectionsManager::SetPayloadPathStatus(
    int64_t payload_id,
    ConnectionsStatus status) {
  payload_path_status_[payload_id] = status;
}

base::WeakPtr<FakeNearbyConnectionsManager::PayloadStatusListener>
FakeNearbyConnectionsManager::GetRegisteredPayloadStatusListener(
    int64_t payload_id) {
  auto it = payload_status_listeners_.find(payload_id);
  if (it != payload_status_listeners_.end())
    return it->second;

  return nullptr;
}

void FakeNearbyConnectionsManager::SetIncomingPayload(int64_t payload_id,
                                                      PayloadPtr payload) {
  incoming_payloads_[payload_id] = std::move(payload);
}

bool FakeNearbyConnectionsManager::WasPayloadCanceled(
    const int64_t& payload_id) const {
  return base::Contains(canceled_payload_ids_, payload_id);
}

absl::optional<base::FilePath>
FakeNearbyConnectionsManager::GetRegisteredPayloadPath(int64_t payload_id) {
  auto it = registered_payload_paths_.find(payload_id);
  if (it == registered_payload_paths_.end())
    return absl::nullopt;

  return it->second;
}

void FakeNearbyConnectionsManager::CleanupForProcessStopped() {
  advertising_listener_ = nullptr;
  advertising_data_usage_ = DataUsage::kUnknown;
  advertising_power_level_ = PowerLevel::kUnknown;
  advertising_endpoint_info_.reset();

  discovery_listener_ = nullptr;

  is_shutdown_ = true;
}

FakeNearbyConnectionsManager::ConnectionsCallback
FakeNearbyConnectionsManager::GetStartAdvertisingCallback() {
  capture_next_start_advertising_callback_ = true;
  return base::BindOnce(
      &FakeNearbyConnectionsManager::HandleStartAdvertisingCallback,
      base::Unretained(this));
}

FakeNearbyConnectionsManager::ConnectionsCallback
FakeNearbyConnectionsManager::GetStopAdvertisingCallback() {
  capture_next_stop_advertising_callback_ = true;
  return base::BindOnce(
      &FakeNearbyConnectionsManager::HandleStopAdvertisingCallback,
      base::Unretained(this));
}

void FakeNearbyConnectionsManager::HandleStartAdvertisingCallback(
    ConnectionsStatus status) {
  if (pending_start_advertising_callback_) {
    std::move(pending_start_advertising_callback_).Run(status);
  }
  capture_next_start_advertising_callback_ = false;
}

void FakeNearbyConnectionsManager::HandleStopAdvertisingCallback(
    ConnectionsStatus status) {
  if (pending_stop_advertising_callback_) {
    std::move(pending_stop_advertising_callback_).Run(status);
  }
  capture_next_stop_advertising_callback_ = false;
}
