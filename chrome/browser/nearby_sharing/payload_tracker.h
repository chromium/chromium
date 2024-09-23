// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_PAYLOAD_TRACKER_H_
#define CHROME_BROWSER_NEARBY_SHARING_PAYLOAD_TRACKER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/attachment_info.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"

// Listens for incoming or outgoing transfer updates from Nearby Connections and
// forwards the transfer progress to the |update_callback|.
class PayloadTracker : public NearbyConnectionsManager::PayloadStatusListener {
 public:
  PayloadTracker(
      const ShareTarget& share_target,
      const base::flat_map<int64_t, AttachmentInfo>& attachment_info_map,
      base::RepeatingCallback<void(ShareTarget, TransferMetadata)>
          update_callback);
  ~PayloadTracker() override;

  // NearbyConnectionsManager::PayloadStatusListener:
  void OnStatusUpdate(PayloadTransferUpdatePtr update,
                      std::optional<Medium> upgraded_medium) override;

 private:
  struct State {
    explicit State(int64_t total_size) : total_size(total_size) {}
    ~State() = default;

    uint64_t amount_transferred = 0;
    const uint64_t total_size;
    nearby::connections::mojom::PayloadStatus status =
        nearby::connections::mojom::PayloadStatus::kInProgress;
  };

  void OnTransferUpdate();

  bool IsComplete() const;
  bool IsCancelled() const;
  bool HasFailed() const;

  uint64_t GetTotalTransferred() const;
  double CalculateProgressPercent() const;

  void EmitFinalMetrics(nearby::connections::mojom::PayloadStatus status) const;

  ShareTarget share_target_;
  base::RepeatingCallback<void(ShareTarget, TransferMetadata)> update_callback_;

  // Map of payload id to state of payload.
  std::map<int64_t, State> payload_state_;

  uint64_t total_transfer_size_;

  int last_update_progress_ = 0;
  base::Time last_update_timestamp_;

  // For metrics.
  size_t num_text_attachments_ = 0;
  size_t num_file_attachments_ = 0;
  size_t num_wifi_credentials_attachments_ = 0;
  uint64_t num_first_update_bytes_ = 0;
  std::optional<base::TimeTicks> first_update_timestamp_;
  std::optional<Medium> last_upgraded_medium_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_PAYLOAD_TRACKER_H_
