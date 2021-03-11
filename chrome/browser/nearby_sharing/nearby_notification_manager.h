// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/nearby_notification_delegate.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/share_target_discovered_callback.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "chrome/browser/nearby_sharing/transfer_update_callback.h"

class NotificationDisplayService;
class PrefService;
class Profile;
class SkBitmap;

// Manages notifications shown for Nearby Share. Only a single notification will
// be shown as simultaneous connections are not supported. All methods should be
// called from the UI thread.
class NearbyNotificationManager : public TransferUpdateCallback,
                                  public ShareTargetDiscoveredCallback,
                                  public NearbySharingService::Observer {
 public:
  static constexpr base::TimeDelta kOnboardingDismissedTimeout =
      base::TimeDelta::FromMinutes(15);

  enum class SuccessNotificationAction {
    kNone,
    kCopyText,
    kCopyImage,
    kOpenDownloads,
    kOpenUrl,
  };

  // Type of content we received that determines the actions we provide.
  enum class ReceivedContentType {
    kFiles,        // One or more generic files
    kSingleImage,  // One image that will be shown as a preview
    kSingleUrl,    // One URL that will be opened on click.
    kText,         // Arbitrary text content
  };

  NearbyNotificationManager(
      NotificationDisplayService* notification_display_service,
      NearbySharingService* nearby_service,
      PrefService* pref_service,
      Profile* profile);
  ~NearbyNotificationManager() override;

  // TransferUpdateCallback:
  void OnTransferUpdate(const ShareTarget& share_target,
                        const TransferMetadata& transfer_metadata) override;

  // ShareTargetDiscoveredCallback:
  void OnShareTargetDiscovered(ShareTarget share_target) override;
  void OnShareTargetLost(ShareTarget share_target) override;

  // NearbySharingService::Observer
  void OnHighVisibilityChanged(bool in_high_visibility) override {}
  void OnNearbyProcessStopped() override;
  void OnShutdown() override {}

  // Shows a progress notification of the data being transferred to or from
  // |share_target|. Has a cancel action to cancel the transfer.
  void ShowProgress(const ShareTarget& share_target,
                    const TransferMetadata& transfer_metadata);

  // Shows an incoming connection request notification from |share_target|
  // wanting to send data to this device. Has a decline action and optionally an
  // accept action if the transfer needs to be accepted on the local device.
  void ShowConnectionRequest(const ShareTarget& share_target,
                             const TransferMetadata& transfer_metadata);

  // Shows an onboarding notification when a nearby device is attempting to
  // share. Clicking it will make the local device visible to all nearby
  // devices.
  void ShowOnboarding();

  // Shows a notification for send or receive success.
  void ShowSuccess(const ShareTarget& share_target);

  // Shows a notification for send or receive failure.
  void ShowFailure(const ShareTarget& share_target,
                   const TransferMetadata& transfer_metadata);

  // Shows a notification for send or receive cancellation.
  void ShowCancelled(const ShareTarget& share_target);

  // Closes any currently shown transfer notification (e.g. progress or
  // connection).
  void CloseTransfer();

  // Closes any currently shown onboarding notification.
  void CloseOnboarding();

  // Gets the currently registered delegate for |notification_id|.
  NearbyNotificationDelegate* GetNotificationDelegate(
      const std::string& notification_id);

  void OpenURL(GURL url);

  // Cancels the currently in progress transfer.
  void CancelTransfer();

  // Rejects the currently in progress transfer.
  void RejectTransfer();

  // Accepts the currently in progress transfer.
  void AcceptTransfer();

  // Called when the onboarding notification got clicked.
  void OnOnboardingClicked();

  // Called when the onboarding notification got dismissed. We won't show
  // another one for a certain time period after this.
  void OnOnboardingDismissed();

  void CloseSuccessNotification();

  void SetOnSuccessClickedForTesting(
      base::OnceCallback<void(SuccessNotificationAction)> callback);

 private:
  void ShowIncomingSuccess(const ShareTarget& share_target,
                           ReceivedContentType type,
                           const SkBitmap& image);

  NotificationDisplayService* notification_display_service_;
  NearbySharingService* nearby_service_;
  PrefService* pref_service_;
  Profile* profile_;

  // Maps notification ids to notification delegates.
  base::flat_map<std::string, std::unique_ptr<NearbyNotificationDelegate>>
      delegate_map_;

  // ShareTarget of the current transfer.
  base::Optional<ShareTarget> share_target_;

  // Last transfer status reported to OnTransferUpdate(). Null when no transfer
  // is in progress.
  base::Optional<TransferMetadata::Status> last_transfer_status_;

  base::OnceCallback<void(SuccessNotificationAction)>
      success_action_test_callback_;

  base::WeakPtrFactory<NearbyNotificationManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_
