// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/session/session_observer.h"
#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/attachment_info.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_http_notifier.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_enums.h"
#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"
#include "chrome/browser/nearby_sharing/incoming_share_target_info.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_file_handler.h"
#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"
#include "chrome/browser/nearby_sharing/nearby_process_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/outgoing_share_target_info.h"
#include "chrome/browser/nearby_sharing/power_client.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "chrome/services/sharing/public/mojom/nearby_decoder_types.mojom.h"
#include "chrome/services/sharing/public/proto/wire_format.pb.h"
#include "components/prefs/pref_change_registrar.h"

class FastInitiationManager;
class NearbyConnectionsManager;
class NearbyShareContactManager;
class NearbyShareCertificateManager;
class NearbyShareClientFactory;
class NearbyShareLocalDeviceDataManager;
class NotificationDisplayService;
class PrefService;
class Profile;

// All methods should be called from the same sequence that created the service.
class NearbySharingServiceImpl
    : public NearbySharingService,
      public nearby_share::mojom::NearbyShareSettingsObserver,
      public NearbyProcessManager::Observer,
      public device::BluetoothAdapter::Observer,
      public NearbyConnectionsManager::IncomingConnectionListener,
      public NearbyConnectionsManager::DiscoveryListener,
      public ash::SessionObserver,
      public PowerClient::Observer {
 public:
  explicit NearbySharingServiceImpl(
      PrefService* prefs,
      NotificationDisplayService* notification_display_service,
      Profile* profile,
      std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
      NearbyProcessManager* process_manager,
      std::unique_ptr<PowerClient> power_client);
  ~NearbySharingServiceImpl() override;

  // NearbySharingService:
  void Shutdown() override;
  void AddObserver(NearbySharingService::Observer* observer) override;
  void RemoveObserver(NearbySharingService::Observer* observer) override;
  bool HasObserver(NearbySharingService::Observer* observer) override;
  StatusCodes RegisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback,
      SendSurfaceState state) override;
  StatusCodes UnregisterSendSurface(
      TransferUpdateCallback* transfer_callback,
      ShareTargetDiscoveredCallback* discovery_callback) override;
  StatusCodes RegisterReceiveSurface(TransferUpdateCallback* transfer_callback,
                                     ReceiveSurfaceState state) override;
  StatusCodes UnregisterReceiveSurface(
      TransferUpdateCallback* transfer_callback) override;
  bool IsInHighVisibility() override;
  StatusCodes SendAttachments(
      const ShareTarget& share_target,
      std::vector<std::unique_ptr<Attachment>> attachments) override;
  void Accept(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Reject(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Cancel(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Open(const ShareTarget& share_target,
            StatusCodesCallback status_codes_callback) override;
  NearbyNotificationDelegate* GetNotificationDelegate(
      const std::string& notification_id) override;
  NearbyShareSettings* GetSettings() override;
  NearbyShareHttpNotifier* GetHttpNotifier() override;
  NearbyShareLocalDeviceDataManager* GetLocalDeviceDataManager() override;
  NearbyShareContactManager* GetContactManager() override;
  NearbyShareCertificateManager* GetCertificateManager() override;

  // NearbyProcessManager::Observer:
  void OnNearbyProfileChanged(Profile* profile) override;
  void OnNearbyProcessStarted() override;
  void OnNearbyProcessStopped() override;

  // NearbyConnectionsManager::IncomingConnectionListener:
  void OnIncomingConnection(const std::string& endpoint_id,
                            const std::vector<uint8_t>& endpoint_info,
                            NearbyConnection* connection) override;

  // Test methods
  void FlushMojoForTesting();
  void set_free_disk_space_for_testing(int64_t free_disk_space) {
    free_disk_space_for_testing_ = free_disk_space;
  }

 private:
  // nearby_share::mojom::NearbyShareSettingsObserver:
  void OnEnabledChanged(bool enabled) override;
  void OnDeviceNameChanged(const std::string& device_name) override;
  void OnDataUsageChanged(nearby_share::mojom::DataUsage data_usage) override;
  void OnVisibilityChanged(nearby_share::mojom::Visibility visibility) override;
  void OnAllowedContactsChanged(
      const std::vector<std::string>& allowed_contacts) override;

  // NearbyConnectionsManager::DiscoveryListener:
  void OnEndpointDiscovered(const std::string& endpoint_id,
                            const std::vector<uint8_t>& endpoint_info) override;
  void OnEndpointLost(const std::string& endpoint_id) override;

  // ash::SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;

  // PowerClient::Observer:
  void SuspendImminent() override;
  void SuspendDone() override;

  base::ObserverList<TransferUpdateCallback>& GetReceiveCallbacksFromState(
      ReceiveSurfaceState state);
  bool IsVisibleInBackground(Visibility visibility);
  const base::Optional<std::vector<uint8_t>> CreateEndpointInfo(
      const base::Optional<std::string>& device_name);
  void GetBluetoothAdapter();
  void OnGetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void StartFastInitiationAdvertising();
  void OnStartFastInitiationAdvertising();
  void OnStartFastInitiationAdvertisingError();
  void StopFastInitiationAdvertising();
  void OnStopFastInitiationAdvertising();
  void OnOutgoingAdvertisementDecoded(
      const std::string& endpoint_id,
      sharing::mojom::AdvertisementPtr advertisement);
  void OnOutgoingDecryptedCertificate(
      const std::string& endpoint_id,
      sharing::mojom::AdvertisementPtr advertisement,
      base::Optional<NearbyShareDecryptedPublicCertificate> certificate);
  bool IsBluetoothPresent() const;
  bool IsBluetoothPowered() const;
  bool HasAvailableConnectionMediums();
  void InvalidateSurfaceState();
  bool ShouldStopNearbyProcess();
  void InvalidateSendSurfaceState();
  void InvalidateScanningState();
  void InvalidateFastInitiationAdvertising();
  void InvalidateReceiveSurfaceState();
  void InvalidateAdvertisingState();
  void StopAdvertising();
  void StartScanning();
  StatusCodes StopScanning();
  void ScheduleRotateBackgroundAdvertisementTimer();
  void OnRotateBackgroundAdvertisementTimerFired();

  void OnTransferComplete();
  void OnTransferStarted(bool is_incoming);

  void ReceivePayloads(ShareTarget share_target,
                       StatusCodesCallback status_codes_callback);
  StatusCodes SendPayloads(const ShareTarget& share_target);
  void OnUniquePathFetched(
      int64_t attachment_id,
      int64_t payload_id,
      base::OnceCallback<void(location::nearby::connections::mojom::Status)>
          callback,
      base::FilePath path);
  void OnPayloadPathRegistered(
      base::ScopedClosureRunner closure_runner,
      bool* aggregated_success,
      location::nearby::connections::mojom::Status status);
  void OnPayloadPathsRegistered(const ShareTarget& share_target,
                                std::unique_ptr<bool> aggregated_success,
                                StatusCodesCallback status_codes_callback);

  void OnOutgoingConnection(const ShareTarget& share_target,
                            NearbyConnection* connection);
  void SendIntroduction(const ShareTarget& share_target,
                        base::Optional<std::string> four_digit_token);

  void CreatePayloads(ShareTarget share_target,
                      base::OnceCallback<void(ShareTarget, bool)> callback);
  void OnCreatePayloads(std::vector<uint8_t> endpoint_info,
                        ShareTarget share_target,
                        bool success);
  void OnOpenFiles(ShareTarget share_target,
                   base::OnceCallback<void(ShareTarget, bool)> callback,
                   std::vector<NearbyFileHandler::FileInfo> files);
  std::vector<location::nearby::connections::mojom::PayloadPtr>
  CreateTextPayloads(const std::vector<TextAttachment>& attachments);

  void WriteResponse(
      NearbyConnection& connection,
      sharing::nearby::ConnectionResponseFrame::Status reponse_status);
  void Fail(const ShareTarget& share_target, TransferMetadata::Status status);
  void OnIncomingAdvertisementDecoded(
      const std::string& endpoint_id,
      ShareTarget placeholder_share_target,
      sharing::mojom::AdvertisementPtr advertisement);
  void OnIncomingTransferUpdate(const ShareTarget& share_target,
                                const TransferMetadata& metadata);
  void OnOutgoingTransferUpdate(const ShareTarget& share_target,
                                const TransferMetadata& metadata);
  void CloseConnection(const ShareTarget& share_target);
  void OnIncomingDecryptedCertificate(
      const std::string& endpoint_id,
      sharing::mojom::AdvertisementPtr advertisement,
      ShareTarget placeholder_share_target,
      base::Optional<NearbyShareDecryptedPublicCertificate> certificate);
  void RunPairedKeyVerification(
      const ShareTarget& share_target,
      const std::string& endpoint_id,
      base::OnceCallback<void(
          PairedKeyVerificationRunner::PairedKeyVerificationResult)> callback);
  void OnIncomingConnectionKeyVerificationDone(
      ShareTarget share_target,
      base::Optional<std::string> four_digit_token,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result);
  void OnOutgoingConnectionKeyVerificationDone(
      const ShareTarget& share_target,
      base::Optional<std::string> four_digit_token,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result);
  void RefreshUIOnDisconnection(ShareTarget share_target);
  void ReceiveIntroduction(ShareTarget share_target,
                           base::Optional<std::string> four_digit_token);
  void OnReceivedIntroduction(ShareTarget share_target,
                              base::Optional<std::string> four_digit_token,
                              base::Optional<sharing::mojom::V1FramePtr> frame);
  void ReceiveConnectionResponse(ShareTarget share_target);
  void OnReceiveConnectionResponse(
      ShareTarget share_target,
      base::Optional<sharing::mojom::V1FramePtr> frame);
  void OnStorageCheckCompleted(ShareTarget share_target,
                               base::Optional<std::string> four_digit_token,
                               bool is_out_of_storage);
  void OnFrameRead(ShareTarget share_target,
                   base::Optional<sharing::mojom::V1FramePtr> frame);
  void HandleCertificateInfoFrame(
      const sharing::mojom::CertificateInfoFramePtr& certificate_frame);

  void OnIncomingConnectionDisconnected(const ShareTarget& share_target);
  void OnOutgoingConnectionDisconnected(const ShareTarget& share_target);

  void OnIncomingMutualAcceptanceTimeout(const ShareTarget& share_target);
  void OnOutgoingMutualAcceptanceTimeout(const ShareTarget& share_target);

  base::Optional<ShareTarget> CreateShareTarget(
      const std::string& endpoint_id,
      const sharing::mojom::AdvertisementPtr& advertisement,
      base::Optional<NearbyShareDecryptedPublicCertificate> certificate,
      bool is_incoming);

  void OnPayloadTransferUpdate(ShareTarget share_target,
                               TransferMetadata metadata);
  bool OnIncomingPayloadsComplete(ShareTarget& share_target);
  void OnPayloadsFailed(ShareTarget share_target);
  void Disconnect(const ShareTarget& share_target, TransferMetadata metadata);
  void OnDisconnectingConnectionTimeout(const std::string& endpoint_id);
  void OnDisconnectingConnectionDisconnected(const ShareTarget& share_target,
                                             const std::string& endpoint_id);

  ShareTargetInfo& GetOrCreateShareTargetInfo(const ShareTarget& share_target,
                                              const std::string& endpoint_id);

  ShareTargetInfo* GetShareTargetInfo(const ShareTarget& share_target);
  IncomingShareTargetInfo* GetIncomingShareTargetInfo(
      const ShareTarget& share_target);
  OutgoingShareTargetInfo* GetOutgoingShareTargetInfo(
      const ShareTarget& share_target);

  NearbyConnection* GetConnection(const ShareTarget& share_target);
  base::Optional<std::vector<uint8_t>> GetBluetoothMacAddress(
      const ShareTarget& share_target);

  void ClearOutgoingShareTargetInfoMap();
  void SetAttachmentPayloadId(const Attachment& attachment, int64_t payload_id);
  base::Optional<int64_t> GetAttachmentPayloadId(int64_t attachment_id);
  void UnregisterShareTarget(const ShareTarget& share_target);

  void OnStartAdvertisingResult(
      bool used_device_name,
      NearbyConnectionsManager::ConnectionsStatus status);
  void SetInHighVisibility(bool in_high_visibility);

  Profile* profile_;
  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  NearbyProcessManager* process_manager_;
  std::unique_ptr<PowerClient> power_client_;
  ScopedObserver<NearbyProcessManager, NearbyProcessManager::Observer>
      nearby_process_observer_{this};
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  std::unique_ptr<FastInitiationManager> fast_initiation_manager_;
  std::unique_ptr<NearbyNotificationManager> nearby_notification_manager_;
  NearbyShareHttpNotifier nearby_share_http_notifier_;
  std::unique_ptr<NearbyShareClientFactory> http_client_factory_;
  std::unique_ptr<NearbyShareLocalDeviceDataManager> local_device_data_manager_;
  std::unique_ptr<NearbyShareContactManager> contact_manager_;
  std::unique_ptr<NearbyShareCertificateManager> certificate_manager_;
  NearbyShareSettings settings_;
  NearbyFileHandler file_handler_;
  bool is_screen_locked_ = false;
  base::OneShotTimer rotate_background_advertisement_timer_;

  // A list of service observers.
  base::ObserverList<NearbySharingService::Observer> observers_;
  // A list of foreground receivers.
  base::ObserverList<TransferUpdateCallback> foreground_receive_callbacks_;
  // A list of foreground receivers.
  base::ObserverList<TransferUpdateCallback> background_receive_callbacks_;
  // A list of foreground receivers for transfer updates on the send surface.
  base::ObserverList<TransferUpdateCallback>
      foreground_send_transfer_callbacks_;
  // A list of foreground receivers for discovered device updates on the send
  // surface.
  base::ObserverList<ShareTargetDiscoveredCallback>
      foreground_send_discovery_callbacks_;
  // A list of background receivers for transfer updates on the send surface.
  base::ObserverList<TransferUpdateCallback>
      background_send_transfer_callbacks_;
  // A list of background receivers for discovered device updates on the send
  // surface.
  base::ObserverList<ShareTargetDiscoveredCallback>
      background_send_discovery_callbacks_;

  // Registers the most recent TransferMetadata and ShareTarget used for
  // transitioning notifications between foreground surfaces and background
  // surfaces. Empty if no metadata is available.
  base::Optional<std::pair<ShareTarget, TransferMetadata>>
      last_incoming_metadata_;
  // The most recent outgoing TransferMetadata and ShareTarget.
  base::Optional<std::pair<ShareTarget, TransferMetadata>>
      last_outgoing_metadata_;
  // A map of ShareTarget id to IncomingShareTargetInfo. This lets us know which
  // Nearby Connections endpoint and public certificate are related to the
  // incoming share target.
  base::flat_map<base::UnguessableToken, IncomingShareTargetInfo>
      incoming_share_target_info_map_;
  // A map of endpoint id to ShareTarget, where each ShareTarget entry
  // directly corresponds to a OutgoingShareTargetInfo entry in
  // outgoing_share_target_info_map_;
  base::flat_map<std::string, ShareTarget> outgoing_share_target_map_;
  // A map of ShareTarget id to OutgoingShareTargetInfo. This lets us know which
  // endpoint and public certificate are related to the outgoing share target.
  // TODO(crbug/1085068) update this map when handling payloads
  base::flat_map<base::UnguessableToken, OutgoingShareTargetInfo>
      outgoing_share_target_info_map_;

  // A mapping of Attachment Id to additional AttachmentInfo related to the
  // Attachment.
  base::flat_map<int64_t, AttachmentInfo> attachment_info_map_;

  // This alarm is used to disconnect the sharing connection if both sides do
  // not press accept within the timeout.
  base::CancelableOnceClosure mutual_acceptance_timeout_alarm_;

  // A map of ShareTarget id to disconnection timeout callback. Used to only
  // disconnect after a timeout to keep sending any pending payloads.
  base::flat_map<std::string, std::unique_ptr<base::CancelableOnceClosure>>
      disconnection_timeout_alarms_;

  // The current advertising power level. PowerLevel::kUnknown while not
  // advertising.
  PowerLevel advertising_power_level_ = PowerLevel::kUnknown;
  // True if we are currently scanning for remote devices.
  bool is_scanning_ = false;
  // True if we're currently sending or receiving a file.
  bool is_transferring_ = false;
  // True if we're currently receiving a file.
  bool is_receiving_files_ = false;
  // True if we're currently sending a file.
  bool is_sending_files_ = false;
  // True if we're currently attempting to connect to a remote device.
  bool is_connecting_ = false;
  // The time scanning began.
  base::Time scanning_start_timestamp_;
  // True when we are advertising with a device name visible to everyone.
  bool in_high_visibility = false;

  // Available free disk space for testing. Using real disk space can introduce
  // flakiness in tests.
  base::Optional<int64_t> free_disk_space_for_testing_;

  mojo::Receiver<nearby_share::mojom::NearbyShareSettingsObserver>
      settings_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NearbySharingServiceImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
