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

#include "base/cancelable_callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
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
      public NearbyConnectionsManager::DiscoveryListener {
 public:
  explicit NearbySharingServiceImpl(
      PrefService* prefs,
      NotificationDisplayService* notification_display_service,
      Profile* profile,
      std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
      NearbyProcessManager* process_manager);
  ~NearbySharingServiceImpl() override;

  // NearbySharingService:
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
  StatusCodes SendText(const ShareTarget& share_target,
                       std::string text) override;
  StatusCodes SendFiles(const ShareTarget& share_target,
                        const std::vector<base::FilePath>& files) override;
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

  // nearby_share::mojom::NearbyShareSettingsObserver:
  void OnEnabledChanged(bool enabled) override;
  void OnDeviceNameChanged(const std::string& device_name) override;
  void OnDataUsageChanged(nearby_share::mojom::DataUsage data_usage) override;
  void OnVisibilityChanged(nearby_share::mojom::Visibility visibility) override;
  void OnAllowedContactsChanged(
      const std::vector<std::string>& allowed_contacts) override;

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
  NearbyShareHttpNotifier* GetHttpNotifier() override;
  NearbyShareLocalDeviceDataManager* GetLocalDeviceDataManager() override;
  NearbyShareContactManager* GetContactManager() override;
  NearbyShareCertificateManager* GetCertificateManager() override;

  // NearbyConnectionsManager::DiscoveryListener:
  void OnEndpointDiscovered(const std::string& endpoint_id,
                            const std::vector<uint8_t>& endpoint_info) override;
  void OnEndpointLost(const std::string& endpoint_id) override;

 private:
  bool IsVisibleInBackground(Visibility visibility);
  const base::Optional<std::vector<uint8_t>> CreateEndpointInfo(
      const base::Optional<std::string>& device_name);
  void StartFastInitiationAdvertising();
  void StopFastInitiationAdvertising();
  void GetBluetoothAdapter();
  void OnGetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartFastInitiationAdvertising();
  void OnStartFastInitiationAdvertisingError();
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
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void InvalidateSurfaceState();
  bool ShouldStopNearbyProcess();
  void InvalidateSendSurfaceState();
  void InvalidateScanningState();
  void InvalidateReceiveSurfaceState();
  void InvalidateAdvertisingState();
  void StopAdvertising();
  void StartScanning();
  StatusCodes StopScanning();

  void OnTransferComplete();
  void OnTransferStarted(bool is_incoming);

  StatusCodes ReceivePayloads(const ShareTarget& share_target);
  StatusCodes SendPayloads(const ShareTarget& share_target);

  StatusCodes SendAttachments(ShareTarget share_target);
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

  Profile* profile_;
  NearbyShareSettings settings_;
  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  NearbyProcessManager* process_manager_;
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
  NearbyFileHandler file_handler_;

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

  mojo::Receiver<nearby_share::mojom::NearbyShareSettingsObserver>
      settings_receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NearbySharingServiceImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
