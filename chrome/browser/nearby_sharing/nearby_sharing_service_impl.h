// Copyright 2020 The Chromium Authors
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
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/attachment_info.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_http_notifier.h"
#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner_feature_usage_metrics.h"
#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"
#include "chrome/browser/nearby_sharing/incoming_share_target_info.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager.h"
#include "chrome/browser/nearby_sharing/metrics/attachment_metric_logger.h"
#include "chrome/browser/nearby_sharing/metrics/discovery_metric_logger.h"
#include "chrome/browser/nearby_sharing/metrics/nearby_share_metric_logger.h"
#include "chrome/browser/nearby_sharing/metrics/throughput_metric_logger.h"
#include "chrome/browser/nearby_sharing/nearby_notification_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_usage_metrics.h"
#include "chrome/browser/nearby_sharing/nearby_share_logger.h"
#include "chrome/browser/nearby_sharing/nearby_share_profile_info_provider_impl.h"
#include "chrome/browser/nearby_sharing/nearby_share_settings.h"
#include "chrome/browser/nearby_sharing/nearby_share_transfer_profiler.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/outgoing_share_target_info.h"
#include "chrome/browser/nearby_sharing/power_client.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/wifi_network_configuration/wifi_network_configuration_handler.h"
#include "chrome/services/sharing/public/proto/wire_format.pb.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_file_handler.h"
#include "chromeos/ash/components/nearby/presence/nearby_presence_service.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "components/prefs/pref_change_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "net/base/network_change_notifier.h"

class FastInitiationAdvertiser;
class FastInitiationScanner;
class NearbyConnectionsManager;
class NearbyShareContactManager;
class NearbyShareCertificateManager;
class NearbyShareClientFactory;
class NearbyShareLocalDeviceDataManager;
class NotificationDisplayService;
class PrefService;
class Profile;

namespace NearbySharingServiceUnitTests {
class NearbySharingServiceImplTestBase;
}

// All methods should be called from the same sequence that created the service.
class NearbySharingServiceImpl
    : public NearbySharingService,
      public nearby_share::mojom::NearbyShareSettingsObserver,
      public NearbyShareCertificateManager::Observer,
      public device::BluetoothAdapter::Observer,
      public NearbyConnectionsManager::IncomingConnectionListener,
      public NearbyConnectionsManager::DiscoveryListener,
      public NearbyConnectionsManager::BandwidthUpgradeListener,
      public ash::SessionObserver,
      public PowerClient::Observer,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // The number of unexpected nearby process shutdowns that we allow during a
  // fixed window before deciding not to restart the process.
  static constexpr int kMaxRecentNearbyProcessUnexpectedShutdownCount = 4;

  explicit NearbySharingServiceImpl(
      PrefService* prefs,
      NotificationDisplayService* notification_display_service,
      Profile* profile,
      std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
      ash::nearby::NearbyProcessManager* process_manager,
      std::unique_ptr<PowerClient> power_client,
      std::unique_ptr<WifiNetworkConfigurationHandler> wifi_network_handler);
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
  StatusCodes ClearForegroundReceiveSurfaces() override;
  bool IsInHighVisibility() const override;
  bool IsTransferring() const override;
  bool IsReceivingFile() const override;
  bool IsSendingFile() const override;
  bool IsScanning() const override;
  bool IsConnecting() const override;
  StatusCodes SendAttachments(
      const ShareTarget& share_target,
      std::vector<std::unique_ptr<Attachment>> attachments) override;
  void Accept(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Reject(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  void Cancel(const ShareTarget& share_target,
              StatusCodesCallback status_codes_callback) override;
  bool DidLocalUserCancelTransfer(const ShareTarget& share_target) override;
  void Open(const ShareTarget& share_target,
            StatusCodesCallback status_codes_callback) override;
  void OpenURL(GURL url) override;
  void SetArcTransferCleanupCallback(
      base::OnceCallback<void()> callback) override;
  NearbyNotificationDelegate* GetNotificationDelegate(
      const std::string& notification_id) override;
  void RecordFastInitiationNotificationUsage(bool success) override;
  NearbyShareSettings* GetSettings() override;
  NearbyShareHttpNotifier* GetHttpNotifier() override;
  NearbyShareLocalDeviceDataManager* GetLocalDeviceDataManager() override;
  NearbyShareContactManager* GetContactManager() override;
  NearbyShareCertificateManager* GetCertificateManager() override;
  NearbyNotificationManager* GetNotificationManager() override;

  // NearbyConnectionsManager::IncomingConnectionListener:
  void OnIncomingConnectionInitiated(
      const std::string& endpoint_id,
      const std::vector<uint8_t>& endpoint_info) override {}
  void OnIncomingConnectionAccepted(const std::string& endpoint_id,
                                    const std::vector<uint8_t>& endpoint_info,
                                    NearbyConnection* connection) override;

  // net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Test methods
  void FlushMojoForTesting();
  void set_free_disk_space_for_testing(int64_t free_disk_space) {
    free_disk_space_for_testing_ = free_disk_space;
  }
  void set_visibility_reminder_timer_delay_for_testing(base::TimeDelta delay) {
    visibility_reminder_timer_delay_ = delay;
    UpdateVisibilityReminderTimer(true);
  }

 private:
  friend class NearbySharingServiceUnitTests::NearbySharingServiceImplTestBase;

  // nearby_share::mojom::NearbyShareSettingsObserver:
  void OnEnabledChanged(bool enabled) override;
  void OnFastInitiationNotificationStateChanged(
      nearby_share::mojom::FastInitiationNotificationState state) override;
  void OnIsFastInitiationHardwareSupportedChanged(bool is_supported) override {}
  void OnDeviceNameChanged(const std::string& device_name) override;
  void OnDataUsageChanged(nearby_share::mojom::DataUsage data_usage) override;
  void OnVisibilityChanged(nearby_share::mojom::Visibility visibility) override;
  void OnAllowedContactsChanged(
      const std::vector<std::string>& allowed_contacts) override;
  void OnIsOnboardingCompleteChanged(bool is_complete) override {}

  // NearbyShareCertificateManager::Observer:
  void OnPublicCertificatesDownloaded() override;
  void OnPrivateCertificatesChanged() override;

  // NearbyConnectionsManager::DiscoveryListener:
  void OnEndpointDiscovered(const std::string& endpoint_id,
                            const std::vector<uint8_t>& endpoint_info) override;
  void OnEndpointLost(const std::string& endpoint_id) override;

  // NearbyConnectionsManager::BandwidthUpgradeListener:
  void OnInitialMedium(const std::string& endpoint_id,
                       const Medium medium) override;
  void OnBandwidthUpgrade(const std::string& endpoint_id,
                          const Medium medium) override;
  void OnBandwidthUpgradeV3(nearby::presence::PresenceDevice remote_device,
                            const Medium medium) override;

  // ash::SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void LowEnergyScanSessionHardwareOffloadingStatusChanged(
      device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
          status) override;

  // PowerClient::Observer:
  void SuspendImminent() override;
  void SuspendDone() override;

  base::ObserverList<TransferUpdateCallback>& GetReceiveCallbacksFromState(
      ReceiveSurfaceState state);
  bool IsVisibleInBackground(nearby_share::mojom::Visibility visibility);
  const std::optional<std::vector<uint8_t>> CreateEndpointInfo(
      const std::optional<std::string>& device_name);
  void GetBluetoothAdapter();
  void OnGetBluetoothAdapter(scoped_refptr<device::BluetoothAdapter> adapter);
  void StartFastInitiationAdvertising();
  void OnStartFastInitiationAdvertising();
  void OnStartFastInitiationAdvertisingError();
  void StopFastInitiationAdvertising();
  void OnStopFastInitiationAdvertising();

  // Processes endpoint discovered/lost events. We queue up the events to ensure
  // each discovered or lost event is fully handled before the next is run. For
  // example, we don't want to start processing an endpoint-lost event before
  // the corresponding endpoint-discovered event is finished. This is especially
  // important because of the asynchronous steps required to process an
  // endpoint-discovered event.
  void AddEndpointDiscoveryEvent(base::OnceClosure event);
  void HandleEndpointDiscovered(const std::string& endpoint_id,
                                const std::vector<uint8_t>& endpoint_info);
  void HandleEndpointLost(const std::string& endpoint_id);
  void FinishEndpointDiscoveryEvent();
  void OnOutgoingAdvertisementDecoded(
      const std::string& endpoint_id,
      const std::vector<uint8_t>& endpoint_info,
      sharing::mojom::AdvertisementPtr advertisement);
  void OnOutgoingDecryptedCertificate(
      const std::string& endpoint_id,
      const std::vector<uint8_t>& endpoint_info,
      sharing::mojom::AdvertisementPtr advertisement,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);
  void ScheduleCertificateDownloadDuringDiscovery(size_t attempt_count);
  void OnCertificateDownloadDuringDiscoveryTimerFired(size_t attempt_count);

  bool IsBluetoothPresent() const;
  bool IsBluetoothPowered() const;
  bool HasAvailableAdvertisingMediums();
  bool HasAvailableDiscoveryMediums();
  void InvalidateSurfaceState();
  bool ShouldStopNearbyProcess();
  void OnProcessShutdownTimerFired();
  void InvalidateSendSurfaceState();
  void InvalidateScanningState();
  void InvalidateFastInitiationAdvertising();
  void InvalidateReceiveSurfaceState();
  void InvalidateAdvertisingState();
  void StopAdvertising();
  void StartScanning();
  StatusCodes StopScanning();
  void StopAdvertisingAndInvalidateSurfaceState();

  void InvalidateFastInitiationScanning();
  void StartFastInitiationScanning();
  void OnFastInitiationDevicesDetected();
  void OnFastInitiationDevicesNotDetected();
  void StopFastInitiationScanning();

  void ScheduleRotateBackgroundAdvertisementTimer();
  void OnRotateBackgroundAdvertisementTimerFired();
  void RemoveOutgoingShareTargetWithEndpointId(const std::string& endpoint_id);

  void OnTransferComplete();
  void OnTransferStarted(bool is_incoming);

  void ReceivePayloads(ShareTarget share_target,
                       StatusCodesCallback status_codes_callback);
  StatusCodes SendPayloads(const ShareTarget& share_target);
  void OnUniquePathFetched(
      int64_t attachment_id,
      int64_t payload_id,
      base::OnceCallback<void(nearby::connections::mojom::Status)> callback,
      base::FilePath path);
  void OnPayloadPathRegistered(base::ScopedClosureRunner closure_runner,
                               bool* aggregated_success,
                               nearby::connections::mojom::Status status);
  void OnPayloadPathsRegistered(const ShareTarget& share_target,
                                std::unique_ptr<bool> aggregated_success,
                                StatusCodesCallback status_codes_callback);

  void OnOutgoingConnection(const ShareTarget& share_target,
                            base::TimeTicks connect_start_time,
                            NearbyConnection* connection);
  void SendIntroduction(const ShareTarget& share_target,
                        std::optional<std::string> four_digit_token);

  void CreatePayloads(ShareTarget share_target,
                      base::OnceCallback<void(ShareTarget, bool)> callback);
  void OnCreatePayloads(std::vector<uint8_t> endpoint_info,
                        ShareTarget share_target,
                        bool success);
  void OnOpenFiles(ShareTarget share_target,
                   base::OnceCallback<void(ShareTarget, bool)> callback,
                   std::vector<NearbyFileHandler::FileInfo> files);
  std::vector<nearby::connections::mojom::PayloadPtr> CreateTextPayloads(
      const std::vector<TextAttachment>& attachments);

  void WriteResponse(
      NearbyConnection& connection,
      sharing::nearby::ConnectionResponseFrame::Status reponse_status);
  void WriteCancel(NearbyConnection& connection);
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
      std::optional<NearbyShareDecryptedPublicCertificate> certificate);
  void RunPairedKeyVerification(
      const ShareTarget& share_target,
      const std::string& endpoint_id,
      base::OnceCallback<void(
          PairedKeyVerificationRunner::PairedKeyVerificationResult)> callback);
  void OnIncomingConnectionKeyVerificationDone(
      ShareTarget share_target,
      std::optional<std::string> four_digit_token,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result);
  void OnOutgoingConnectionKeyVerificationDone(
      const ShareTarget& share_target,
      std::optional<std::string> four_digit_token,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result);
  void RefreshUIOnDisconnection(ShareTarget share_target);
  void ReceiveIntroduction(ShareTarget share_target,
                           std::optional<std::string> four_digit_token);
  void OnReceivedIntroduction(ShareTarget share_target,
                              std::optional<std::string> four_digit_token,
                              std::optional<sharing::mojom::V1FramePtr> frame);
  void ReceiveConnectionResponse(ShareTarget share_target);
  void OnReceiveConnectionResponse(
      ShareTarget share_target,
      std::optional<sharing::mojom::V1FramePtr> frame);
  void OnStorageCheckCompleted(ShareTarget share_target,
                               std::optional<std::string> four_digit_token,
                               bool is_out_of_storage);
  void OnFrameRead(ShareTarget share_target,
                   std::optional<sharing::mojom::V1FramePtr> frame);
  void HandleCertificateInfoFrame(
      const sharing::mojom::CertificateInfoFramePtr& certificate_frame);

  void OnIncomingConnectionDisconnected(const ShareTarget& share_target);
  void OnOutgoingConnectionDisconnected(const ShareTarget& share_target);

  void OnIncomingMutualAcceptanceTimeout(const ShareTarget& share_target);
  void OnOutgoingMutualAcceptanceTimeout(const ShareTarget& share_target);

  void OnNearbyProcessStopped(
      ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);
  void CleanupAfterNearbyProcessStopped();
  void RestartNearbyProcessIfAppropriate(
      ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);
  bool ShouldRestartNearbyProcess(
      ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);
  void ClearRecentNearbyProcessUnexpectedShutdownCount();
  void BindToNearbyProcess();
  sharing::mojom::NearbySharingDecoder* GetNearbySharingDecoder();

  std::optional<ShareTarget> CreateShareTarget(
      const std::string& endpoint_id,
      const sharing::mojom::AdvertisementPtr& advertisement,
      std::optional<NearbyShareDecryptedPublicCertificate> certificate,
      bool is_incoming);

  void OnPayloadTransferUpdate(ShareTarget share_target,
                               TransferMetadata metadata);
  bool OnIncomingPayloadsComplete(ShareTarget& share_target);
  void RemoveIncomingPayloads(ShareTarget share_target);
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
  std::optional<std::vector<uint8_t>> GetBluetoothMacAddressForShareTarget(
      const ShareTarget& share_target);

  void ClearOutgoingShareTargetInfoMap();
  void SetAttachmentPayloadId(const Attachment& attachment, int64_t payload_id);
  std::optional<int64_t> GetAttachmentPayloadId(int64_t attachment_id);
  void UnregisterShareTarget(const ShareTarget& share_target);

  void OnStartAdvertisingResult(
      bool used_device_name,
      NearbyConnectionsManager::ConnectionsStatus status);
  void OnStopAdvertisingResult(
      NearbyConnectionsManager::ConnectionsStatus status);
  void OnStartDiscoveryResult(
      NearbyConnectionsManager::ConnectionsStatus status);
  void SetInHighVisibility(bool new_in_high_visibility);

  // Note: |share_target| is intentionally passed by value. A share target
  // reference could likely be invalidated by the owner during the multi-step
  // cancellation process.
  void DoCancel(ShareTarget share_target,
                StatusCodesCallback status_codes_callback,
                bool is_initiator_of_cancellation);

  void AbortAndCloseConnectionIfNecessary(const TransferMetadata::Status status,
                                          const ShareTarget& share_target);

  // The method is responsible for updating visibility reminder timer:
  // 1) Stops the timer if the feature flag is disabled OR Nearby Share is
  // disabled OR visibility is changed to 'Hidden"; 2) Restart the timer and
  // update the timestamp if we force it to update OR it's past 180 days since
  // last time we updated it.
  void UpdateVisibilityReminderTimer(bool reset_timestamp);
  void OnVisibilityReminderTimerFired();
  base::TimeDelta GetTimeUntilNextVisibilityReminder();

  raw_ptr<PrefService> prefs_ = nullptr;
  raw_ptr<Profile> profile_;
  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  raw_ptr<ash::nearby::NearbyProcessManager> process_manager_;
  std::unique_ptr<ash::nearby::NearbyProcessManager::NearbyProcessReference>
      process_reference_;
  std::unique_ptr<PowerClient> power_client_;
  std::unique_ptr<WifiNetworkConfigurationHandler> wifi_network_handler_;
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;
  // Advertiser which is non-null when we are attempting to share and
  // broadcasting Fast Initiation advertisements.
  std::unique_ptr<FastInitiationAdvertiser> fast_initiation_advertiser_;
  // Scanner which is non-null when we are performing a background scan for
  // remote devices that are attempting to share.
  std::unique_ptr<FastInitiationScanner> fast_initiation_scanner_;
  std::unique_ptr<NearbyNotificationManager> nearby_notification_manager_;
  NearbyShareHttpNotifier nearby_share_http_notifier_;
  std::unique_ptr<NearbyShareClientFactory> http_client_factory_;
  std::unique_ptr<NearbyShareProfileInfoProvider> profile_info_provider_;
  std::unique_ptr<NearbyShareLocalDeviceDataManager> local_device_data_manager_;
  std::unique_ptr<NearbyShareContactManager> contact_manager_;
  std::unique_ptr<NearbyShareCertificateManager> certificate_manager_;
  std::unique_ptr<NearbyShareTransferProfiler> transfer_profiler_;
  std::unique_ptr<NearbyShareLogger> logger_;
  NearbyShareSettings settings_;
  NearbyShareFeatureUsageMetrics feature_usage_metrics_;
  std::unique_ptr<FastInitiationScannerFeatureUsageMetrics>
      fast_initiation_scanning_metrics_;
  NearbyFileHandler file_handler_;
  bool is_screen_locked_ = false;
  base::OneShotTimer rotate_background_advertisement_timer_;
  base::OneShotTimer certificate_download_during_discovery_timer_;
  base::OneShotTimer process_shutdown_pending_timer_;

  // A list of service observers.
  base::ObserverList<NearbySharingService::Observer> observers_;
  // A list of foreground receivers.
  base::ObserverList<TransferUpdateCallback> foreground_receive_callbacks_;
  // A list of background receivers.
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
  std::optional<std::pair<ShareTarget, TransferMetadata>>
      last_incoming_metadata_;
  // The most recent outgoing TransferMetadata and ShareTarget.
  std::optional<std::pair<ShareTarget, TransferMetadata>>
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
  // TODO(crbug.com/40132032) update this map when handling payloads
  base::flat_map<base::UnguessableToken, OutgoingShareTargetInfo>
      outgoing_share_target_info_map_;
  // For metrics. The IDs of ShareTargets that are cancelled while trying to
  // establish an outgoing connection.
  base::flat_set<base::UnguessableToken> all_cancelled_share_target_ids_;
  // The IDs of ShareTargets that we cancelled the transfer to.
  base::flat_set<base::UnguessableToken> locally_cancelled_share_target_ids_;
  // A map from endpoint ID to endpoint info from discovered, contact-based
  // advertisements that could not decrypt any available public certificates.
  // During discovery, if certificates are downloaded, we revist this map and
  // retry certificate decryption.
  base::flat_map<std::string, std::vector<uint8_t>>
      discovered_advertisements_to_retry_map_;

  // Mapping of Endpoint Id to share targets.
  base::flat_map<std::string, ShareTarget> share_target_map_;
  // Mapping of Endpoint Id to total transfer size.
  base::flat_map<std::string, int64_t> transfer_size_map_;

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
  NearbyConnectionsManager::PowerLevel advertising_power_level_ =
      NearbyConnectionsManager::PowerLevel::kUnknown;
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
  bool in_high_visibility_ = false;
  // The time attachments are sent after a share target is selected. This is
  // used to time the process from selecting a share target to writing the
  // introduction frame (last frame before receiver gets notified).
  base::TimeTicks send_attachments_timestamp_;
  // Whether an incoming share has been accepted and we are waiting to log the
  // time from acceptance to the start of payload transfer.
  bool is_waiting_to_record_accept_to_transfer_start_metric_ = false;
  // Time at which an incoming transfer was accepted. This is used to calculate
  // the time between an incoming share being accepted and the first payload
  // byte being processed.
  base::TimeTicks incoming_share_accepted_timestamp_;
  int recent_nearby_process_unexpected_shutdown_count_ = 0;
  base::OneShotTimer clear_recent_nearby_process_shutdown_count_timer_;

  // Used to debounce OnNetworkChanged processing.
  base::RetainingOneShotTimer on_network_changed_delay_timer_;

  // Used to prevent the "Device nearby is sharing" notification from appearing
  // immediately after a completed share.
  base::OneShotTimer fast_initiation_scanner_cooldown_timer_;

  // The duration of reminder timer. In production, this is 180 days.
  // Can be shorten for testing efficiency purpose.
  base::TimeDelta visibility_reminder_timer_delay_;

  // Used to control when to show visibility reminder notification to users.
  base::OneShotTimer visibility_reminder_timer_;

  // Available free disk space for testing. Using real disk space can introduce
  // flakiness in tests.
  std::optional<int64_t> free_disk_space_for_testing_;

  // A queue of endpoint-discovered and endpoint-lost events that ensures the
  // events are processed sequentially, in the order received from Nearby
  // Connections. An event is processed either immediately, if there are no
  // other events in the queue, or as soon as the previous event processing
  // finishes. When processing finishes, the event is removed from the queue.
  base::queue<base::OnceClosure> endpoint_discovery_events_;

  mojo::Receiver<nearby_share::mojom::NearbyShareSettingsObserver>
      settings_receiver_{this};

  // Called when cleanup for ARC is needed as part of the transfer.
  base::OnceCallback<void()> arc_transfer_cleanup_callback_;

  // Stores the user's selected visibility state and allowed contacts when the
  // screen is locked and visibility is set to kYourDevices.
  nearby_share::mojom::Visibility user_visibility_;
  std::set<std::string> user_allowed_contacts_ = {};

  // Metrics loggers.
  std::unique_ptr<nearby::share::metrics::DiscoveryMetricLogger>
      discovery_metric_logger_;
  std::unique_ptr<nearby::share::metrics::ThroughputMetricLogger>
      throughput_metric_logger_;
  std::unique_ptr<nearby::share::metrics::AttachmentMetricLogger>
      attachment_metric_logger_;
  std::unique_ptr<nearby::share::metrics::NearbyShareMetricLogger>
      neaby_share_metric_logger_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<NearbySharingServiceImpl> weak_ptr_factory_{this};
  base::WeakPtrFactory<NearbySharingServiceImpl>
      endpoint_discovery_weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
