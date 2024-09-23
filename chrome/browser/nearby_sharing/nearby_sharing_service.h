// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/nearby_sharing/share_target_discovered_callback.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_update_callback.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom-shared.h"
#include "components/keyed_service/core/keyed_service.h"

class NearbyNotificationDelegate;
class NearbyNotificationManager;
class NearbyShareContactManager;
class NearbyShareCertificateManager;
class NearbyShareHttpNotifier;
class NearbyShareLocalDeviceDataManager;
class NearbyShareSettings;

// This service implements Nearby Sharing on top of the Nearby Connections mojo.
// Currently only single profile will be allowed to be bound at a time and only
// after the user has enabled Nearby Sharing in prefs.
class NearbySharingService : public KeyedService {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class StatusCodes {
    // The operation was successful.
    kOk = 0,
    // The operation failed, without any more information.
    kError = 1,
    // The operation failed since it was called in an invalid order.
    kOutOfOrderApiCall = 2,
    // Tried to stop something that was already stopped.
    kStatusAlreadyStopped = 3,
    // Tried to register an opposite foreground surface in the midst of a
    // transfer or connection.
    // (Tried to register Send Surface when receiving a file or tried to
    // register Receive Surface when
    // sending a file.)
    kTransferAlreadyInProgress = 4,
    // There is no available connection medium to use.
    kNoAvailableConnectionMedium = 5,
    kMaxValue = kNoAvailableConnectionMedium
  };

  enum class ReceiveSurfaceState {
    // Default, invalid state.
    kUnknown,
    // Background receive surface advertises only to contacts.
    kBackground,
    // Foreground receive surface advertises to everyone.
    kForeground,
  };

  enum class SendSurfaceState {
    // Default, invalid state.
    kUnknown,
    // Background send surface only listens to transfer update.
    kBackground,
    // Foreground send surface both scans and listens to transfer update.
    kForeground,
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnHighVisibilityChangeRequested() {}
    virtual void OnHighVisibilityChanged(bool in_high_visibility) {}

    virtual void OnNearbyProcessStopped() {}
    virtual void OnStartAdvertisingFailure() {}
    virtual void OnStartDiscoveryResult(bool success) {}

    virtual void OnFastInitiationDevicesDetected() {}
    virtual void OnFastInitiationDevicesNotDetected() {}
    virtual void OnFastInitiationScanningStopped() {}

    // Called during the |KeyedService| shutdown, but before everything has been
    // cleaned up. It is safe to remove any observers on this event.
    virtual void OnShutdown() {}

    // Share target specific events.
    virtual void OnShareTargetDiscoveryStarted() {}
    virtual void OnShareTargetDiscoveryStopped() {}
    virtual void OnShareTargetAdded(const ShareTarget& share_target) {}
    virtual void OnShareTargetRemoved(const ShareTarget& share_target) {}
    virtual void OnShareTargetSelected(const ShareTarget& share_target) {}
    virtual void OnShareTargetConnected(const ShareTarget& share_target) {}

    // Transfer specific events.
    virtual void OnTransferAccepted(const ShareTarget& share_target) {}
    // Note: Senders and receivers will emit this metric at different times.
    // Senders start transfers as soon as the receiver accepts it, but receivers
    // will end up starting the transfer a bit later due to the round-trip of
    // the accept message to the sender.
    virtual void OnTransferStarted(const ShareTarget& share_target,
                                   long total_bytes) {}
    virtual void OnTransferUpdated(const ShareTarget& share_target,
                                   float percentage_complete) {}
    virtual void OnTransferCompleted(const ShareTarget& share_target,
                                     TransferMetadata::Status status) {}
    virtual void OnInitialMedium(const ShareTarget& share_target,
                                 nearby::connections::mojom::Medium medium) {}
    virtual void OnBandwidthUpgrade(const ShareTarget& share_target,
                                    nearby::connections::mojom::Medium medium) {
    }
  };

  using StatusCodesCallback =
      base::OnceCallback<void(StatusCodes status_codes)>;

  ~NearbySharingService() override = default;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
  virtual bool HasObserver(Observer* observer) = 0;

  // Registers a send surface for handling payload transfer status and device
  // discovery.
  virtual StatusCodes RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback,
      SendSurfaceState state) = 0;

  // Unregisters the current send surface.
  virtual StatusCodes UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback) = 0;

  // Registers a receiver surface for handling payload transfer status.
  virtual StatusCodes RegisterReceiveSurface(
      TransferUpdateCallback* transfer_callback,
      ReceiveSurfaceState state) = 0;

  // Unregisters the current receive surface.
  virtual StatusCodes UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback) = 0;

  // Unregisters all foreground receive surfaces.
  virtual StatusCodes ClearForegroundReceiveSurfaces() = 0;

  // Returns true if a foreground receive surface is registered.
  virtual bool IsInHighVisibility() const = 0;

  // Returns true if there is an ongoing file transfer.
  virtual bool IsTransferring() const = 0;

  // Returns true if we're currently receiving a file.
  virtual bool IsReceivingFile() const = 0;

  // Returns true if we're currently sending a file.
  virtual bool IsSendingFile() const = 0;

  // Returns true if we're currently attempting to connect to a
  // remote device.
  virtual bool IsConnecting() const = 0;

  // Returns true if we are currently scanning for remote devices.
  virtual bool IsScanning() const = 0;

  // Sends |attachments| to the remote |share_target|.
  virtual StatusCodes SendAttachments(
      const ShareTarget& share_target,
      std::vector<std::unique_ptr<Attachment>> attachments) = 0;

  // Accepts incoming share from the remote |share_target|.
  virtual void Accept(const ShareTarget& share_target,
                      StatusCodesCallback status_codes_callback) = 0;

  // Rejects incoming share from the remote |share_target|.
  virtual void Reject(const ShareTarget& share_target,
                      StatusCodesCallback status_codes_callback) = 0;

  // Cancels outgoing shares to the remote |share_target|.
  virtual void Cancel(const ShareTarget& share_target,
                      StatusCodesCallback status_codes_callback) = 0;

  // Returns true if the local user cancelled the transfer to remote
  // |share_target|.
  virtual bool DidLocalUserCancelTransfer(const ShareTarget& share_target) = 0;

  // Opens attachments from the remote |share_target|.
  virtual void Open(const ShareTarget& share_target,
                    StatusCodesCallback status_codes_callback) = 0;

  // Opens an url target on a browser instance.
  virtual void OpenURL(GURL url) = 0;

  // Sets a cleanup callback to be called once done with transfer for ARC.
  virtual void SetArcTransferCleanupCallback(
      base::OnceCallback<void()> callback) = 0;

  // Gets a delegate to handle events for |notification_id| or nullptr.
  virtual NearbyNotificationDelegate* GetNotificationDelegate(
      const std::string& notification_id) = 0;

  // Records via Standard Feature Usage Logging whether or not advertising
  // successfully starts when the user clicks the "Device nearby is sharing"
  // notification.
  virtual void RecordFastInitiationNotificationUsage(bool success) = 0;

  virtual NearbyNotificationManager* GetNotificationManager() = 0;
  virtual NearbyShareSettings* GetSettings() = 0;
  virtual NearbyShareHttpNotifier* GetHttpNotifier() = 0;
  virtual NearbyShareLocalDeviceDataManager* GetLocalDeviceDataManager() = 0;
  virtual NearbyShareContactManager* GetContactManager() = 0;
  virtual NearbyShareCertificateManager* GetCertificateManager() = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
