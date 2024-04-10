// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_TRANSFER_PROFILER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_TRANSFER_PROFILER_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"

struct SenderData {
  std::optional<base::TimeTicks> discovered_time = std::nullopt;
  std::optional<base::TimeTicks> endpoint_decoded_time = std::nullopt;
  std::optional<base::TimeTicks> share_target_selected_time = std::nullopt;
  std::optional<base::TimeTicks> connection_established_time = std::nullopt;
  std::optional<base::TimeTicks> introduction_sent_time = std::nullopt;
  std::optional<base::TimeTicks> send_start_time = std::nullopt;
  std::optional<base::TimeTicks> bandwidth_upgrade_time = std::nullopt;
  std::optional<nearby::connections::mojom::Medium> upgraded_medium =
      std::nullopt;
};

struct ReceiverData {
  bool is_high_visibility = false;
  std::optional<base::TimeTicks> endpoint_decoded_time = std::nullopt;
  std::optional<base::TimeTicks> paired_key_handshake_time = std::nullopt;
  std::optional<base::TimeTicks> introduction_received_time = std::nullopt;
  std::optional<base::TimeTicks> accept_transfer_time = std::nullopt;
  std::optional<base::TimeTicks> bandwidth_upgrade_time = std::nullopt;
  std::optional<nearby::connections::mojom::Medium> upgraded_medium =
      std::nullopt;
};

class NearbyShareTransferProfiler {
 public:
  NearbyShareTransferProfiler();
  NearbyShareTransferProfiler(const NearbyShareTransferProfiler&) = delete;
  ~NearbyShareTransferProfiler();

  // Sender events.
  void OnEndpointDiscovered(const std::string& endpoint_id);
  void OnEndpointLost(const std::string& endpoint_id);
  void OnOutgoingEndpointDecoded(const std::string& endpoint_id);
  void OnShareTargetSelected(const std::string& endpoint_id);
  void OnConnectionEstablished(const std::string& endpoint_id);
  void OnIntroductionFrameSent(const std::string& endpoint_id);
  void OnSendStart(const std::string& endpoint_id);
  void OnSendComplete(const std::string& endpoint_id,
                      TransferMetadata::Status status);

  // Receiver events.
  void OnIncomingEndpointDecoded(const std::string& endpoint_id,
                                 bool is_high_visibility);
  void OnPairedKeyHandshakeComplete(const std::string& endpoint_id);
  void OnIntroductionFrameReceived(const std::string& endpoint_id);
  void OnTransferAccepted(const std::string& endpoint_id);
  void OnReceiveComplete(const std::string& endpoint_id,
                         TransferMetadata::Status status);

  // Events emitted by both senders and receivers.
  void OnBandwidthUpgrade(const std::string& endpoint_id,
                          nearby::connections::mojom::Medium medium);

 private:
  // Maps of endpoint ids to their associated transfer data.
  base::flat_map<std::string, SenderData> sender_data_;
  base::flat_map<std::string, ReceiverData> receiver_data_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_TRANSFER_PROFILER_H_
