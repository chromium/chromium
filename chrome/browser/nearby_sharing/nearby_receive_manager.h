// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_RECEIVE_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_RECEIVE_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/transfer_update_callback.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

// |NearbyReceiveManager| is a mojo implementation that is bound in os-settings
// to allow the user to enter high-visibility advertising and accept incoming
// connections. Entering high visibility mode causes this object to register
// itself as a foreground receive surface with the |NearbySharingService| and
// listen for incoming share attempts. The client can register an observer to
// receive notifications when high visibility mode state has changed or when a
// share request has come in. The client can then accept or reject the share.
// This is a transient object and only lives while os-settings has it bound.
class NearbyReceiveManager : public nearby_share::mojom::ReceiveManager,
                             public TransferUpdateCallback,
                             public NearbySharingService::Observer {
 public:
  explicit NearbyReceiveManager(NearbySharingService* nearby_sharing_service);
  ~NearbyReceiveManager() override;

  // TransferUpdateCallback:
  void OnTransferUpdate(const ShareTarget& share_target,
                        const TransferMetadata& transfer_metadata) override;

  // nearby_share::mojom::ReceiveManager:
  void AddReceiveObserver(
      ::mojo::PendingRemote<nearby_share::mojom::ReceiveObserver> observer)
      override;
  void IsInHighVisibility(IsInHighVisibilityCallback callback) override;
  void RegisterForegroundReceiveSurface(
      RegisterForegroundReceiveSurfaceCallback callback) override;
  void UnregisterForegroundReceiveSurface(
      UnregisterForegroundReceiveSurfaceCallback callback) override;
  void Accept(const base::UnguessableToken& share_target_id,
              AcceptCallback callback) override;
  void Reject(const base::UnguessableToken& share_target_id,
              RejectCallback callback) override;
  void RecordFastInitiationNotificationUsage(bool success) override;

  // NearbySharingService::Observer
  void OnHighVisibilityChanged(bool in_high_visibility) override;
  void OnShutdown() override {}
  void OnNearbyProcessStopped() override;
  void OnStartAdvertisingFailure() override;

 private:
  void NotifyOnTransferUpdate(const ShareTarget& share_target,
                              const TransferMetadata& metadata);

  raw_ptr<NearbySharingService> nearby_sharing_service_;

  base::flat_map<base::UnguessableToken, ShareTarget> share_targets_map_;
  mojo::RemoteSet<nearby_share::mojom::ReceiveObserver> observers_set_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_RECEIVE_MANAGER_H_
