// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include <utility>

#include "ash/public/cpp/session/session_controller.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client_impl.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"
#include "chrome/browser/nearby_sharing/fast_initiation_manager.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_connections_manager.h"
#include "chrome/browser/nearby_sharing/nearby_share_default_device_name.h"
#include "chrome/browser/nearby_sharing/paired_key_verification_runner.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"
#include "chrome/services/sharing/public/cpp/conversions.h"
#include "chrome/services/sharing/public/mojom/nearby_connections_types.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/random.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace {

constexpr base::TimeDelta kBackgroundAdvertisementRotationDelayMin =
    base::TimeDelta::FromMinutes(12);
constexpr base::TimeDelta kBackgroundAdvertisementRotationDelayMax =
    base::TimeDelta::FromMinutes(15);

// Used to hash a token into a 4 digit string.
constexpr int kHashModulo = 9973;
constexpr int kHashBaseMultiplier = 31;

std::string ReceiveSurfaceStateToString(
    NearbySharingService::ReceiveSurfaceState state) {
  switch (state) {
    case NearbySharingService::ReceiveSurfaceState::kForeground:
      return "FOREGROUND";
    case NearbySharingService::ReceiveSurfaceState::kBackground:
      return "BACKGROUND";
    case NearbySharingService::ReceiveSurfaceState::kUnknown:
      return "UNKNOWN";
  }
}

std::string SendSurfaceStateToString(
    NearbySharingService::SendSurfaceState state) {
  switch (state) {
    case NearbySharingService::SendSurfaceState::kForeground:
      return "FOREGROUND";
    case NearbySharingService::SendSurfaceState::kBackground:
      return "BACKGROUND";
    case NearbySharingService::SendSurfaceState::kUnknown:
      return "UNKNOWN";
  }
}

std::string PowerLevelToString(PowerLevel level) {
  switch (level) {
    case PowerLevel::kLowPower:
      return "LOW_POWER";
    case PowerLevel::kMediumPower:
      return "MEDIUM_POWER";
    case PowerLevel::kHighPower:
      return "HIGH_POWER";
    case PowerLevel::kUnknown:
      return "UNKNOWN";
  }
}

base::Optional<std::string> GetDeviceName(
    const sharing::mojom::AdvertisementPtr& advertisement,
    const base::Optional<NearbyShareDecryptedPublicCertificate>& certificate) {
  DCHECK(advertisement);

  // Device name is always included when visible to everyone.
  if (advertisement->device_name)
    return *(advertisement->device_name);

  // For contacts only advertisements, we can't do anything without the
  // certificate.
  if (!certificate || !certificate->unencrypted_metadata().has_device_name())
    return base::nullopt;

  return certificate->unencrypted_metadata().device_name();
}

std::string GetDeviceId(
    const std::string& endpoint_id,
    const base::Optional<NearbyShareDecryptedPublicCertificate>& certificate) {
  if (!certificate || certificate->id().empty())
    return endpoint_id;

  return std::string(certificate->id().begin(), certificate->id().end());
}

base::Optional<std::string> ToFourDigitString(
    const base::Optional<std::vector<uint8_t>>& bytes) {
  if (!bytes)
    return base::nullopt;

  int hash = 0;
  int multiplier = 1;
  for (uint8_t byte : *bytes) {
    // Java bytes are signed two's complement so cast to use the correct sign.
    hash = (hash + static_cast<int8_t>(byte) * multiplier) % kHashModulo;
    multiplier = (multiplier * kHashBaseMultiplier) % kHashModulo;
  }

  return base::StringPrintf("%04d", std::abs(hash));
}

bool IsOutOfStorage(base::FilePath file_path,
                    int64_t storage_required,
                    base::Optional<int64_t> free_disk_space_for_testing) {
  int64_t free_space = free_disk_space_for_testing.value_or(
      base::SysInfo::AmountOfFreeDiskSpace(file_path));
  return free_space < storage_required;
}

bool DoAttachmentsExceedThreshold(const ShareTarget& share_target,
                                  int64_t threshold) {
  for (const auto& attachment : share_target.text_attachments) {
    if (attachment.size() > threshold)
      return false;

    threshold -= attachment.size();
  }

  for (const auto& attachment : share_target.file_attachments) {
    if (attachment.size() > threshold)
      return false;

    threshold -= attachment.size();
  }

  return true;
}

DataUsage CheckFileSizeForDataUsagePreference(DataUsage client_preference,
                                              const ShareTarget& share_target) {
  if (client_preference == DataUsage::kOffline)
    return client_preference;

  if (DoAttachmentsExceedThreshold(share_target, kOnlineFileSizeLimitBytes))
    return DataUsage::kOffline;

  return client_preference;
}

int64_t GeneratePayloadId() {
  int64_t payload_id = 0;
  crypto::RandBytes(&payload_id, sizeof(payload_id));
  return payload_id;
}

// Wraps a call to OnTransferUpdate() to filter any updates after receiving a
// final status.
class TransferUpdateDecorator : public TransferUpdateCallback {
 public:
  using Callback = base::RepeatingCallback<void(const ShareTarget&,
                                                const TransferMetadata&)>;

  explicit TransferUpdateDecorator(Callback callback)
      : callback_(std::move(callback)) {}
  TransferUpdateDecorator(const TransferUpdateDecorator&) = delete;
  TransferUpdateDecorator& operator=(const TransferUpdateDecorator&) = delete;
  ~TransferUpdateDecorator() override = default;

  void OnTransferUpdate(const ShareTarget& share_target,
                        const TransferMetadata& transfer_metadata) override {
    NS_LOG(VERBOSE) << __func__ << ": Transfer update decorator: "
                    << "Transfer update for share target with ID "
                    << share_target.id << ": "
                    << TransferMetadata::StatusToString(
                           transfer_metadata.status());
    if (got_final_status_) {
      // If we already got a final status, we can ignore any subsequent final
      // statuses caused by race conditions.
      return;
    }
    got_final_status_ = transfer_metadata.is_final_status();
    callback_.Run(share_target, transfer_metadata);
  }

 private:
  bool got_final_status_ = false;
  Callback callback_;
};

}  // namespace

NearbySharingServiceImpl::NearbySharingServiceImpl(
    PrefService* prefs,
    NotificationDisplayService* notification_display_service,
    Profile* profile,
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
    NearbyProcessManager* process_manager,
    std::unique_ptr<PowerClient> power_client)
    : profile_(profile),
      nearby_connections_manager_(std::move(nearby_connections_manager)),
      process_manager_(process_manager),
      power_client_(std::move(power_client)),
      http_client_factory_(std::make_unique<NearbyShareClientFactoryImpl>(
          IdentityManagerFactory::GetForProfile(profile),
          profile->GetURLLoaderFactory(),
          &nearby_share_http_notifier_)),
      local_device_data_manager_(
          NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
              prefs,
              http_client_factory_.get(),
              GetNearbyShareDefaultDeviceName(profile_))),
      contact_manager_(NearbyShareContactManagerImpl::Factory::Create(
          prefs,
          http_client_factory_.get(),
          local_device_data_manager_.get())),
      certificate_manager_(NearbyShareCertificateManagerImpl::Factory::Create(
          local_device_data_manager_.get(),
          contact_manager_.get(),
          prefs,
          content::BrowserContext::GetDefaultStoragePartition(profile)
              ->GetProtoDatabaseProvider(),
          profile->GetPath(),
          http_client_factory_.get())),
      settings_(prefs, local_device_data_manager_.get()) {
  DCHECK(profile_);
  DCHECK(nearby_connections_manager_);
  DCHECK(power_client_);

#if defined(OS_CHROMEOS)
  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    is_screen_locked_ = session_controller->IsScreenLocked();
    session_controller->AddObserver(this);
  }
#endif  // OS_CHROMEOS

  nearby_process_observer_.Add(process_manager_);
  power_client_->AddObserver(this);

  settings_.AddSettingsObserver(settings_receiver_.BindNewPipeAndPassRemote());

  GetBluetoothAdapter();

  nearby_notification_manager_ = std::make_unique<NearbyNotificationManager>(
      notification_display_service, this, prefs, profile_);

  if (settings_.GetEnabled()) {
    local_device_data_manager_->Start();
    contact_manager_->Start();
    certificate_manager_->Start();
  }
}

NearbySharingServiceImpl::~NearbySharingServiceImpl() {
  // Make sure the service has been shut down properly before.
  DCHECK(!nearby_notification_manager_);
  DCHECK(!bluetooth_adapter_ || !bluetooth_adapter_->HasObserver(this));
}

void NearbySharingServiceImpl::Shutdown() {
  // Before we clean up, lets give observers a heads up we are shutting down.
  for (auto& observer : observers_) {
    observer.OnShutdown();
  }
  observers_.Clear();

  // Clear in-progress transfers.
  ClearOutgoingShareTargetInfoMap();
  incoming_share_target_info_map_.clear();

  StopAdvertising();
  StopFastInitiationAdvertising();
  StopScanning();
  nearby_connections_manager_->Shutdown();

  // Destroy NearbyNotificationManager as its profile has been shut down.
  nearby_notification_manager_.reset();

  // Stop listening to NearbyProcessManager events and stop the utility process.
  nearby_process_observer_.Remove(process_manager_);
  if (process_manager_->IsActiveProfile(profile_))
    process_manager_->StopProcess(profile_);

  power_client_->RemoveObserver(this);

  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);

#if defined(OS_CHROMEOS)
  auto* session_controller = ash::SessionController::Get();
  if (session_controller)
    session_controller->RemoveObserver(this);
#endif  // OS_CHROMEOS

  foreground_receive_callbacks_.Clear();
  background_receive_callbacks_.Clear();
  foreground_send_transfer_callbacks_.Clear();
  foreground_send_discovery_callbacks_.Clear();
  background_send_transfer_callbacks_.Clear();
  background_send_discovery_callbacks_.Clear();

  last_incoming_metadata_.reset();
  last_outgoing_metadata_.reset();
  attachment_info_map_.clear();
  mutual_acceptance_timeout_alarm_.Cancel();
  disconnection_timeout_alarms_.clear();

  is_transferring_ = false;
  is_receiving_files_ = false;
  is_sending_files_ = false;
  is_connecting_ = false;

  settings_receiver_.reset();

  if (settings_.GetEnabled()) {
    local_device_data_manager_->Stop();
    contact_manager_->Stop();
    certificate_manager_->Stop();
  }

  // |profile_| has now been shut down so we shouldn't use it anymore.
  profile_ = nullptr;
}

void NearbySharingServiceImpl::AddObserver(
    NearbySharingService::Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbySharingServiceImpl::RemoveObserver(
    NearbySharingService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NearbySharingServiceImpl::HasObserver(
    NearbySharingService::Observer* observer) {
  return observers_.HasObserver(observer);
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::RegisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback,
    SendSurfaceState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transfer_callback);
  DCHECK(discovery_callback);
  DCHECK_NE(state, SendSurfaceState::kUnknown);

  if (foreground_send_transfer_callbacks_.HasObserver(transfer_callback) ||
      background_send_transfer_callbacks_.HasObserver(transfer_callback)) {
    NS_LOG(VERBOSE) << __func__
                    << ": RegisterSendSurface failed. Already registered for a "
                       "different state.";
    return StatusCodes::kError;
  }

  if (state == SendSurfaceState::kForeground) {
    foreground_send_transfer_callbacks_.AddObserver(transfer_callback);
    foreground_send_discovery_callbacks_.AddObserver(discovery_callback);
  } else {
    background_send_transfer_callbacks_.AddObserver(transfer_callback);
    background_send_discovery_callbacks_.AddObserver(discovery_callback);
  }

  if (is_receiving_files_) {
    UnregisterSendSurface(transfer_callback, discovery_callback);
    NS_LOG(VERBOSE)
        << __func__
        << ": Ignore registering (and unregistering if registered) send "
           "surface because we're currently receiving files.";
    return StatusCodes::kTransferAlreadyInProgress;
  }

  // If the share sheet to be registered is a foreground surface, let it catch
  // up with most recent transfer metadata immediately.
  if (state == SendSurfaceState::kForeground && last_outgoing_metadata_) {
    // When a new share sheet is registered, we want to immediately show the
    // in-progress bar.
    discovery_callback->OnShareTargetDiscovered(last_outgoing_metadata_->first);
    transfer_callback->OnTransferUpdate(last_outgoing_metadata_->first,
                                        last_outgoing_metadata_->second);
  }

  // Let newly registered send surface catch up with discovered share targets
  // from current scanning session.
  for (const std::pair<std::string, ShareTarget>& item :
       outgoing_share_target_map_) {
    discovery_callback->OnShareTargetDiscovered(item.second);
  }

  NS_LOG(VERBOSE) << __func__
                  << ": A SendSurface has been registered for state: "
                  << SendSurfaceStateToString(state);
  InvalidateSendSurfaceState();
  return StatusCodes::kOk;
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::UnregisterSendSurface(
    TransferUpdateCallback* transfer_callback,
    ShareTargetDiscoveredCallback* discovery_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transfer_callback);
  DCHECK(discovery_callback);
  if (!foreground_send_transfer_callbacks_.HasObserver(transfer_callback) &&
      !background_send_transfer_callbacks_.HasObserver(transfer_callback)) {
    NS_LOG(VERBOSE)
        << __func__
        << ": unregisterSendSurface failed. Unknown TransferUpdateCallback";
    return StatusCodes::kError;
  }

  if (foreground_send_transfer_callbacks_.might_have_observers() &&
      last_outgoing_metadata_ &&
      last_outgoing_metadata_->second.is_final_status()) {
    // We already saw the final status in the foreground
    // Nullify it so the next time the user opens sharing, it starts the UI from
    // the beginning
    last_outgoing_metadata_.reset();
  }

  SendSurfaceState state = SendSurfaceState::kUnknown;
  if (foreground_send_transfer_callbacks_.HasObserver(transfer_callback)) {
    foreground_send_transfer_callbacks_.RemoveObserver(transfer_callback);
    foreground_send_discovery_callbacks_.RemoveObserver(discovery_callback);
    state = SendSurfaceState::kForeground;
  } else {
    background_send_transfer_callbacks_.RemoveObserver(transfer_callback);
    background_send_discovery_callbacks_.RemoveObserver(discovery_callback);
    state = SendSurfaceState::kBackground;
  }

  // Displays the most recent payload status processed by foreground surfaces on
  // background surfaces.
  if (!foreground_send_transfer_callbacks_.might_have_observers() &&
      last_outgoing_metadata_) {
    for (TransferUpdateCallback& background_transfer_callback :
         background_send_transfer_callbacks_) {
      background_transfer_callback.OnTransferUpdate(
          last_outgoing_metadata_->first, last_outgoing_metadata_->second);
    }
  }

  NS_LOG(VERBOSE) << __func__ << ": A SendSurface has been unregistered: "
                  << SendSurfaceStateToString(state);
  InvalidateSurfaceState();
  return StatusCodes::kOk;
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::RegisterReceiveSurface(
    TransferUpdateCallback* transfer_callback,
    ReceiveSurfaceState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transfer_callback);
  DCHECK_NE(state, ReceiveSurfaceState::kUnknown);

  if (is_sending_files_) {
    UnregisterReceiveSurface(transfer_callback);
    NS_LOG(VERBOSE)
        << __func__
        << ": Ignore registering (and unregistering if registered) receive "
           "surface, because we're currently sending files.";
    return StatusCodes::kTransferAlreadyInProgress;
  }

  // We specifically allow re-registring with out error so it is clear to caller
  // that the transfer_callback is currently registered.
  if (GetReceiveCallbacksFromState(state).HasObserver(transfer_callback)) {
    NS_LOG(VERBOSE) << __func__
                    << ": transfer callback already registered, ignoring";
    return StatusCodes::kOk;
  } else if (foreground_receive_callbacks_.HasObserver(transfer_callback) ||
             background_receive_callbacks_.HasObserver(transfer_callback)) {
    NS_LOG(ERROR)
        << __func__
        << ":  transfer callback already registered but for a different state.";
    return StatusCodes::kError;
  }

  // If the receive surface to be registered is a foreground surface, let it
  // catch up with most recent transfer metadata immediately.
  if (state == ReceiveSurfaceState::kForeground && last_incoming_metadata_) {
    transfer_callback->OnTransferUpdate(last_incoming_metadata_->first,
                                        last_incoming_metadata_->second);
  }

  GetReceiveCallbacksFromState(state).AddObserver(transfer_callback);

  NS_LOG(VERBOSE) << __func__ << ": A ReceiveSurface("
                  << ReceiveSurfaceStateToString(state)
                  << ") has been registered";
  InvalidateReceiveSurfaceState();
  return StatusCodes::kOk;
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::UnregisterReceiveSurface(
    TransferUpdateCallback* transfer_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(transfer_callback);
  bool is_foreground =
      foreground_receive_callbacks_.HasObserver(transfer_callback);
  bool is_background =
      background_receive_callbacks_.HasObserver(transfer_callback);
  if (!is_foreground && !is_background) {
    NS_LOG(VERBOSE)
        << __func__
        << ": Unknown transfer callback was un-registered, ignoring.";
    // We intentionally allow this be successful so the caller can be sure
    // they are not registered anymore.
    return StatusCodes::kOk;
  }

  if (foreground_receive_callbacks_.might_have_observers() &&
      last_incoming_metadata_ &&
      last_incoming_metadata_->second.is_final_status()) {
    // We already saw the final status in the foreground.
    // Nullify it so the next time the user opens sharing, it starts the UI from
    // the beginning
    last_incoming_metadata_.reset();
  }

  if (is_foreground) {
    foreground_receive_callbacks_.RemoveObserver(transfer_callback);
  } else {
    background_receive_callbacks_.RemoveObserver(transfer_callback);
  }

  // Displays the most recent payload status processed by foreground surfaces on
  // background surface.
  if (!foreground_receive_callbacks_.might_have_observers() &&
      last_incoming_metadata_) {
    for (TransferUpdateCallback& background_callback :
         background_receive_callbacks_) {
      background_callback.OnTransferUpdate(last_incoming_metadata_->first,
                                           last_incoming_metadata_->second);
    }
  }

  NS_LOG(VERBOSE) << __func__ << ": A ReceiveSurface("
                  << (is_foreground ? "foreground" : "background")
                  << ") has been unregistered";
  InvalidateSurfaceState();
  return StatusCodes::kOk;
}

bool NearbySharingServiceImpl::IsInHighVisibility() {
  return in_high_visibility;
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::SendAttachments(
    const ShareTarget& share_target,
    std::vector<std::unique_ptr<Attachment>> attachments) {
  if (!is_scanning_) {
    NS_LOG(WARNING) << __func__
                    << ": Failed to send attachments. Not scanning.";
    return StatusCodes::kError;
  }

  // |is_scanning_| means at least one send transfer callback.
  DCHECK(foreground_send_transfer_callbacks_.might_have_observers() ||
         background_send_transfer_callbacks_.might_have_observers());
  // |is_scanning_| and |is_transferring_| are mutually exclusive.
  DCHECK(!is_transferring_);

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->endpoint_id()) {
    // TODO(crbug.com/1119276): Support scanning for unknown share targets.
    NS_LOG(WARNING) << __func__
                    << ": Failed to send attachments. Unknown ShareTarget.";
    return StatusCodes::kError;
  }

  ShareTarget share_target_copy = share_target;
  for (std::unique_ptr<Attachment>& attachment : attachments) {
    DCHECK(attachment);
    attachment->MoveToShareTarget(share_target_copy);
  }

  if (!share_target_copy.has_attachments()) {
    NS_LOG(WARNING) << __func__ << ": No attachments to send.";
    return StatusCodes::kError;
  }

  // For sending advertisement from scanner, the request advertisement should
  // always be visible to everyone.
  base::Optional<std::vector<uint8_t>> endpoint_info =
      CreateEndpointInfo(local_device_data_manager_->GetDeviceName());
  if (!endpoint_info) {
    NS_LOG(WARNING) << __func__ << ": Could not create local endpoint info.";
    return StatusCodes::kError;
  }

  info->set_transfer_update_callback(std::make_unique<TransferUpdateDecorator>(
      base::BindRepeating(&NearbySharingServiceImpl::OnOutgoingTransferUpdate,
                          weak_ptr_factory_.GetWeakPtr())));

  OnTransferStarted(/*is_incoming=*/false);
  is_connecting_ = true;
  InvalidateSendSurfaceState();

  // Send process initialized successfully, from now on status updated will be
  // sent out via OnOutgoingTransferUpdate().
  info->transfer_update_callback()->OnTransferUpdate(
      share_target_copy, TransferMetadataBuilder()
                             .set_status(TransferMetadata::Status::kConnecting)
                             .build());

  CreatePayloads(std::move(share_target_copy),
                 base::BindOnce(&NearbySharingServiceImpl::OnCreatePayloads,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(*endpoint_info)));

  return StatusCodes::kOk;
}

void NearbySharingServiceImpl::Accept(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING) << __func__ << ": Accept invoked for unknown share target";
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  base::Optional<std::pair<ShareTarget, TransferMetadata>> metadata =
      share_target.is_incoming ? last_incoming_metadata_
                               : last_outgoing_metadata_;
  if (!metadata || metadata->second.status() !=
                       TransferMetadata::Status::kAwaitingLocalConfirmation) {
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  if (share_target.is_incoming) {
    ReceivePayloads(share_target, std::move(status_codes_callback));
    return;
  }

  std::move(status_codes_callback).Run(SendPayloads(share_target));
}

void NearbySharingServiceImpl::Reject(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING) << __func__ << ": Reject invoked for unknown share target";
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }
  NearbyConnection* connection = info->connection();

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NearbySharingServiceImpl::CloseConnection,
                     weak_ptr_factory_.GetWeakPtr(), share_target),
      kIncomingRejectionDelay);

  connection->SetDisconnectionListener(
      base::BindOnce(&NearbySharingServiceImpl::UnregisterShareTarget,
                     weak_ptr_factory_.GetWeakPtr(), share_target));

  WriteResponse(*connection, sharing::nearby::ConnectionResponseFrame::REJECT);
  NS_LOG(VERBOSE) << __func__
                  << ": Successfully wrote a rejection response frame";

  if (info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder()
                          .set_status(TransferMetadata::Status::kRejected)
                          .build());
  }

  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Cancel(
    const ShareTarget& share_target,
    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::Open(const ShareTarget& share_target,
                                    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

NearbyNotificationDelegate* NearbySharingServiceImpl::GetNotificationDelegate(
    const std::string& notification_id) {
  if (!nearby_notification_manager_)
    return nullptr;

  return nearby_notification_manager_->GetNotificationDelegate(notification_id);
}

NearbyShareSettings* NearbySharingServiceImpl::GetSettings() {
  return &settings_;
}

NearbyShareHttpNotifier* NearbySharingServiceImpl::GetHttpNotifier() {
  return &nearby_share_http_notifier_;
}

NearbyShareLocalDeviceDataManager*
NearbySharingServiceImpl::GetLocalDeviceDataManager() {
  return local_device_data_manager_.get();
}

NearbyShareContactManager* NearbySharingServiceImpl::GetContactManager() {
  return contact_manager_.get();
}

NearbyShareCertificateManager*
NearbySharingServiceImpl::GetCertificateManager() {
  return certificate_manager_.get();
}

void NearbySharingServiceImpl::OnNearbyProfileChanged(Profile* profile) {
  // TODO(crbug.com/1084576): Notify UI about the new active profile.
  if (profile) {
    NS_LOG(VERBOSE) << __func__ << ": Active Nearby profile changed to: "
                    << profile->GetProfileUserName();
  } else {
    NS_LOG(VERBOSE) << __func__ << ": Active Nearby profile cleared";
  }
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::OnNearbyProcessStarted() {
  DCHECK(profile_);
  if (process_manager_->IsActiveProfile(profile_)) {
    NS_LOG(VERBOSE) << __func__ << ": Nearby process started for profile: "
                    << profile_->GetProfileUserName();
  }
}

void NearbySharingServiceImpl::OnNearbyProcessStopped() {
  DCHECK(profile_);
  InvalidateSurfaceState();
  if (process_manager_->IsActiveProfile(profile_)) {
    NS_LOG(VERBOSE) << __func__ << ": Nearby process stopped for profile: "
                    << profile_->GetProfileUserName();
  }
}

void NearbySharingServiceImpl::OnIncomingConnection(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    NearbyConnection* connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection);
  DCHECK(profile_);
  ShareTarget placeholder_share_target;
  placeholder_share_target.is_incoming = true;
  ShareTargetInfo& share_target_info =
      GetOrCreateShareTargetInfo(placeholder_share_target, endpoint_id);
  share_target_info.set_connection(connection);

  connection->SetDisconnectionListener(
      base::BindOnce(&NearbySharingServiceImpl::RefreshUIOnDisconnection,
                     weak_ptr_factory_.GetWeakPtr(), placeholder_share_target));

  process_manager_->GetOrStartNearbySharingDecoder(profile_)
      ->DecodeAdvertisement(
          endpoint_info,
          base::BindOnce(
              &NearbySharingServiceImpl::OnIncomingAdvertisementDecoded,
              weak_ptr_factory_.GetWeakPtr(), endpoint_id,
              std::move(placeholder_share_target)));
}

void NearbySharingServiceImpl::FlushMojoForTesting() {
  settings_receiver_.FlushForTesting();
}

void NearbySharingServiceImpl::OnEnabledChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (enabled) {
    NS_LOG(VERBOSE) << __func__ << ": Nearby sharing enabled!";
    local_device_data_manager_->Start();
    contact_manager_->Start();
    certificate_manager_->Start();
  } else {
    NS_LOG(VERBOSE) << __func__ << ": Nearby sharing disabled!";
    StopAdvertising();
    StopScanning();
    nearby_connections_manager_->Shutdown();
    local_device_data_manager_->Stop();
    contact_manager_->Stop();
    certificate_manager_->Stop();
  }
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::OnDeviceNameChanged(
    const std::string& device_name) {
  NS_LOG(VERBOSE) << __func__ << ": Nearby sharing device name changed to "
                  << device_name;
  // TODO(vecore): handle device name change
}

void NearbySharingServiceImpl::OnDataUsageChanged(DataUsage data_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NS_LOG(VERBOSE) << __func__ << ": Nearby sharing data usage changed to "
                  << data_usage;

  if (advertising_power_level_ != PowerLevel::kUnknown)
    StopAdvertising();

  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::OnVisibilityChanged(Visibility new_visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NS_LOG(VERBOSE) << __func__ << ": Nearby sharing visibility changed to "
                  << new_visibility;
  if (advertising_power_level_ != PowerLevel::kUnknown)
    StopAdvertising();

  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::OnAllowedContactsChanged(
    const std::vector<std::string>& allowed_contacts) {
  NS_LOG(VERBOSE) << __func__ << ": Nearby sharing visible contacts changed";
  // TODO(vecore): handle visible contacts change
}

void NearbySharingServiceImpl::OnEndpointDiscovered(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(profile_);
  if (!is_scanning_) {
    NS_LOG(VERBOSE)
        << __func__
        << ": Ignoring discovered endpoint because we're no longer scanning";
    return;
  }

  process_manager_->GetOrStartNearbySharingDecoder(profile_)
      ->DecodeAdvertisement(
          endpoint_info,
          base::BindOnce(
              &NearbySharingServiceImpl::OnOutgoingAdvertisementDecoded,
              weak_ptr_factory_.GetWeakPtr(), endpoint_id));
}

void NearbySharingServiceImpl::OnEndpointLost(const std::string& endpoint_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_scanning_) {
    NS_LOG(VERBOSE)
        << __func__
        << ": Ignoring lost endpoint because we're no longer scanning";
    return;
  }

  // Remove the share target with this endpoint id.
  auto it = outgoing_share_target_map_.find(endpoint_id);
  if (it == outgoing_share_target_map_.end()) {
    NS_LOG(VERBOSE) << __func__
                    << ": Ignoring lost endpoint because we don't have an "
                       "associated ShareTarget";
    return;
  }

  ShareTarget share_target = std::move(it->second);
  outgoing_share_target_map_.erase(it);

  auto info_it = outgoing_share_target_info_map_.find(share_target.id);
  if (info_it != outgoing_share_target_info_map_.end()) {
    file_handler_.ReleaseFilePayloads(info_it->second.ExtractFilePayloads());
    outgoing_share_target_info_map_.erase(info_it);
  }

  for (ShareTargetDiscoveredCallback& discovery_callback :
       foreground_send_discovery_callbacks_) {
    discovery_callback.OnShareTargetLost(share_target);
  }
  for (ShareTargetDiscoveredCallback& discovery_callback :
       background_send_discovery_callbacks_) {
    discovery_callback.OnShareTargetLost(share_target);
  }

  NS_LOG(VERBOSE) << __func__ << ": Reported onShareTargetLost";
}

void NearbySharingServiceImpl::OnLockStateChanged(bool locked) {
  NS_LOG(VERBOSE) << __func__ << ": Screen lock state changed. (" << locked
                  << ")";
  is_screen_locked_ = locked;
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  NS_LOG(VERBOSE) << "Bluetooth present changed: " << present;
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  NS_LOG(VERBOSE) << "Bluetooth powered changed: " << powered;
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::SuspendImminent() {
  NS_LOG(VERBOSE) << __func__ << ": Suspend imminent.";
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::SuspendDone() {
  NS_LOG(VERBOSE) << __func__ << ": Suspend done.";
  InvalidateSurfaceState();
}

base::ObserverList<TransferUpdateCallback>&
NearbySharingServiceImpl::GetReceiveCallbacksFromState(
    ReceiveSurfaceState state) {
  switch (state) {
    case ReceiveSurfaceState::kForeground:
      return foreground_receive_callbacks_;
    case ReceiveSurfaceState::kBackground:
      return background_receive_callbacks_;
    case ReceiveSurfaceState::kUnknown:
      NOTREACHED();
      return foreground_receive_callbacks_;
  }
}

bool NearbySharingServiceImpl::IsVisibleInBackground(Visibility visibility) {
  return visibility == Visibility::kAllContacts ||
         visibility == Visibility::kSelectedContacts;
}

const base::Optional<std::vector<uint8_t>>
NearbySharingServiceImpl::CreateEndpointInfo(
    const base::Optional<std::string>& device_name) {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> encrypted_key;

  nearby_share::mojom::Visibility visibility = settings_.GetVisibility();
  if (visibility == Visibility::kAllContacts ||
      visibility == Visibility::kSelectedContacts) {
    base::Optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
        certificate_manager_->EncryptPrivateCertificateMetadataKey(visibility);
    if (encrypted_metadata_key) {
      salt = encrypted_metadata_key->salt();
      encrypted_key = encrypted_metadata_key->encrypted_key();
    }
  }

  if (salt.empty() || encrypted_key.empty()) {
    // Generate random metadata key.
    salt = GenerateRandomBytes(sharing::Advertisement::kSaltSize);
    encrypted_key = GenerateRandomBytes(
        sharing::Advertisement::kMetadataEncryptionKeyHashByteSize);
  }

  nearby_share::mojom::ShareTargetType device_type =
      nearby_share::mojom::ShareTargetType::kLaptop;

  std::unique_ptr<sharing::Advertisement> advertisement =
      sharing::Advertisement::NewInstance(
          std::move(salt), std::move(encrypted_key), device_type, device_name);
  if (advertisement) {
    return advertisement->ToEndpointInfo();
  } else {
    return base::nullopt;
  }
}

void NearbySharingServiceImpl::GetBluetoothAdapter() {
  auto* adapter_factory = device::BluetoothAdapterFactory::Get();
  if (!adapter_factory->IsBluetoothSupported())
    return;

  // Because this will be called from the constructor, GetAdapter() may call
  // OnGetBluetoothAdapter() immediately which can cause problems during tests
  // since the class is not fully constructed yet.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &device::BluetoothAdapterFactory::GetAdapter,
          base::Unretained(adapter_factory),
          base::BindOnce(&NearbySharingServiceImpl::OnGetBluetoothAdapter,
                         weak_ptr_factory_.GetWeakPtr())));
}

void NearbySharingServiceImpl::OnGetBluetoothAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  bluetooth_adapter_->AddObserver(this);

  // TODO(crbug.com/1132469): This was added to fix an issue where advertising
  // was not starting on sign-in. Add a unit test to cover this case.
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::StartFastInitiationAdvertising() {
  fast_initiation_manager_ =
      FastInitiationManager::Factory::Create(bluetooth_adapter_);

  // TODO(crbug.com/1100686): Determine whether to call StartAdvertising() with
  // kNotify or kSilent.
  fast_initiation_manager_->StartAdvertising(
      FastInitiationManager::FastInitType::kNotify,
      base::BindOnce(
          &NearbySharingServiceImpl::OnStartFastInitiationAdvertising,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &NearbySharingServiceImpl::OnStartFastInitiationAdvertisingError,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::OnStartFastInitiationAdvertising() {
  // TODO(hansenmichael): Do not invoke
  // |register_send_surface_callback_| until Nearby Connections
  // scanning is kicked off.
  NS_LOG(VERBOSE) << "Started advertising FastInitiation.";
}

void NearbySharingServiceImpl::OnStartFastInitiationAdvertisingError() {
  fast_initiation_manager_.reset();
  NS_LOG(ERROR) << "Failed to start FastInitiation advertising.";
}

void NearbySharingServiceImpl::StopFastInitiationAdvertising() {
  if (!fast_initiation_manager_) {
    NS_LOG(VERBOSE)
        << "Can't stop advertising FastInitiation. Not advertising.";
    return;
  }

  fast_initiation_manager_->StopAdvertising(
      base::BindOnce(&NearbySharingServiceImpl::OnStopFastInitiationAdvertising,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::OnStopFastInitiationAdvertising() {
  fast_initiation_manager_.reset();
  NS_LOG(VERBOSE) << "Stopped advertising FastInitiation";
}

void NearbySharingServiceImpl::OnOutgoingAdvertisementDecoded(
    const std::string& endpoint_id,
    sharing::mojom::AdvertisementPtr advertisement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!advertisement) {
    NS_LOG(VERBOSE) << __func__
                    << ": Failed to parse discovered advertisement.";
    return;
  }

  // Now we will report endpoints met before in NearbyConnectionsManager.
  // Check outgoingShareTargetInfoMap first and pass the same shareTarget if we
  // found one.

  // Looking for the ShareTarget based on endpoint id.
  if (outgoing_share_target_map_.find(endpoint_id) !=
      outgoing_share_target_map_.end()) {
    return;
  }

  // Once we get the advertisement, the first thing to do is decrypt the
  // certificate.
  NearbyShareEncryptedMetadataKey encrypted_metadata_key(
      advertisement->salt, advertisement->encrypted_metadata_key);
  GetCertificateManager()->GetDecryptedPublicCertificate(
      std::move(encrypted_metadata_key),
      base::BindOnce(&NearbySharingServiceImpl::OnOutgoingDecryptedCertificate,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id,
                     std::move(advertisement)));
}

void NearbySharingServiceImpl::OnOutgoingDecryptedCertificate(
    const std::string& endpoint_id,
    sharing::mojom::AdvertisementPtr advertisement,
    base::Optional<NearbyShareDecryptedPublicCertificate> certificate) {
  // Check again for this endpoint id, to avoid race conditions.
  if (outgoing_share_target_map_.find(endpoint_id) !=
      outgoing_share_target_map_.end()) {
    return;
  }

  // The certificate provides the device name, in order to create a ShareTarget
  // to represent this remote device.
  base::Optional<ShareTarget> share_target = CreateShareTarget(
      endpoint_id, std::move(advertisement), std::move(certificate),
      /*is_incoming=*/false);
  if (!share_target) {
    NS_LOG(VERBOSE) << __func__
                    << ": Failed to convert advertisement to share target from "
                       "discovered advertisement. Ignoring endpoint.";
    return;
  }

  // Update the endpoint id for the share target.
  NS_LOG(VERBOSE) << __func__
                  << ": An endpoint has been discovered, with an advertisement "
                     "containing a valid share target.";

  // Notifies the user that we discovered a device.
  for (ShareTargetDiscoveredCallback& discovery_callback :
       foreground_send_discovery_callbacks_) {
    discovery_callback.OnShareTargetDiscovered(*share_target);
  }
  for (ShareTargetDiscoveredCallback& discovery_callback :
       background_send_discovery_callbacks_) {
    discovery_callback.OnShareTargetDiscovered(*share_target);
  }

  NS_LOG(VERBOSE) << __func__ << ": Reported OnShareTargetDiscovered "
                  << (base::Time::Now() - scanning_start_timestamp_);

  // TODO(crbug/1108348) CachingManager should cache known and non-external
  // share targets.
}

bool NearbySharingServiceImpl::IsBluetoothPresent() const {
  return bluetooth_adapter_.get() && bluetooth_adapter_->IsPresent();
}

bool NearbySharingServiceImpl::IsBluetoothPowered() const {
  return IsBluetoothPresent() && bluetooth_adapter_->IsPowered();
}

bool NearbySharingServiceImpl::HasAvailableConnectionMediums() {
  // Check if Wifi or Ethernet LAN is off.  Advertisements won't work, so
  // disable them, unless bluetooth is known to be enabled. Not all platforms
  // have bluetooth, so wifi LAN is a platform-agnostic check.
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  bool hasNetworkConnection =
      connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI ||
      connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET;
  return IsBluetoothPowered() || (kIsWifiLanSupported && hasNetworkConnection);
}

void NearbySharingServiceImpl::InvalidateSurfaceState() {
  InvalidateSendSurfaceState();
  InvalidateReceiveSurfaceState();
  if (ShouldStopNearbyProcess()) {
    NS_LOG(VERBOSE) << __func__ << ": Stopping process because it's not in use";
    process_manager_->StopProcess(profile_);
  }
}

bool NearbySharingServiceImpl::ShouldStopNearbyProcess() {
  // Nothing to do if we're shutting down the profile.
  if (!profile_)
    return false;

  // Cannot stop process without being the active profile.
  if (!process_manager_->IsActiveProfile(profile_))
    return false;

  // We're currently advertising.
  if (advertising_power_level_ != PowerLevel::kUnknown)
    return false;

  // We're currently discovering.
  if (is_scanning_)
    return false;

  // We're currently attempting to connect to a remote device.
  if (is_connecting_)
    return false;

  // We're currently sending or receiving a file.
  if (is_transferring_)
    return false;

  // We're not using NearbyConnections, should stop the process.
  return true;
}

void NearbySharingServiceImpl::InvalidateSendSurfaceState() {
  InvalidateScanningState();
  InvalidateFastInitiationAdvertising();
}

void NearbySharingServiceImpl::InvalidateScanningState() {
  // Nothing to do if we're shutting down the profile.
  if (!profile_)
    return;

  if (power_client_->IsSuspended()) {
    StopScanning();
    NS_LOG(VERBOSE) << __func__
                    << ": Stopping discovery because the system is suspended.";
    return;
  }

  if (!process_manager_->IsActiveProfile(profile_)) {
    NS_LOG(VERBOSE) << __func__
                    << ": Stopping discovery because profile was not active: "
                    << profile_->GetProfileUserName();
    StopScanning();
    return;
  }

  // Screen is off. Do no work.
  if (is_screen_locked_) {
    StopScanning();
    NS_LOG(VERBOSE) << __func__
                    << ": Stopping discovery because the screen is locked.";
    return;
  }

  if (!HasAvailableConnectionMediums()) {
    StopScanning();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping scanning because both bluetooth and wifi LAN are "
           "disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't advertise.
  if (!settings_.GetEnabled()) {
    StopScanning();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping discovery because Nearby Sharing is disabled.";
    return;
  }

  if (is_transferring_ || is_connecting_) {
    StopScanning();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping discovery because we're currently in the midst of a "
           "transfer.";
    return;
  }

  if (!foreground_send_transfer_callbacks_.might_have_observers()) {
    StopScanning();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping discovery because no scanning surface has been "
           "registered.";
    return;
  }

  // Screen is on, Bluetooth is enabled, and Nearby Sharing is enabled! Start
  // discovery.
  StartScanning();
}

void NearbySharingServiceImpl::InvalidateFastInitiationAdvertising() {
  // Nothing to do if we're shutting down the profile.
  if (!profile_)
    return;

  if (power_client_->IsSuspended()) {
    StopFastInitiationAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping fast init advertising because the system is suspended.";
    return;
  }

  if (!process_manager_->IsActiveProfile(profile_)) {
    StopFastInitiationAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping fast init advertising because profile was not active: "
        << profile_->GetProfileUserName();
    return;
  }

  // Screen is off. Do no work.
  if (is_screen_locked_) {
    StopFastInitiationAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping fast init advertising because the screen is locked.";
    return;
  }

  if (!IsBluetoothPowered()) {
    StopFastInitiationAdvertising();
    NS_LOG(VERBOSE) << __func__
                    << ": Stopping fast init advertising because both "
                       "bluetooth is disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't fast init advertise.
  if (!settings_.GetEnabled()) {
    StopFastInitiationAdvertising();
    NS_LOG(VERBOSE) << __func__
                    << ": Stopping fast init advertising because Nearby "
                       "Sharing is disabled.";
    return;
  }

  if (!foreground_send_transfer_callbacks_.might_have_observers()) {
    StopFastInitiationAdvertising();
    NS_LOG(VERBOSE) << __func__
                    << ": Stopping fast init advertising because no send "
                       "surface is registered.";
    return;
  }

  if (fast_initiation_manager_) {
    NS_LOG(VERBOSE)
        << "Failed to advertise FastInitiation. Already advertising.";
    return;
  }

  NS_LOG(VERBOSE) << __func__ << ": Starting fast init advertising.";

  StartFastInitiationAdvertising();
}

void NearbySharingServiceImpl::InvalidateReceiveSurfaceState() {
  InvalidateAdvertisingState();
  // TODO(b/161889067) InvalidateFastInitScan();
}

void NearbySharingServiceImpl::InvalidateAdvertisingState() {
  // Nothing to do if we're shutting down the profile.
  if (!profile_)
    return;

  if (power_client_->IsSuspended()) {
    StopAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping advertising because the system is suspended.";
    return;
  }

  if (!process_manager_->IsActiveProfile(profile_)) {
    NS_LOG(VERBOSE) << __func__
                    << ": Stopping advertising because profile was not active: "
                    << profile_->GetProfileUserName();
    StopAdvertising();
    return;
  }

  // Screen is off. Do no work.
  if (is_screen_locked_) {
    StopAdvertising();
    NS_LOG(VERBOSE) << __func__
                    << ": Stopping advertising because the screen is locked.";
    return;
  }

  if (!HasAvailableConnectionMediums()) {
    StopAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping advertising because both bluetooth and wifi LAN are "
           "disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't advertise.
  if (!settings_.GetEnabled()) {
    StopAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping advertising because Nearby Sharing is disabled.";
    return;
  }

  // We're scanning for other nearby devices. Don't advertise.
  if (is_scanning_) {
    StopAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping advertising because we're scanning for other devices.";
    return;
  }

  if (is_transferring_) {
    StopAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping advertising because we're currently in the midst of "
           "a transfer.";
    return;
  }

  if (!foreground_receive_callbacks_.might_have_observers() &&
      !background_receive_callbacks_.might_have_observers()) {
    StopAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping advertising because no receive surface is registered.";
    return;
  }

  if (!IsVisibleInBackground(settings_.GetVisibility()) &&
      !foreground_receive_callbacks_.might_have_observers()) {
    StopAdvertising();
    NS_LOG(VERBOSE)
        << __func__
        << ": Stopping advertising because no high power receive surface "
           "is registered and device is visible to NO_ONE.";
    return;
  }

  PowerLevel power_level;
  if (foreground_receive_callbacks_.might_have_observers()) {
    power_level = PowerLevel::kHighPower;
    // TODO(crbug/1100367) handle fast init
    // } else if (isFastInitDeviceNearby) {
    //   power_level = PowerLevel::kMediumPower;
  } else {
    power_level = PowerLevel::kLowPower;
  }

  DataUsage data_usage = settings_.GetDataUsage();
  if (advertising_power_level_ != PowerLevel::kUnknown) {
    if (power_level == advertising_power_level_) {
      NS_LOG(VERBOSE)
          << __func__
          << ": Failed to advertise because we're already advertising with "
             "power level "
          << PowerLevelToString(advertising_power_level_)
          << " and data usage preference " << data_usage;
      return;
    }

    StopAdvertising();
    NS_LOG(VERBOSE) << __func__ << ": Restart advertising with power level "
                    << PowerLevelToString(power_level)
                    << " and data usage preference " << data_usage;
  }

  base::Optional<std::string> device_name;
  if (foreground_receive_callbacks_.might_have_observers())
    device_name = local_device_data_manager_->GetDeviceName();

  // Starts advertising through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopAdvertising is called.
  base::Optional<std::vector<uint8_t>> endpoint_info =
      CreateEndpointInfo(device_name);
  if (!endpoint_info) {
    NS_LOG(VERBOSE) << __func__
                    << ": Unable to advertise since could not parse the "
                       "endpoint info from the advertisement.";
    return;
  }

  nearby_connections_manager_->StartAdvertising(
      *endpoint_info,
      /* listener= */ this, power_level, data_usage,
      base::BindOnce(&NearbySharingServiceImpl::OnStartAdvertisingResult,
                     weak_ptr_factory_.GetWeakPtr(), device_name.has_value()));

  advertising_power_level_ = power_level;
  NS_LOG(VERBOSE) << __func__
                  << ": StartAdvertising requested over Nearby Connections: "
                  << " power level: " << PowerLevelToString(power_level)
                  << " visibility: " << settings_.GetVisibility()
                  << " data usage: " << data_usage << " device name: "
                  << device_name.value_or("** no device name **");

  ScheduleRotateBackgroundAdvertisementTimer();
}

void NearbySharingServiceImpl::StopAdvertising() {
  SetInHighVisibility(false);
  if (advertising_power_level_ == PowerLevel::kUnknown) {
    NS_LOG(VERBOSE)
        << __func__
        << ": Failed to stop advertising because we weren't advertising";
    return;
  }

  nearby_connections_manager_->StopAdvertising();
  advertising_power_level_ = PowerLevel::kUnknown;
  NS_LOG(VERBOSE) << __func__ << ": Advertising has stopped";
}

void NearbySharingServiceImpl::StartScanning() {
  DCHECK(profile_);
  DCHECK(!power_client_->IsSuspended());
  DCHECK(settings_.GetEnabled());
  DCHECK(!is_screen_locked_);
  DCHECK(HasAvailableConnectionMediums());
  DCHECK(foreground_send_transfer_callbacks_.might_have_observers());

  if (is_scanning_) {
    NS_LOG(VERBOSE) << __func__
                    << ": Failed to scan because we're currently scanning.";
    return;
  }

  scanning_start_timestamp_ = base::Time::Now();
  is_scanning_ = true;
  InvalidateReceiveSurfaceState();

  ClearOutgoingShareTargetInfoMap();

  nearby_connections_manager_->StartDiscovery(
      /* listener= */ this,
      base::BindOnce([](NearbyConnectionsManager::ConnectionsStatus status) {
        NS_LOG(VERBOSE) << __func__
                        << ": Scanning start attempted over Nearby Connections "
                           "with result "
                        << status;
      }));

  InvalidateSendSurfaceState();
  NS_LOG(VERBOSE) << __func__ << ": Scanning has started";
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::StopScanning() {
  if (!is_scanning_) {
    NS_LOG(VERBOSE) << __func__
                    << ": Failed to stop scanning because weren't scanning.";
    return StatusCodes::kStatusAlreadyStopped;
  }

  nearby_connections_manager_->StopDiscovery();
  is_scanning_ = false;

  // Note: We don't know if we stopped scanning in preparation to send a file,
  // or we stopped because the user left the page. We'll invalidate after a
  // short delay.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NearbySharingServiceImpl::InvalidateSurfaceState,
                     weak_ptr_factory_.GetWeakPtr()),
      kInvalidateDelay);

  NS_LOG(VERBOSE) << __func__ << ": Scanning has stopped.";
  return StatusCodes::kOk;
}

void NearbySharingServiceImpl::ScheduleRotateBackgroundAdvertisementTimer() {
  uint64_t delayRangeMilliseconds = base::checked_cast<uint64_t>(
      kBackgroundAdvertisementRotationDelayMax.InMilliseconds() -
      kBackgroundAdvertisementRotationDelayMin.InMilliseconds());
  uint64_t delayMilliseconds =
      base::RandGenerator(delayRangeMilliseconds) +
      base::checked_cast<uint64_t>(
          kBackgroundAdvertisementRotationDelayMin.InMilliseconds());
  rotate_background_advertisement_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(
          base::checked_cast<uint64_t>(delayMilliseconds)),
      base::BindOnce(
          &NearbySharingServiceImpl::OnRotateBackgroundAdvertisementTimerFired,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::OnRotateBackgroundAdvertisementTimerFired() {
  if (foreground_receive_callbacks_.might_have_observers()) {
    ScheduleRotateBackgroundAdvertisementTimer();
  } else {
    StopAdvertising();
    InvalidateSurfaceState();
  }
}

void NearbySharingServiceImpl::OnTransferComplete() {
  is_receiving_files_ = false;
  is_transferring_ = false;
  is_sending_files_ = false;

  NS_LOG(VERBOSE) << __func__
                  << ": NearbySharing state change transfer finished";
  // TODO(crbug.com/1123167): Check if we need to delay InvalidateSurfaceState()
  // for 500ms similar to GmsCore impl.
  // Post a task as InvalidateSurfaceState() might invalidate ShareTargetInfo
  // object that are still in use.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&NearbySharingServiceImpl::InvalidateSurfaceState,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::OnTransferStarted(bool is_incoming) {
  is_transferring_ = true;
  if (is_incoming) {
    is_receiving_files_ = true;
  } else {
    is_sending_files_ = true;
  }
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::ReceivePayloads(
    ShareTarget share_target,
    StatusCodesCallback status_codes_callback) {
  DCHECK(profile_);
  mutual_acceptance_timeout_alarm_.Cancel();

  base::FilePath download_path =
      DownloadPrefs::FromDownloadManager(
          content::BrowserContext::GetDownloadManager(profile_))
          ->DownloadPath();

  // Register payload path for all valid file payloads.
  base::flat_map<int64_t, base::FilePath> valid_file_payloads;
  for (auto& file : share_target.file_attachments) {
    base::Optional<int64_t> payload_id = GetAttachmentPayloadId(file.id());
    if (!payload_id) {
      NS_LOG(WARNING)
          << __func__
          << ": Failed to register payload path for attachment id - "
          << file.id();
      continue;
    }

    base::FilePath file_path = download_path.AppendASCII(file.file_name());
    valid_file_payloads.emplace(file.id(), std::move(file_path));
  }

  auto aggregated_success = std::make_unique<bool>(true);
  bool* aggregated_success_ptr = aggregated_success.get();

  if (valid_file_payloads.empty()) {
    OnPayloadPathsRegistered(share_target, std::move(aggregated_success),
                             std::move(status_codes_callback));
    return;
  }

  auto all_paths_registered_callback = base::BarrierClosure(
      valid_file_payloads.size(),
      base::BindOnce(&NearbySharingServiceImpl::OnPayloadPathsRegistered,
                     weak_ptr_factory_.GetWeakPtr(), share_target,
                     std::move(aggregated_success),
                     std::move(status_codes_callback)));

  for (const auto& payload : valid_file_payloads) {
    base::Optional<int64_t> payload_id = GetAttachmentPayloadId(payload.first);
    DCHECK(payload_id);

    file_handler_.GetUniquePath(
        payload.second,
        base::BindOnce(
            &NearbySharingServiceImpl::OnUniquePathFetched,
            weak_ptr_factory_.GetWeakPtr(), payload.first, *payload_id,
            base::BindOnce(
                &NearbySharingServiceImpl::OnPayloadPathRegistered,
                weak_ptr_factory_.GetWeakPtr(),
                base::ScopedClosureRunner(all_paths_registered_callback),
                aggregated_success_ptr)));
  }
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::SendPayloads(
    const ShareTarget& share_target) {
  NS_LOG(VERBOSE) << __func__ << ": Preparing to send payloads to "
                  << share_target.device_name;
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING) << "Failed to send payload due to missing connection.";
    return StatusCodes::kOutOfOrderApiCall;
  }
  if (!info->transfer_update_callback()) {
    NS_LOG(WARNING) << "Failed to send payload due to missing transfer update "
                       "callback. Disconnecting.";
    info->connection()->Close();
    return StatusCodes::kOutOfOrderApiCall;
  }

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_token(info->token())
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .build());

  if (!info->endpoint_id()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder()
                          .set_status(TransferMetadata::Status::kFailed)
                          .build());
    info->connection()->Close();
    NS_LOG(WARNING) << "Failed to send payload due to missing endpoint id.";
    return StatusCodes::kOutOfOrderApiCall;
  }

  ReceiveConnectionResponse(share_target);
  return StatusCodes::kOk;
}

void NearbySharingServiceImpl::OnUniquePathFetched(
    int64_t attachment_id,
    int64_t payload_id,
    base::OnceCallback<void(location::nearby::connections::mojom::Status)>
        callback,
    base::FilePath path) {
  attachment_info_map_[attachment_id].file_path = path;
  nearby_connections_manager_->RegisterPayloadPath(payload_id, path,
                                                   std::move(callback));
}

void NearbySharingServiceImpl::OnPayloadPathRegistered(
    base::ScopedClosureRunner closure_runner,
    bool* aggregated_success,
    location::nearby::connections::mojom::Status status) {
  if (status != location::nearby::connections::mojom::Status::kSuccess)
    *aggregated_success = false;
}

void NearbySharingServiceImpl::OnPayloadPathsRegistered(
    const ShareTarget& share_target,
    std::unique_ptr<bool> aggregated_success,
    StatusCodesCallback status_codes_callback) {
  DCHECK(aggregated_success);
  if (!*aggregated_success) {
    NS_LOG(WARNING)
        << __func__
        << ": Not all payload paths could be registered successfully.";
    std::move(status_codes_callback).Run(StatusCodes::kError);
    return;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING) << __func__ << ": Accept invoked for unknown share target";
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    NS_LOG(WARNING) << __func__
                    << ": Accept invoked for share target without transfer "
                       "update callback. Disconnecting.";
    connection->Close();
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  info->set_payload_tracker(std::make_unique<PayloadTracker>(
      share_target, attachment_info_map_,
      base::BindRepeating(&NearbySharingServiceImpl::OnPayloadTransferUpdate,
                          weak_ptr_factory_.GetWeakPtr())));

  // Register status listener for all payloads.
  for (int64_t attachment_id : share_target.GetAttachmentIds()) {
    base::Optional<int64_t> payload_id = GetAttachmentPayloadId(attachment_id);
    if (!payload_id) {
      NS_LOG(WARNING) << __func__
                      << ": Failed to retrieve payload for attachment id - "
                      << attachment_id;
      continue;
    }

    NS_LOG(VERBOSE) << __func__
                    << ": Started listening for progress on payload - "
                    << *payload_id;

    nearby_connections_manager_->RegisterPayloadStatusListener(
        *payload_id, info->payload_tracker());

    NS_LOG(VERBOSE) << __func__
                    << ": Accepted incoming files from share target - "
                    << share_target.device_name;
  }

  WriteResponse(*connection, sharing::nearby::ConnectionResponseFrame::ACCEPT);
  NS_LOG(VERBOSE) << __func__ << ": Successfully wrote response frame";

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .set_token(info->token())
          .build());

  base::Optional<std::string> endpoint_id = info->endpoint_id();
  if (endpoint_id) {
    nearby_connections_manager_->UpgradeBandwidth(*endpoint_id);
  } else {
    NS_LOG(WARNING) << __func__
                    << ": Failed to initiate bandwidth upgrade. No endpoint_id "
                       "found for target - "
                    << share_target.device_name;
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::OnOutgoingConnection(
    const ShareTarget& share_target,
    NearbyConnection* connection) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (!info || !info->endpoint_id() || !connection) {
    NS_LOG(WARNING) << __func__
                    << ": Failed to initate connection to share target "
                    << share_target.device_name;
    if (info->transfer_update_callback()) {
      info->transfer_update_callback()->OnTransferUpdate(
          share_target, TransferMetadataBuilder()
                            .set_status(TransferMetadata::Status::kFailed)
                            .build());
    }
    return;
  }

  info->set_connection(connection);

  connection->SetDisconnectionListener(base::BindOnce(
      &NearbySharingServiceImpl::OnOutgoingConnectionDisconnected,
      weak_ptr_factory_.GetWeakPtr(), share_target));

  base::Optional<std::string> four_digit_token =
      ToFourDigitString(nearby_connections_manager_->GetRawAuthenticationToken(
          *info->endpoint_id()));

  RunPairedKeyVerification(
      share_target, *info->endpoint_id(),
      base::BindOnce(
          &NearbySharingServiceImpl::OnOutgoingConnectionKeyVerificationDone,
          weak_ptr_factory_.GetWeakPtr(), share_target,
          std::move(four_digit_token)));
}

void NearbySharingServiceImpl::SendIntroduction(
    const ShareTarget& share_target,
    base::Optional<std::string> four_digit_token) {
  // We successfully connected! Now lets build up Payloads for all the files we
  // want to send them. We won't send any just yet, but we'll send the Payload
  // IDs in our our introduction frame so that they know what to expect if they
  // accept.
  NS_LOG(VERBOSE) << __func__ << ": Preparing to send introduction to "
                  << share_target.device_name;

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING) << __func__ << ": No NearbyConnection tied to "
                    << share_target.device_name;
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    connection->Close();
    NS_LOG(WARNING) << __func__
                    << ": No transfer update callback, disconnecting.";
    return;
  }

  if (!foreground_send_transfer_callbacks_.might_have_observers() &&
      !background_send_transfer_callbacks_.might_have_observers()) {
    connection->Close();
    NS_LOG(WARNING) << __func__ << ": No transfer callbacks, disconnecting.";
    return;
  }

  // Build the introduction.
  auto introduction = std::make_unique<sharing::nearby::IntroductionFrame>();
  NS_LOG(VERBOSE) << __func__ << ": Sending attachments to "
                  << share_target.device_name;

  // Write introduction of file payloads.
  for (const auto& file : share_target.file_attachments) {
    base::Optional<int64_t> payload_id = GetAttachmentPayloadId(file.id());
    if (!payload_id) {
      NS_LOG(VERBOSE) << __func__ << ": Skipping unknown file attachment";
      continue;
    }
    auto* file_metadata = introduction->add_file_metadata();
    file_metadata->set_id(file.id());
    file_metadata->set_name(file.file_name());
    file_metadata->set_payload_id(*payload_id);
    file_metadata->set_type(sharing::ConvertFileMetadataType(file.type()));
    file_metadata->set_mime_type(file.mime_type());
    file_metadata->set_size(file.size());
  }

  // Write introduction of text payloads.
  for (const auto& text : share_target.text_attachments) {
    base::Optional<int64_t> payload_id = GetAttachmentPayloadId(text.id());
    if (!payload_id) {
      NS_LOG(VERBOSE) << __func__ << ": Skipping unknown text attachment";
      continue;
    }
    auto* text_metadata = introduction->add_text_metadata();
    text_metadata->set_id(text.id());
    text_metadata->set_text_title(text.text_title());
    text_metadata->set_type(sharing::ConvertTextMetadataType(text.type()));
    text_metadata->set_size(text.size());
    text_metadata->set_payload_id(*payload_id);
  }

  if (introduction->file_metadata_size() == 0 &&
      introduction->text_metadata_size() == 0) {
    NS_LOG(WARNING) << __func__
                    << ": No payloads tied to transfer, disconnecting.";
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder()
                          .set_status(TransferMetadata::Status::kFailed)
                          .build());
    connection->Close();
    return;
  }

  // Write the introduction to the remote device.
  sharing::nearby::Frame frame;
  frame.set_version(sharing::nearby::Frame::V1);
  sharing::nearby::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(sharing::nearby::V1Frame::INTRODUCTION);
  v1_frame->set_allocated_introduction(introduction.release());

  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());
  connection->Write(std::move(data));

  // We've successfully written the introduction, so we now have to wait for the
  // remote side to accept.
  NS_LOG(VERBOSE) << __func__ << ": Successfully wrote the introduction frame";

  mutual_acceptance_timeout_alarm_.Reset(base::BindOnce(
      &NearbySharingServiceImpl::OnOutgoingMutualAcceptanceTimeout,
      weak_ptr_factory_.GetWeakPtr(), share_target));

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(mutual_acceptance_timeout_alarm_.callback()),
      kReadResponseFrameTimeout);

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .set_token(four_digit_token)
          .build());
}

void NearbySharingServiceImpl::CreatePayloads(
    ShareTarget share_target,
    base::OnceCallback<void(ShareTarget, bool)> callback) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (!info || !share_target.has_attachments()) {
    std::move(callback).Run(std::move(share_target), /*success=*/false);
    return;
  }

  if (!info->file_payloads().empty() || !info->text_payloads().empty()) {
    // We may have already created the payloads in the case of retry, so we can
    // skip this step.
    std::move(callback).Run(std::move(share_target), /*success=*/false);
    return;
  }

  info->set_text_payloads(CreateTextPayloads(share_target.text_attachments));
  if (share_target.file_attachments.empty()) {
    std::move(callback).Run(std::move(share_target), /*success=*/true);
    return;
  }

  std::vector<base::FilePath> file_paths;
  for (const FileAttachment& attachment : share_target.file_attachments) {
    if (!attachment.file_path()) {
      NS_LOG(WARNING) << __func__ << ": Got file attachment without path";
      std::move(callback).Run(std::move(share_target), /*success=*/false);
      return;
    }
    file_paths.push_back(*attachment.file_path());
  }

  file_handler_.OpenFiles(
      std::move(file_paths),
      base::BindOnce(&NearbySharingServiceImpl::OnOpenFiles,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target),
                     std::move(callback)));
}

void NearbySharingServiceImpl::OnCreatePayloads(
    std::vector<uint8_t> endpoint_info,
    ShareTarget share_target,
    bool success) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  bool has_payloads = info && (!info->text_payloads().empty() ||
                               !info->file_payloads().empty());
  if (!success || !has_payloads || !info->endpoint_id()) {
    NS_LOG(WARNING) << __func__
                    << ": Failed to send file to remote ShareTarget. Failed to "
                       "create payloads.";
    if (info && info->transfer_update_callback()) {
      info->transfer_update_callback()->OnTransferUpdate(
          share_target,
          TransferMetadataBuilder()
              .set_status(TransferMetadata::Status::kMediaUnavailable)
              .build());
    }
    return;
  }

  base::Optional<std::vector<uint8_t>> bluetooth_mac_address =
      GetBluetoothMacAddress(share_target);

  DataUsage adjusted_data_usage = CheckFileSizeForDataUsagePreference(
      settings_.GetDataUsage(), share_target);

  // TODO(crbug.com/1111458): Add preferred transfer type.
  nearby_connections_manager_->Connect(
      std::move(endpoint_info), *info->endpoint_id(),
      std::move(bluetooth_mac_address), adjusted_data_usage,
      base::BindOnce(&NearbySharingServiceImpl::OnOutgoingConnection,
                     weak_ptr_factory_.GetWeakPtr(), share_target));
}

void NearbySharingServiceImpl::OnOpenFiles(
    ShareTarget share_target,
    base::OnceCallback<void(ShareTarget, bool)> callback,
    std::vector<NearbyFileHandler::FileInfo> files) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (!info || files.size() != share_target.file_attachments.size()) {
    std::move(callback).Run(std::move(share_target), /*success=*/false);
    return;
  }

  std::vector<location::nearby::connections::mojom::PayloadPtr> payloads;
  payloads.reserve(files.size());

  for (size_t i = 0; i < files.size(); ++i) {
    FileAttachment& attachment = share_target.file_attachments[i];
    attachment.set_size(files[i].size);
    base::File& file = files[i].file;
    int64_t payload_id = GeneratePayloadId();
    SetAttachmentPayloadId(attachment, payload_id);
    payloads.push_back(location::nearby::connections::mojom::Payload::New(
        payload_id,
        location::nearby::connections::mojom::PayloadContent::NewFile(
            location::nearby::connections::mojom::FilePayload::New(
                std::move(file)))));
  }

  info->set_file_payloads(std::move(payloads));
  std::move(callback).Run(std::move(share_target), /*success=*/true);
}

std::vector<location::nearby::connections::mojom::PayloadPtr>
NearbySharingServiceImpl::CreateTextPayloads(
    const std::vector<TextAttachment>& attachments) {
  std::vector<location::nearby::connections::mojom::PayloadPtr> payloads;
  payloads.reserve(attachments.size());
  for (const TextAttachment& attachment : attachments) {
    const std::string& body = attachment.text_body();
    std::vector<uint8_t> bytes(body.begin(), body.end());

    int64_t payload_id = GeneratePayloadId();
    SetAttachmentPayloadId(attachment, payload_id);
    payloads.push_back(location::nearby::connections::mojom::Payload::New(
        payload_id,
        location::nearby::connections::mojom::PayloadContent::NewBytes(
            location::nearby::connections::mojom::BytesPayload::New(
                std::move(bytes)))));
  }
  return payloads;
}

void NearbySharingServiceImpl::WriteResponse(
    NearbyConnection& connection,
    sharing::nearby::ConnectionResponseFrame::Status status) {
  sharing::nearby::Frame frame;
  frame.set_version(sharing::nearby::Frame::V1);
  sharing::nearby::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(sharing::nearby::V1Frame::RESPONSE);
  v1_frame->mutable_connection_response()->set_status(status);

  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());

  connection.Write(std::move(data));
}

void NearbySharingServiceImpl::Fail(const ShareTarget& share_target,
                                    TransferMetadata::Status status) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING) << __func__ << ": Fail invoked for unknown share target.";
    return;
  }
  NearbyConnection* connection = info->connection();

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NearbySharingServiceImpl::CloseConnection,
                     weak_ptr_factory_.GetWeakPtr(), share_target),
      kIncomingRejectionDelay);

  connection->SetDisconnectionListener(
      base::BindOnce(&NearbySharingServiceImpl::UnregisterShareTarget,
                     weak_ptr_factory_.GetWeakPtr(), share_target));

  // Send response to remote device.
  sharing::nearby::ConnectionResponseFrame::Status response_status;
  switch (status) {
    case TransferMetadata::Status::kNotEnoughSpace:
      response_status =
          sharing::nearby::ConnectionResponseFrame::NOT_ENOUGH_SPACE;
      break;

    case TransferMetadata::Status::kUnsupportedAttachmentType:
      response_status =
          sharing::nearby::ConnectionResponseFrame::UNSUPPORTED_ATTACHMENT_TYPE;
      break;

    case TransferMetadata::Status::kTimedOut:
      response_status = sharing::nearby::ConnectionResponseFrame::TIMED_OUT;
      break;

    default:
      response_status = sharing::nearby::ConnectionResponseFrame::UNKNOWN;
      break;
  }

  WriteResponse(*connection, response_status);

  if (info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder().set_status(status).build());
  }
}

void NearbySharingServiceImpl::OnIncomingAdvertisementDecoded(
    const std::string& endpoint_id,
    ShareTarget placeholder_share_target,
    sharing::mojom::AdvertisementPtr advertisement) {
  NearbyConnection* connection = GetConnection(placeholder_share_target);
  if (!connection) {
    NS_LOG(VERBOSE) << __func__ << ": Invalid connection for endoint id - "
                    << endpoint_id;
    return;
  }

  if (!advertisement) {
    NS_LOG(VERBOSE) << __func__
                    << "Failed to parse incoming connection from endpoint - "
                    << endpoint_id << ", disconnecting.";
    connection->Close();
    return;
  }

  NearbyShareEncryptedMetadataKey encrypted_metadata_key(
      advertisement->salt, advertisement->encrypted_metadata_key);
  GetCertificateManager()->GetDecryptedPublicCertificate(
      std::move(encrypted_metadata_key),
      base::BindOnce(&NearbySharingServiceImpl::OnIncomingDecryptedCertificate,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id,
                     std::move(advertisement),
                     std::move(placeholder_share_target)));
}

void NearbySharingServiceImpl::OnIncomingTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& metadata) {
  NS_LOG(VERBOSE) << __func__ << ": Nearby Share service: "
                  << "Incoming transfer update for share target with ID "
                  << share_target.id << ": "
                  << TransferMetadata::StatusToString(metadata.status());
  if (metadata.status() != TransferMetadata::Status::kCancelled &&
      metadata.status() != TransferMetadata::Status::kRejected) {
    last_incoming_metadata_ =
        std::make_pair(share_target, TransferMetadataBuilder::Clone(metadata)
                                         .set_is_original(false)
                                         .build());
  } else {
    last_incoming_metadata_ = base::nullopt;
  }

  if (metadata.is_final_status()) {
    OnTransferComplete();
  } else if (metadata.status() ==
             TransferMetadata::Status::kAwaitingLocalConfirmation) {
    OnTransferStarted(/*is_incoming=*/true);
  }

  base::ObserverList<TransferUpdateCallback>& transfer_callbacks =
      foreground_receive_callbacks_.might_have_observers()
          ? foreground_receive_callbacks_
          : background_receive_callbacks_;

  for (TransferUpdateCallback& callback : transfer_callbacks) {
    callback.OnTransferUpdate(share_target, metadata);
  }
}

void NearbySharingServiceImpl::OnOutgoingTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& metadata) {
  NS_LOG(VERBOSE) << __func__ << ": Nearby Share service: "
                  << "Outgoing transfer update for share target with ID "
                  << share_target.id << ": "
                  << TransferMetadata::StatusToString(metadata.status());

  if (metadata.is_final_status()) {
    is_connecting_ = false;
    OnTransferComplete();
  } else if (metadata.status() == TransferMetadata::Status::kMediaDownloading ||
             metadata.status() ==
                 TransferMetadata::Status::kAwaitingLocalConfirmation) {
    is_connecting_ = false;
    OnTransferStarted(/*is_incoming=*/false);
  }

  bool has_foreground_send_surface =
      foreground_send_transfer_callbacks_.might_have_observers();
  base::ObserverList<TransferUpdateCallback>& transfer_callbacks =
      has_foreground_send_surface ? foreground_send_transfer_callbacks_
                                  : background_send_transfer_callbacks_;

  for (TransferUpdateCallback& callback : transfer_callbacks)
    callback.OnTransferUpdate(share_target, metadata);

  if (has_foreground_send_surface && metadata.is_final_status()) {
    last_outgoing_metadata_ = base::nullopt;
  } else {
    last_outgoing_metadata_ =
        std::make_pair(share_target, TransferMetadataBuilder::Clone(metadata)
                                         .set_is_original(false)
                                         .build());
  }
}

void NearbySharingServiceImpl::CloseConnection(
    const ShareTarget& share_target) {
  NearbyConnection* connection = GetConnection(share_target);
  if (!connection) {
    NS_LOG(WARNING) << __func__ << ": Invalid connection for target - "
                    << share_target.device_name;
    return;
  }
  connection->Close();
}

void NearbySharingServiceImpl::OnIncomingDecryptedCertificate(
    const std::string& endpoint_id,
    sharing::mojom::AdvertisementPtr advertisement,
    ShareTarget placeholder_share_target,
    base::Optional<NearbyShareDecryptedPublicCertificate> certificate) {
  NearbyConnection* connection = GetConnection(placeholder_share_target);
  if (!connection) {
    NS_LOG(VERBOSE) << __func__ << ": Invalid connection for endpoint id - "
                    << endpoint_id;
    return;
  }

  // Remove placeholder share target since we are creating the actual share
  // target below.
  incoming_share_target_info_map_.erase(placeholder_share_target.id);

  base::Optional<ShareTarget> share_target = CreateShareTarget(
      endpoint_id, advertisement, std::move(certificate), /*is_incoming=*/true);

  if (!share_target) {
    NS_LOG(VERBOSE) << __func__
                    << "Failed to convert advertisement to share target for "
                       "incoming connection, disconnecting";
    connection->Close();
    return;
  }

  NS_LOG(VERBOSE) << __func__ << "Received incoming connection from "
                  << share_target->device_name;

  ShareTargetInfo* share_target_info = GetShareTargetInfo(*share_target);
  DCHECK(share_target_info);
  share_target_info->set_connection(connection);

  share_target_info->set_transfer_update_callback(
      std::make_unique<TransferUpdateDecorator>(base::BindRepeating(
          &NearbySharingServiceImpl::OnIncomingTransferUpdate,
          weak_ptr_factory_.GetWeakPtr())));

  connection->SetDisconnectionListener(
      base::BindOnce(&NearbySharingServiceImpl::UnregisterShareTarget,
                     weak_ptr_factory_.GetWeakPtr(), *share_target));

  base::Optional<std::string> four_digit_token = ToFourDigitString(
      nearby_connections_manager_->GetRawAuthenticationToken(endpoint_id));

  RunPairedKeyVerification(
      *share_target, endpoint_id,
      base::BindOnce(
          &NearbySharingServiceImpl::OnIncomingConnectionKeyVerificationDone,
          weak_ptr_factory_.GetWeakPtr(), *share_target,
          std::move(four_digit_token)));
}

void NearbySharingServiceImpl::RunPairedKeyVerification(
    const ShareTarget& share_target,
    const std::string& endpoint_id,
    base::OnceCallback<void(
        PairedKeyVerificationRunner::PairedKeyVerificationResult)> callback) {
  DCHECK(profile_);
  base::Optional<std::vector<uint8_t>> token =
      nearby_connections_manager_->GetRawAuthenticationToken(endpoint_id);
  if (!token) {
    NS_LOG(VERBOSE) << __func__
                    << ": Failed to read authentication token from endpoint - "
                    << endpoint_id;
    std::move(callback).Run(
        PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail);
    return;
  }

  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  DCHECK(share_target_info);

  share_target_info->set_frames_reader(std::make_unique<IncomingFramesReader>(
      process_manager_, profile_, share_target_info->connection()));

  bool restrict_to_contacts =
      share_target.is_incoming &&
      advertising_power_level_ != PowerLevel::kHighPower;
  share_target_info->set_key_verification_runner(
      std::make_unique<PairedKeyVerificationRunner>(
          share_target, endpoint_id, *token, share_target_info->connection(),
          share_target_info->certificate(), GetCertificateManager(),
          settings_.GetVisibility(), restrict_to_contacts,
          share_target_info->frames_reader(), kReadFramesTimeout));
  share_target_info->key_verification_runner()->Run(std::move(callback));
}

void NearbySharingServiceImpl::OnIncomingConnectionKeyVerificationDone(
    ShareTarget share_target,
    base::Optional<std::string> four_digit_token,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection() || !info->endpoint_id()) {
    NS_LOG(VERBOSE) << __func__ << ": Invalid connection or endpoint id";
    return;
  }

  switch (result) {
    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail:
      NS_LOG(VERBOSE) << __func__ << ": Paired key handshake failed for target "
                      << share_target.device_name << ". Disconnecting.";
      info->connection()->Close();
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess:
      NS_LOG(VERBOSE) << __func__
                      << ": Paired key handshake succeeded for target - "
                      << share_target.device_name;
      nearby_connections_manager_->UpgradeBandwidth(*info->endpoint_id());
      ReceiveIntroduction(share_target, /*four_digit_token=*/base::nullopt);
      break;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable:
      NS_LOG(VERBOSE) << __func__
                      << ": Unable to verify paired key encryption when "
                         "receiving connection from target - "
                      << share_target.device_name;
      if (advertising_power_level_ == PowerLevel::kHighPower)
        nearby_connections_manager_->UpgradeBandwidth(*info->endpoint_id());

      if (four_digit_token)
        info->set_token(*four_digit_token);

      ReceiveIntroduction(share_target, std::move(four_digit_token));
      break;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown:
      NS_LOG(VERBOSE) << __func__
                      << ": Unknown PairedKeyVerificationResult for target "
                      << share_target.device_name << ". Disconnecting.";
      info->connection()->Close();
      break;
  }
}

void NearbySharingServiceImpl::OnOutgoingConnectionKeyVerificationDone(
    const ShareTarget& share_target,
    base::Optional<std::string> four_digit_token,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection())
    return;

  if (!info->transfer_update_callback()) {
    NS_LOG(VERBOSE) << __func__
                    << ": No transfer update callback. Disconnecting.";
    info->connection()->Close();
    return;
  }

  // TODO(crbug.com/1119279): Check if we need to set this to false for
  // Advanced Protection users.
  bool sender_skips_confirmation = true;

  switch (result) {
    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail:
      NS_LOG(VERBOSE) << __func__ << ": Paired key handshake failed for target "
                      << share_target.device_name << ". Disconnecting.";
      info->transfer_update_callback()->OnTransferUpdate(
          share_target, TransferMetadataBuilder()
                            .set_status(TransferMetadata::Status::kFailed)
                            .build());
      info->connection()->Close();
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess:
      NS_LOG(VERBOSE) << __func__
                      << ": Paired key handshake succeeded for target - "
                      << share_target.device_name;
      SendIntroduction(share_target, /*four_digit_token=*/base::nullopt);
      SendPayloads(share_target);
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable:
      NS_LOG(VERBOSE) << __func__
                      << ": Unable to verify paired key encryption when "
                         "initating connection to target - "
                      << share_target.device_name;

      if (four_digit_token)
        info->set_token(*four_digit_token);

      if (sender_skips_confirmation) {
        NS_LOG(VERBOSE) << __func__
                        << ": Sender-side verification is disabled. Skipping "
                           "token comparison with "
                        << share_target.device_name;
        SendIntroduction(share_target, /*four_digit_token=*/base::nullopt);
        SendPayloads(share_target);
      } else {
        SendIntroduction(share_target, std::move(four_digit_token));
      }
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown:
      NS_LOG(VERBOSE) << __func__
                      << ": Unknown PairedKeyVerificationResult for target "
                      << share_target.device_name << ". Disconnecting.";
      info->connection()->Close();
      break;
  }
}

void NearbySharingServiceImpl::RefreshUIOnDisconnection(
    ShareTarget share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target,
        TransferMetadataBuilder()
            .set_status(
                TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed)
            .build());
  }

  UnregisterShareTarget(share_target);
}

void NearbySharingServiceImpl::ReceiveIntroduction(
    ShareTarget share_target,
    base::Optional<std::string> four_digit_token) {
  NS_LOG(INFO) << __func__ << ": Receiving introduction from "
               << share_target.device_name;
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  DCHECK(info && info->connection());

  info->frames_reader()->ReadFrame(
      sharing::mojom::V1Frame::Tag::INTRODUCTION,
      base::BindOnce(&NearbySharingServiceImpl::OnReceivedIntroduction,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target),
                     std::move(four_digit_token)),
      kReadFramesTimeout);
}

void NearbySharingServiceImpl::OnReceivedIntroduction(
    ShareTarget share_target,
    base::Optional<std::string> four_digit_token,
    base::Optional<sharing::mojom::V1FramePtr> frame) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING)
        << __func__
        << ": Ignore received introduction, due to no connection established.";
    return;
  }
  NearbyConnection* connection = info->connection();
  DCHECK(profile_);

  if (!frame) {
    connection->Close();
    NS_LOG(WARNING) << __func__ << ": Invalid introduction frame";
    return;
  }

  NS_LOG(INFO) << __func__ << ": Successfully read the introduction frame.";

  base::CheckedNumeric<int64_t> file_size_sum(0);

  sharing::mojom::IntroductionFramePtr introduction_frame =
      std::move((*frame)->get_introduction());
  for (const auto& file : introduction_frame->file_metadata) {
    if (file->size <= 0) {
      Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);
      NS_LOG(WARNING)
          << __func__
          << ": Ignore introduction, due to invalid attachment size";
      return;
    }

    NS_LOG(VERBOSE) << __func__ << "Found file attachment " << file->name
                    << " of type " << file->type << " with mimeType "
                    << file->mime_type;
    FileAttachment attachment(file->id, file->size, file->name, file->mime_type,
                              file->type);
    SetAttachmentPayloadId(attachment, file->payload_id);
    share_target.file_attachments.push_back(std::move(attachment));

    file_size_sum += file->size;
    if (!file_size_sum.IsValid()) {
      Fail(share_target, TransferMetadata::Status::kNotEnoughSpace);
      NS_LOG(WARNING) << __func__
                      << ": Ignoring introduction, total file size overflowed "
                         "64 bit integer.";
      return;
    }
  }

  for (const auto& text : introduction_frame->text_metadata) {
    if (text->size <= 0) {
      Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);
      NS_LOG(WARNING)
          << __func__
          << ": Ignore introduction, due to invalid attachment size";
      return;
    }

    NS_LOG(VERBOSE) << __func__ << "Found text attachment " << text->text_title
                    << " of type " << text->type;
    TextAttachment attachment(text->id, text->type, text->text_title,
                              text->size);
    SetAttachmentPayloadId(attachment, text->payload_id);
    share_target.text_attachments.push_back(std::move(attachment));
  }

  if (!share_target.has_attachments()) {
    NS_LOG(WARNING) << __func__
                    << ": No attachment is found for this share target. It can "
                       "be result of unrecognizable attachment type";
    Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);

    NS_LOG(VERBOSE) << __func__
                    << ": We don't support the attachments sent by the sender. "
                       "We have informed "
                    << share_target.device_name;
    return;
  }

  if (file_size_sum.ValueOrDie() == 0) {
    OnStorageCheckCompleted(std::move(share_target),
                            std::move(four_digit_token),
                            /*is_out_of_storage=*/false);
    return;
  }

  base::FilePath download_path =
      DownloadPrefs::FromDownloadManager(
          content::BrowserContext::GetDownloadManager(profile_))
          ->DownloadPath();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&IsOutOfStorage, std::move(download_path),
                     file_size_sum.ValueOrDie(), free_disk_space_for_testing_),
      base::BindOnce(&NearbySharingServiceImpl::OnStorageCheckCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target),
                     std::move(four_digit_token)));
}

void NearbySharingServiceImpl::ReceiveConnectionResponse(
    ShareTarget share_target) {
  NS_LOG(VERBOSE) << __func__ << ": Receiving response frame from "
                  << share_target.device_name;
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  DCHECK(info && info->connection());

  info->frames_reader()->ReadFrame(
      sharing::mojom::V1Frame::Tag::CONNECTION_RESPONSE,
      base::BindOnce(&NearbySharingServiceImpl::OnReceiveConnectionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target)),
      kReadResponseFrameTimeout);
}

void NearbySharingServiceImpl::OnReceiveConnectionResponse(
    ShareTarget share_target,
    base::Optional<sharing::mojom::V1FramePtr> frame) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING) << __func__
                    << ": Ignore received connection response, due to no "
                       "connection established.";
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    NS_LOG(WARNING) << __func__
                    << ": No transfer update callback. Disconnecting.";
    connection->Close();
    return;
  }

  if (!frame) {
    NS_LOG(WARNING)
        << __func__
        << ": Failed to read a response from the remote device. Disconnecting.";
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder()
                          .set_status(TransferMetadata::Status::kFailed)
                          .build());
    connection->Close();
    return;
  }

  mutual_acceptance_timeout_alarm_.Cancel();

  NS_LOG(VERBOSE) << __func__
                  << ": Successfully read the connection response frame.";

  sharing::mojom::ConnectionResponseFramePtr response =
      std::move((*frame)->get_connection_response());
  switch (response->status) {
    case sharing::mojom::ConnectionResponseFrame::Status::kAccept: {
      info->frames_reader()->ReadFrame(
          base::BindOnce(&NearbySharingServiceImpl::OnFrameRead,
                         weak_ptr_factory_.GetWeakPtr(), share_target));

      info->transfer_update_callback()->OnTransferUpdate(
          share_target, TransferMetadataBuilder()
                            .set_status(TransferMetadata::Status::kInProgress)
                            .build());

      info->set_payload_tracker(std::make_unique<PayloadTracker>(
          share_target, attachment_info_map_,
          base::BindRepeating(
              &NearbySharingServiceImpl::OnPayloadTransferUpdate,
              weak_ptr_factory_.GetWeakPtr())));

      for (auto& payload : info->ExtractTextPayloads()) {
        nearby_connections_manager_->Send(
            *info->endpoint_id(), std::move(payload), info->payload_tracker());
      }
      for (auto& payload : info->ExtractFilePayloads()) {
        nearby_connections_manager_->Send(
            *info->endpoint_id(), std::move(payload), info->payload_tracker());
      }
      NS_LOG(VERBOSE)
          << __func__
          << ": The connection was accepted. Payloads are now being sent.";
      break;
    }
    case sharing::mojom::ConnectionResponseFrame::Status::kReject:
      info->transfer_update_callback()->OnTransferUpdate(
          share_target, TransferMetadataBuilder()
                            .set_status(TransferMetadata::Status::kRejected)
                            .build());
      connection->Close();
      NS_LOG(VERBOSE)
          << __func__
          << ": The connection was rejected. The connection has been closed.";
      break;
    case sharing::mojom::ConnectionResponseFrame::Status::kNotEnoughSpace:
      info->transfer_update_callback()->OnTransferUpdate(
          share_target,
          TransferMetadataBuilder()
              .set_status(TransferMetadata::Status::kNotEnoughSpace)
              .build());
      connection->Close();
      NS_LOG(VERBOSE)
          << __func__
          << ": The connection was rejected because the remote device "
             "does not have enough space for our attachments. The "
             "connection has been closed.";
      break;
    case sharing::mojom::ConnectionResponseFrame::Status::
        kUnsupportedAttachmentType:
      info->transfer_update_callback()->OnTransferUpdate(
          share_target,
          TransferMetadataBuilder()
              .set_status(TransferMetadata::Status::kUnsupportedAttachmentType)
              .build());
      connection->Close();
      NS_LOG(VERBOSE)
          << __func__
          << ": The connection was rejected because the remote device "
             "does not support the attachments we were sending. The "
             "connection has been closed.";
      break;
    case sharing::mojom::ConnectionResponseFrame::Status::kTimedOut:
      info->transfer_update_callback()->OnTransferUpdate(
          share_target, TransferMetadataBuilder()
                            .set_status(TransferMetadata::Status::kTimedOut)
                            .build());
      connection->Close();
      NS_LOG(VERBOSE)
          << __func__
          << ": The connection was rejected because the remote device "
             "timed out. The connection has been closed.";
      break;
    default:
      info->transfer_update_callback()->OnTransferUpdate(
          share_target, TransferMetadataBuilder()
                            .set_status(TransferMetadata::Status::kFailed)
                            .build());
      connection->Close();
      NS_LOG(VERBOSE)
          << __func__
          << ": The connection failed. The connection has been closed.";
      break;
  }
}

void NearbySharingServiceImpl::OnStorageCheckCompleted(
    ShareTarget share_target,
    base::Optional<std::string> four_digit_token,
    bool is_out_of_storage) {
  if (is_out_of_storage) {
    Fail(share_target, TransferMetadata::Status::kNotEnoughSpace);
    NS_LOG(WARNING) << __func__
                    << ": Not enough space on the receiver. We have informed "
                    << share_target.device_name;
    return;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(WARNING) << __func__ << ": Invalid connection for share target - "
                    << share_target.device_name;
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    connection->Close();
    NS_LOG(VERBOSE) << __func__
                    << ": No transfer update callback. Disconnecting.";
    return;
  }

  mutual_acceptance_timeout_alarm_.Reset(base::BindOnce(
      &NearbySharingServiceImpl::OnIncomingMutualAcceptanceTimeout,
      weak_ptr_factory_.GetWeakPtr(), share_target));

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(mutual_acceptance_timeout_alarm_.callback()),
      kReadResponseFrameTimeout);

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .set_token(std::move(four_digit_token))
          .build());

  if (!incoming_share_target_info_map_.count(share_target.id)) {
    connection->Close();
    NS_LOG(VERBOSE) << __func__
                    << ": IncomingShareTarget not found, disconnecting "
                    << share_target.device_name;
    return;
  }

  connection->SetDisconnectionListener(base::BindOnce(
      &NearbySharingServiceImpl::OnIncomingConnectionDisconnected,
      weak_ptr_factory_.GetWeakPtr(), share_target));

  auto* frames_reader = info->frames_reader();
  if (!frames_reader) {
    NS_LOG(WARNING) << __func__
                    << ": Stopped reading further frames, due to no connection "
                       "established.";
    return;
  }

  frames_reader->ReadFrame(
      base::BindOnce(&NearbySharingServiceImpl::OnFrameRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target)));
}

void NearbySharingServiceImpl::OnFrameRead(
    ShareTarget share_target,
    base::Optional<sharing::mojom::V1FramePtr> frame) {
  if (!frame) {
    // This is the case when the connection has been closed since we wait
    // indefinitely for incoming frames.
    return;
  }

  sharing::mojom::V1FramePtr v1_frame = std::move(*frame);
  switch (v1_frame->which()) {
    case sharing::mojom::V1Frame::Tag::CANCEL_FRAME:
      NS_LOG(VERBOSE) << __func__
                      << ": Read the cancel frame, closing connection";
      Cancel(share_target, base::DoNothing());
      break;

    case sharing::mojom::V1Frame::Tag::CERTIFICATE_INFO:
      HandleCertificateInfoFrame(v1_frame->get_certificate_info());
      break;

    default:
      NS_LOG(VERBOSE) << __func__ << ": Discarding unknown frame of type";
      break;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->frames_reader()) {
    NS_LOG(WARNING) << __func__
                    << ": Stopped reading further frames, due to no connection "
                       "established.";
    return;
  }

  info->frames_reader()->ReadFrame(
      base::BindOnce(&NearbySharingServiceImpl::OnFrameRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target)));
}

void NearbySharingServiceImpl::HandleCertificateInfoFrame(
    const sharing::mojom::CertificateInfoFramePtr& certificate_frame) {
  DCHECK(certificate_frame);

  // TODO(crbug.com/1113858): Allow saving certificates from remote devices.
}

void NearbySharingServiceImpl::OnIncomingConnectionDisconnected(
    const ShareTarget& share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder()
                          .set_status(TransferMetadata::Status::kFailed)
                          .build());
  }
  UnregisterShareTarget(share_target);
}

void NearbySharingServiceImpl::OnOutgoingConnectionDisconnected(
    const ShareTarget& share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target,
        TransferMetadataBuilder()
            .set_status(
                TransferMetadata::Status::kAwaitingRemoteAcceptanceFailed)
            .build());
  }
  UnregisterShareTarget(share_target);
}

void NearbySharingServiceImpl::OnIncomingMutualAcceptanceTimeout(
    const ShareTarget& share_target) {
  DCHECK(share_target.is_incoming);

  NS_LOG(VERBOSE)
      << __func__
      << ": Incoming mutual acceptance timed out, closing connection for "
      << share_target.device_name;

  Fail(share_target, TransferMetadata::Status::kTimedOut);
}

void NearbySharingServiceImpl::OnOutgoingMutualAcceptanceTimeout(
    const ShareTarget& share_target) {
  DCHECK(!share_target.is_incoming);

  NS_LOG(VERBOSE)
      << __func__
      << ": Outgoing mutual acceptance timed out, closing connection for "
      << share_target.device_name;

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info)
    return;

  if (info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder()
                          .set_status(TransferMetadata::Status::kTimedOut)
                          .build());
  }

  if (info->connection())
    info->connection()->Close();
}

base::Optional<ShareTarget> NearbySharingServiceImpl::CreateShareTarget(
    const std::string& endpoint_id,
    const sharing::mojom::AdvertisementPtr& advertisement,
    base::Optional<NearbyShareDecryptedPublicCertificate> certificate,
    bool is_incoming) {
  DCHECK(advertisement);

  if (!advertisement->device_name && !certificate) {
    NS_LOG(VERBOSE) << __func__
                    << ": Failed to retrieve public certificate for contact "
                       "only advertisement.";
    return base::nullopt;
  }

  base::Optional<std::string> device_name =
      GetDeviceName(advertisement, certificate);
  if (!device_name) {
    NS_LOG(VERBOSE) << __func__
                    << ": Failed to retrieve device name for advertisement.";
    return base::nullopt;
  }

  ShareTarget target;
  target.type = advertisement->device_type;
  target.device_name = std::move(*device_name);
  target.is_incoming = is_incoming;
  target.device_id = GetDeviceId(endpoint_id, certificate);

  ShareTargetInfo& info = GetOrCreateShareTargetInfo(target, endpoint_id);

  if (certificate) {
    if (certificate->unencrypted_metadata().has_full_name())
      target.full_name = certificate->unencrypted_metadata().full_name();

    if (certificate->unencrypted_metadata().has_icon_url())
      target.image_url = GURL(certificate->unencrypted_metadata().icon_url());

    target.is_known = true;
    info.set_certificate(std::move(*certificate));
  }

  return target;
}

void NearbySharingServiceImpl::OnPayloadTransferUpdate(
    ShareTarget share_target,
    TransferMetadata metadata) {
  NS_LOG(VERBOSE) << __func__ << ": Nearby Share service: "
                  << "Payload transfer update for share target with ID "
                  << share_target.id << ": "
                  << TransferMetadata::StatusToString(metadata.status());

  if (metadata.status() == TransferMetadata::Status::kComplete &&
      share_target.is_incoming && !OnIncomingPayloadsComplete(share_target)) {
    metadata = TransferMetadataBuilder()
                   .set_status(TransferMetadata::Status::kFailed)
                   .build();

    // Reset file paths for file attachments.
    for (auto& file : share_target.file_attachments)
      file.set_file_path(base::nullopt);

    // Reset body of text attachments.
    for (auto& text : share_target.text_attachments)
      text.set_text_body(std::string());
  }

  // Make sure to call this before calling Disconnect or we risk loosing some
  // transfer updates in the receive case due to the Disconnect call cleaning up
  // share targets.
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback())
    info->transfer_update_callback()->OnTransferUpdate(share_target, metadata);

  if (TransferMetadata::IsFinalStatus(metadata.status())) {
    if (metadata.status() != TransferMetadata::Status::kComplete)
      OnPayloadsFailed(share_target);

    Disconnect(share_target, metadata);
  }
}

bool NearbySharingServiceImpl::OnIncomingPayloadsComplete(
    ShareTarget& share_target) {
  DCHECK(share_target.is_incoming);

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    NS_LOG(VERBOSE) << __func__ << ": Connection not found for target - "
                    << share_target.device_name;

    return false;
  }
  NearbyConnection* connection = info->connection();

  connection->SetDisconnectionListener(
      base::BindOnce(&NearbySharingServiceImpl::UnregisterShareTarget,
                     weak_ptr_factory_.GetWeakPtr(), share_target));

  for (auto& file : share_target.file_attachments) {
    AttachmentInfo& attachment_info = attachment_info_map_[file.id()];
    base::Optional<int64_t> payload_id = attachment_info.payload_id;
    if (!payload_id) {
      NS_LOG(WARNING) << __func__ << ": No payload id found for file - "
                      << file.id();
      return false;
    }

    location::nearby::connections::mojom::Payload* incoming_payload =
        nearby_connections_manager_->GetIncomingPayload(*payload_id);
    if (!incoming_payload || !incoming_payload->content ||
        !incoming_payload->content->is_file()) {
      NS_LOG(WARNING) << __func__ << ": No payload found for file - "
                      << file.id();
      return false;
    }

    file.set_file_path(attachment_info.file_path);
  }

  for (auto& text : share_target.text_attachments) {
    AttachmentInfo& attachment_info = attachment_info_map_[text.id()];
    base::Optional<int64_t> payload_id = attachment_info.payload_id;
    if (!payload_id) {
      NS_LOG(WARNING) << __func__ << ": No payload id found for text - "
                      << text.id();
      return false;
    }

    location::nearby::connections::mojom::Payload* incoming_payload =
        nearby_connections_manager_->GetIncomingPayload(*payload_id);
    if (!incoming_payload || !incoming_payload->content ||
        !incoming_payload->content->is_bytes()) {
      NS_LOG(WARNING) << __func__ << ": No payload found for text - "
                      << text.id();
      return false;
    }

    std::vector<uint8_t>& bytes = incoming_payload->content->get_bytes()->bytes;
    if (bytes.empty()) {
      NS_LOG(WARNING)
          << __func__
          << ": Incoming bytes is empty for text payload with payload_id - "
          << *payload_id;
      return false;
    }

    std::string text_body(bytes.begin(), bytes.end());
    text.set_text_body(text_body);

    attachment_info.text_body = std::move(text_body);
  }
  return true;
}

void NearbySharingServiceImpl::OnPayloadsFailed(ShareTarget share_target) {
  if (!share_target.is_incoming)
    return;

  nearby_connections_manager_->ClearIncomingPayloads();
  std::vector<base::FilePath> files_for_deletion;
  for (const auto& file : share_target.file_attachments) {
    auto it = attachment_info_map_.find(file.id());
    if (it == attachment_info_map_.end())
      continue;

    files_for_deletion.push_back(it->second.file_path);
  }

  file_handler_.DeleteFilesFromDisk(std::move(files_for_deletion));
}

void NearbySharingServiceImpl::Disconnect(const ShareTarget& share_target,
                                          TransferMetadata metadata) {
  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  if (!share_target_info) {
    NS_LOG(WARNING)
        << __func__
        << ": Failed to disconnect. No share target info found for target - "
        << share_target.device_name;
    return;
  }

  base::Optional<std::string> endpoint_id = share_target_info->endpoint_id();
  if (!endpoint_id) {
    NS_LOG(WARNING)
        << __func__
        << ": Failed to disconnect. No endpoint id found for share target - "
        << share_target.device_name;
    return;
  }

  // Failed to send or receive. No point in continuing, so disconnect
  // immediately.
  if (metadata.status() != TransferMetadata::Status::kComplete) {
    if (share_target_info->connection()) {
      share_target_info->connection()->Close();
    } else {
      nearby_connections_manager_->Disconnect(*endpoint_id);
    }
    return;
  }

  // Files received successfully. Receivers can immediately cancel.
  if (share_target.is_incoming) {
    if (share_target_info->connection()) {
      share_target_info->connection()->Close();
    } else {
      nearby_connections_manager_->Disconnect(*endpoint_id);
    }
    return;
  }

  // Disconnect after a timeout to make sure any pending payloads are sent.
  auto timer = std::make_unique<base::CancelableOnceClosure>(base::BindOnce(
      &NearbySharingServiceImpl::OnDisconnectingConnectionTimeout,
      weak_ptr_factory_.GetWeakPtr(), *endpoint_id));
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, timer->callback(), kOutgoingDisconnectionDelay);
  disconnection_timeout_alarms_[*endpoint_id] = std::move(timer);

  // Stop the disconnection timeout if the connection has been closed already.
  if (share_target_info->connection()) {
    share_target_info->connection()->SetDisconnectionListener(base::BindOnce(
        &NearbySharingServiceImpl::OnDisconnectingConnectionDisconnected,
        weak_ptr_factory_.GetWeakPtr(), share_target, *endpoint_id));
  }
}

void NearbySharingServiceImpl::OnDisconnectingConnectionTimeout(
    const std::string& endpoint_id) {
  disconnection_timeout_alarms_.erase(endpoint_id);
  nearby_connections_manager_->Disconnect(endpoint_id);
}

void NearbySharingServiceImpl::OnDisconnectingConnectionDisconnected(
    const ShareTarget& share_target,
    const std::string& endpoint_id) {
  disconnection_timeout_alarms_.erase(endpoint_id);
  UnregisterShareTarget(share_target);
}

ShareTargetInfo& NearbySharingServiceImpl::GetOrCreateShareTargetInfo(
    const ShareTarget& share_target,
    const std::string& endpoint_id) {
  if (share_target.is_incoming) {
    auto& info = incoming_share_target_info_map_[share_target.id];
    info.set_endpoint_id(endpoint_id);
    return info;
  } else {
    outgoing_share_target_map_.emplace(endpoint_id, share_target);
    auto& info = outgoing_share_target_info_map_[share_target.id];
    info.set_endpoint_id(endpoint_id);
    return info;
  }
}

ShareTargetInfo* NearbySharingServiceImpl::GetShareTargetInfo(
    const ShareTarget& share_target) {
  if (share_target.is_incoming)
    return GetIncomingShareTargetInfo(share_target);
  else
    return GetOutgoingShareTargetInfo(share_target);
}

IncomingShareTargetInfo* NearbySharingServiceImpl::GetIncomingShareTargetInfo(
    const ShareTarget& share_target) {
  auto it = incoming_share_target_info_map_.find(share_target.id);
  if (it == incoming_share_target_info_map_.end())
    return nullptr;

  return &it->second;
}

OutgoingShareTargetInfo* NearbySharingServiceImpl::GetOutgoingShareTargetInfo(
    const ShareTarget& share_target) {
  auto it = outgoing_share_target_info_map_.find(share_target.id);
  if (it == outgoing_share_target_info_map_.end())
    return nullptr;

  return &it->second;
}

NearbyConnection* NearbySharingServiceImpl::GetConnection(
    const ShareTarget& share_target) {
  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  return share_target_info ? share_target_info->connection() : nullptr;
}

base::Optional<std::vector<uint8_t>>
NearbySharingServiceImpl::GetBluetoothMacAddress(
    const ShareTarget& share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info)
    return base::nullopt;

  const base::Optional<NearbyShareDecryptedPublicCertificate>& certificate =
      info->certificate();
  if (!certificate ||
      !certificate->unencrypted_metadata().has_bluetooth_mac_address()) {
    return base::nullopt;
  }

  std::string mac_address =
      certificate->unencrypted_metadata().bluetooth_mac_address();
  if (mac_address.size() != 6)
    return base::nullopt;

  return std::vector<uint8_t>(mac_address.begin(), mac_address.end());
}

void NearbySharingServiceImpl::ClearOutgoingShareTargetInfoMap() {
  for (auto& entry : outgoing_share_target_info_map_)
    file_handler_.ReleaseFilePayloads(entry.second.ExtractFilePayloads());

  outgoing_share_target_info_map_.clear();
  outgoing_share_target_map_.clear();
}

void NearbySharingServiceImpl::SetAttachmentPayloadId(
    const Attachment& attachment,
    int64_t payload_id) {
  attachment_info_map_[attachment.id()].payload_id = payload_id;
}

base::Optional<int64_t> NearbySharingServiceImpl::GetAttachmentPayloadId(
    int64_t attachment_id) {
  auto it = attachment_info_map_.find(attachment_id);
  if (it == attachment_info_map_.end())
    return base::nullopt;

  return it->second.payload_id;
}

void NearbySharingServiceImpl::UnregisterShareTarget(
    const ShareTarget& share_target) {
  NS_LOG(VERBOSE) << __func__ << ": Unregistering share target - "
                  << share_target.device_name;

  if (share_target.is_incoming) {
    if (last_incoming_metadata_ &&
        last_incoming_metadata_->first.id == share_target.id) {
      last_incoming_metadata_.reset();
    }
    incoming_share_target_info_map_.erase(share_target.id);
    // Clear legacy incoming payloads to release resource
    nearby_connections_manager_->ClearIncomingPayloads();
  } else {
    if (last_outgoing_metadata_ &&
        last_outgoing_metadata_->first.id == share_target.id) {
      last_outgoing_metadata_.reset();
    }
    // Find the endpoint id that matches the given share target.
    base::Optional<std::string> endpoint_id;
    auto it = outgoing_share_target_info_map_.find(share_target.id);
    if (it != outgoing_share_target_info_map_.end())
      endpoint_id = it->second.endpoint_id();

    // Remove info except for this endpoint id, if present.
    ClearOutgoingShareTargetInfoMap();

    if (endpoint_id) {
      NS_LOG(VERBOSE) << __func__ << ": Unregister share target: "
                      << share_target.device_name;
      GetOrCreateShareTargetInfo(share_target, *endpoint_id);
    } else {
      NS_LOG(VERBOSE)
          << __func__
          << ": Cannot unregister share target since none registered: "
          << share_target.device_name;
    }
  }
  mutual_acceptance_timeout_alarm_.Cancel();
}

void NearbySharingServiceImpl::OnStartAdvertisingResult(
    bool used_device_name,
    NearbyConnectionsManager::ConnectionsStatus status) {
  if (status == NearbyConnectionsManager::ConnectionsStatus::kSuccess) {
    NS_LOG(VERBOSE)
        << "StartAdvertising over Nearby Connections was successful.";
    SetInHighVisibility(used_device_name);
  } else {
    NS_LOG(ERROR) << "StartAdvertising over Nearby Connections failed: "
                  << NearbyConnectionsManager::ConnectionsStatusToString(
                         status);
    SetInHighVisibility(false);
  }
}

void NearbySharingServiceImpl::SetInHighVisibility(
    bool new_in_high_visibility) {
  if (in_high_visibility == new_in_high_visibility)
    return;

  in_high_visibility = new_in_high_visibility;
  for (auto& observer : observers_) {
    observer.OnHighVisibilityChanged(in_high_visibility);
  }
}
