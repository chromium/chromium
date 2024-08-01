// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_PER_SESSION_DISCOVERY_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_PER_SESSION_DISCOVERY_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/share_target_discovered_callback.h"
#include "chrome/browser/nearby_sharing/transfer_update_callback.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

// Handles a single nearby device discovery session. Holds all discovered share
// targets for the user to choose from and provides callbacks for when they are
// discovered or lost. All methods are expected to be called on the UI thread
// and there is one instance per WebUI surface.
class NearbyPerSessionDiscoveryManager
    : public TransferUpdateCallback,
      public ShareTargetDiscoveredCallback,
      public nearby_share::mojom::DiscoveryManager,
      public NearbySharingService::Observer {
 public:
  NearbyPerSessionDiscoveryManager(
      NearbySharingService* nearby_sharing_service,
      std::vector<std::unique_ptr<Attachment>> attachments);
  ~NearbyPerSessionDiscoveryManager() override;

  // TransferUpdateCallback:
  void OnTransferUpdate(const ShareTarget& share_target,
                        const TransferMetadata& transfer_metadata) override;

  // ShareTargetDiscoveredCallback:
  void OnShareTargetDiscovered(ShareTarget share_target) override;
  void OnShareTargetLost(ShareTarget share_target) override;

  // nearby_share::mojom::DiscoveryManager:
  void AddDiscoveryObserver(
      ::mojo::PendingRemote<nearby_share::mojom::DiscoveryObserver> observer)
      override;
  void StartDiscovery(
      mojo::PendingRemote<nearby_share::mojom::ShareTargetListener> listener,
      StartDiscoveryCallback callback) override;
  void StopDiscovery(base::OnceClosure callback) override;
  void SelectShareTarget(const base::UnguessableToken& share_target_id,
                         SelectShareTargetCallback callback) override;
  void GetPayloadPreview(GetPayloadPreviewCallback callback) override;

  // NearbySharingService::Observer
  void OnHighVisibilityChanged(bool in_high_visibility) override {}
  void OnShutdown() override {}
  void OnNearbyProcessStopped() override;
  void OnStartDiscoveryResult(bool success) override;

 private:
  // Used for metrics. These values are persisted to logs, and the entries are
  // ordered based on how far along they are in the discovery flow. Entries
  // should not be renumbered and numeric values should never be reused.
  enum class DiscoveryProgress {
    kDiscoveryNotAttempted = 0,
    kFailedToStartDiscovery = 1,
    kStartedDiscoveryNothingFound = 2,
    kDiscoveredShareTargetNothingSent = 3,
    kFailedToLookUpSelectedShareTarget = 4,
    kFailedToStartSend = 5,
    kStartedSend = 6,
    kMaxValue = kStartedSend
  };

  // Used for metrics. Changes |furthest_progress_| to |progress| if |progress|
  // is further along in the discovery flow than |furthest_progress_|.
  void UpdateFurthestDiscoveryProgressIfNecessary(DiscoveryProgress progress);

  bool registered_as_send_surface_ = false;
  raw_ptr<NearbySharingService> nearby_sharing_service_;
  std::vector<std::unique_ptr<Attachment>> attachments_;
  mojo::Remote<nearby_share::mojom::ShareTargetListener> share_target_listener_;
  mojo::Remote<nearby_share::mojom::TransferUpdateListener>
      transfer_update_listener_;

  // Map of ShareTarget id to discovered ShareTargets.
  base::flat_map<base::UnguessableToken, ShareTarget> discovered_share_targets_;

  // Used for metrics to track the furthest step reached in the discovery
  // session.
  DiscoveryProgress furthest_progress_ =
      DiscoveryProgress::kDiscoveryNotAttempted;

  // Used for metrics. Tracks the time when StartDiscovery() is called, or
  // std::nullopt if never called.
  std::optional<base::TimeTicks> discovery_start_time_;

  // Used for metrics. Tracks the total number devices discovered and lost in a
  // given discovery session.
  size_t num_discovered_ = 0;
  size_t num_lost_ = 0;

  mojo::RemoteSet<nearby_share::mojom::DiscoveryObserver> observers_set_;

  base::WeakPtrFactory<NearbyPerSessionDiscoveryManager> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_PER_SESSION_DISCOVERY_MANAGER_H_
