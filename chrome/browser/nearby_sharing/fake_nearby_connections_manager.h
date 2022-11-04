// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connections_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class NearbyConnection;

// Fake NearbyConnectionsManager for testing.
class FakeNearbyConnectionsManager
    : public NearbyConnectionsManager,
      public location::nearby::connections::mojom::EndpointDiscoveryListener {
 public:
  FakeNearbyConnectionsManager();
  ~FakeNearbyConnectionsManager() override;

  // NearbyConnectionsManager:
  void Shutdown() override;
  void StartAdvertising(std::vector<uint8_t> endpoint_info,
                        IncomingConnectionListener* listener,
                        PowerLevel power_level,
                        DataUsage data_usage,
                        ConnectionsCallback callback) override;
  void StopAdvertising(ConnectionsCallback callback) override;
  void StartDiscovery(DiscoveryListener* listener,
                      DataUsage data_usage,
                      ConnectionsCallback callback) override;
  void StopDiscovery() override;
  void Connect(std::vector<uint8_t> endpoint_info,
               const std::string& endpoint_id,
               absl::optional<std::vector<uint8_t>> bluetooth_mac_address,
               DataUsage data_usage,
               NearbyConnectionCallback callback) override;
  void Disconnect(const std::string& endpoint_id) override;
  void Send(const std::string& endpoint_id,
            PayloadPtr payload,
            base::WeakPtr<PayloadStatusListener> listener) override;
  void RegisterPayloadStatusListener(
      int64_t payload_id,
      base::WeakPtr<PayloadStatusListener> listener) override;
  void RegisterPayloadPath(int64_t payload_id,
                           const base::FilePath& file_path,
                           ConnectionsCallback callback) override;
  Payload* GetIncomingPayload(int64_t payload_id) override;
  void Cancel(int64_t payload_id) override;
  void ClearIncomingPayloads() override;
  absl::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      const std::string& endpoint_id) override;
  void UpgradeBandwidth(const std::string& endpoint_id) override;

  void SetRawAuthenticationToken(const std::string& endpoint_id,
                                 std::vector<uint8_t> token);

  // mojom::EndpointDiscoveryListener:
  void OnEndpointFound(
      const std::string& endpoint_id,
      location::nearby::connections::mojom::DiscoveredEndpointInfoPtr info)
      override;
  void OnEndpointLost(const std::string& endpoint_id) override;

  // Testing methods
  bool IsAdvertising() const;
  bool IsDiscovering() const;
  bool DidUpgradeBandwidth(const std::string& endpoint_id) const;
  void SetPayloadPathStatus(int64_t payload_id, ConnectionsStatus status);
  base::WeakPtr<PayloadStatusListener> GetRegisteredPayloadStatusListener(
      int64_t payload_id);
  void SetIncomingPayload(int64_t payload_id, PayloadPtr payload);
  absl::optional<base::FilePath> GetRegisteredPayloadPath(int64_t payload_id);
  bool WasPayloadCanceled(const int64_t& payload_id) const;
  void CleanupForProcessStopped();
  ConnectionsCallback GetStartAdvertisingCallback();
  ConnectionsCallback GetStopAdvertisingCallback();

  bool is_shutdown() const { return is_shutdown_; }
  DataUsage advertising_data_usage() const { return advertising_data_usage_; }
  PowerLevel advertising_power_level() const {
    return advertising_power_level_;
  }
  void set_nearby_connection(NearbyConnection* connection) {
    connection_ = connection;
  }
  DataUsage connected_data_usage() const { return connected_data_usage_; }
  void set_send_payload_callback(
      base::RepeatingCallback<
          void(PayloadPtr, base::WeakPtr<PayloadStatusListener>)> callback) {
    send_payload_callback_ = std::move(callback);
  }
  const absl::optional<std::vector<uint8_t>>& advertising_endpoint_info() {
    return advertising_endpoint_info_;
  }

  absl::optional<std::vector<uint8_t>> connection_endpoint_info(
      const std::string& endpoint_id) {
    auto it = connection_endpoint_infos_.find(endpoint_id);
    if (it == connection_endpoint_infos_.end())
      return absl::nullopt;

    return it->second;
  }

  bool has_incoming_payloads() { return !incoming_payloads_.empty(); }

 private:
  void HandleStartAdvertisingCallback(ConnectionsStatus status);
  void HandleStopAdvertisingCallback(ConnectionsStatus status);

  IncomingConnectionListener* advertising_listener_ = nullptr;
  DiscoveryListener* discovery_listener_ = nullptr;
  bool is_shutdown_ = false;
  DataUsage advertising_data_usage_ = DataUsage::kUnknown;
  PowerLevel advertising_power_level_ = PowerLevel::kUnknown;
  std::set<std::string> upgrade_bandwidth_endpoint_ids_;
  std::map<std::string, std::vector<uint8_t>> endpoint_auth_tokens_;
  NearbyConnection* connection_ = nullptr;
  DataUsage connected_data_usage_ = DataUsage::kUnknown;
  base::RepeatingCallback<void(PayloadPtr,
                               base::WeakPtr<PayloadStatusListener>)>
      send_payload_callback_;
  absl::optional<std::vector<uint8_t>> advertising_endpoint_info_;
  std::set<std::string> disconnected_endpoints_;
  std::set<int64_t> canceled_payload_ids_;
  bool capture_next_stop_advertising_callback_ = false;
  ConnectionsCallback pending_stop_advertising_callback_;
  bool capture_next_start_advertising_callback_ = false;
  ConnectionsCallback pending_start_advertising_callback_;

  // Maps endpoint_id to endpoint_info.
  std::map<std::string, std::vector<uint8_t>> connection_endpoint_infos_;

  std::map<int64_t, ConnectionsStatus> payload_path_status_;
  std::map<int64_t, base::WeakPtr<PayloadStatusListener>>
      payload_status_listeners_;
  std::map<int64_t, PayloadPtr> incoming_payloads_;
  std::map<int64_t, base::FilePath> registered_payload_paths_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_FAKE_NEARBY_CONNECTIONS_MANAGER_H_
