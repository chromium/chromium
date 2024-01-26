// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
  static constexpr base::TimeDelta kNearbyDeviceTryingToShareDismissedTimeout =
      base::Minutes(15);

  enum class SuccessNotificationAction {
    kNone,
    kCopyText,
    kCopyImage,
    kOpenDownloads,
    kOpenUrl,
    kOpenWifiNetworksList,
  };

  // Type of content we received that determines the actions we provide.
  enum class ReceivedContentType {
    kFiles,            // One or more generic files
    kSingleImage,      // One image that will be shown as a preview
    kSingleUrl,        // One URL that will be opened on click.
    kText,             // Arbitrary text content
    kWifiCredentials,  // Wi-Fi credentials for a network configuration
  };

  class SettingsOpener {
   public:
    SettingsOpener() = default;
    SettingsOpener(SettingsOpener&) = delete;
    SettingsOpener& operator=(SettingsOpener&) = delete;
    virtual ~SettingsOpener() = default;

    // Open the chromeos settings page at the given uri, using
    // |chrome::SettingsWindowManager| by default.
    virtual void ShowSettingsPage(Profile* profile,
                                  const std::string& sub_page);
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
  void OnFastInitiationDevicesDetected() override;
  void OnFastInitiationDevicesNotDetected() override;
  void OnFastInitiationScanningStopped() override;

  // Shows a progress notification of the data being transferred to or from
  // |share_target|. Has a cancel action to cancel the transfer.
  void ShowProgress(const ShareTarget& share_target,
                    const TransferMetadata& transfer_metadata);

  // Shows an incoming connection request notification from |share_target|
  // wanting to send data to this device. Has a decline action and optionally an
  // accept action if the transfer needs to be accepted on the local device.
  void ShowConnectionRequest(const ShareTarget& share_target,
                             const TransferMetadata& transfer_metadata);

  // Show the notification that a nearby device is trying to share. If the user
  // is not yet onboarded this notification will prompt the user to set up
  // Nearby Share and redirect to the onboarding flow. If the user has already
  // onboarded this notification will prompt the user to go into high visibility
  // mode.
  void ShowNearbyDeviceTryingToShare();

  // Shows a notification for send or receive success.
  void ShowSuccess(const ShareTarget& share_target);

  // Shows a notification for send or receive failure.
  void ShowFailure(const ShareTarget& share_target,
                   const TransferMetadata& transfer_metadata);

  // Shows a notification for send or receive cancellation.
  void ShowCancelled(const ShareTarget& share_target);

  // Shows a notification to remind users of their current visibility selection.
  void ShowVisibilityReminder();

  // Closes any currently shown transfer notification (e.g. progress or
  // connection).
  void CloseTransfer();

  // Closes any currently shown notification that a nearby device is trying to
  // share. It does not have any effect on the actual onboarding UI or the high
  // visibility mode UI.
  void CloseNearbyDeviceTryingToShare();

  // Closes any currently shown nearby visibility reminder notification.
  void CloseVisibilityReminder();

  // Gets the currently registered delegate for |notification_id|.
  NearbyNotificationDelegate* GetNotificationDelegate(
      const std::string& notification_id);

  void OpenURL(GURL url);

  // Opens Wi-Fi Networks subpage in Settings.
  void OpenWifiNetworksList();

  // Cancels the currently in progress transfer.
  void CancelTransfer();

  // Rejects the currently in progress transfer.
  void RejectTransfer();

  // Accepts the currently in progress transfer.
  void AcceptTransfer();

  // Called when the nearby device is trying notification got clicked.
  void OnNearbyDeviceTryingToShareClicked();

  // Called when the nearby device is trying notification got dismissed. We
  // won't show another one for a certain time period after this.
  void OnNearbyDeviceTryingToShareDismissed(bool did_click_dismiss);

  void CloseSuccessNotification(const std::string& notification_id);

  // Called when the nearby visibility reminder notification got clicked.
  void OnNearbyVisibilityReminderClicked();

  // Called when the nearby visibility reminder notification got dismissed.
  void OnNearbyVisibilityReminderDismissed();

  void SetOnSuccessClickedForTesting(
      base::OnceCallback<void(SuccessNotificationAction)> callback);
  void SetSettingsOpenerForTesting(
      std::unique_ptr<SettingsOpener> settings_opener);

 private:
  void ShowIncomingSuccess(const ShareTarget& share_target,
                           ReceivedContentType type,
                           const SkBitmap& image);

  raw_ptr<NotificationDisplayService, DanglingUntriaged>
      notification_display_service_;
  raw_ptr<NearbySharingService> nearby_service_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<Profile> profile_;
  std::unique_ptr<SettingsOpener> settings_opener_;

  // Maps notification ids to notification delegates.
  base::flat_map<std::string, std::unique_ptr<NearbyNotificationDelegate>>
      delegate_map_;

  // ShareTarget of the current transfer.
  std::optional<ShareTarget> share_target_;

  // Last transfer status reported to OnTransferUpdate(). Null when no transfer
  // is in progress.
  std::optional<TransferMetadata::Status> last_transfer_status_;

  // The last time that 'Nearby device is trying to share' notification was
  // shown.
  base::TimeTicks last_device_nearby_sharing_notification_shown_timestamp_;

  base::OnceCallback<void(SuccessNotificationAction)>
      success_action_test_callback_;

  base::WeakPtrFactory<NearbyNotificationManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_NOTIFICATION_MANAGER_H_
