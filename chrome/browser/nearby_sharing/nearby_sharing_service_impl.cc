// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_sharing_service_impl.h"

#include <utility>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/barrier_closure.h"
#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager_impl.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/client/nearby_share_client_impl.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/nearby_sharing/constants.h"
#include "chrome/browser/nearby_sharing/contacts/nearby_share_contact_manager_impl.h"
#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_advertiser.h"
#include "chrome/browser/nearby_sharing/fast_initiation/fast_initiation_scanner.h"
#include "chrome/browser/nearby_sharing/local_device_data/nearby_share_local_device_data_manager_impl.h"
#include "chrome/browser/nearby_sharing/nearby_share_error.h"
#include "chrome/browser/nearby_sharing/nearby_share_feature_status.h"
#include "chrome/browser/nearby_sharing/nearby_share_metrics.h"
#include "chrome/browser/nearby_sharing/nearby_share_transfer_profiler.h"
#include "chrome/browser/nearby_sharing/paired_key_verification_runner.h"
#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/nearby_sharing/transfer_metadata.h"
#include "chrome/browser/nearby_sharing/transfer_metadata_builder.h"
#include "chrome/browser/nearby_sharing/wifi_network_configuration/wifi_network_configuration_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/services/sharing/public/cpp/advertisement.h"
#include "chrome/services/sharing/public/cpp/conversions.h"
#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connections_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_decoder.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_target_types.mojom.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/cross_device/logging/logging.h"
#include "components/cross_device/nearby/nearby_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/random.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_low_energy_scan_filter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

// static
constexpr int
    NearbySharingServiceImpl::kMaxRecentNearbyProcessUnexpectedShutdownCount;

namespace {

using NearbyProcessShutdownReason =
    ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason;

constexpr base::TimeDelta kBackgroundAdvertisementRotationDelayMin =
    base::Minutes(12);
// 870 seconds represents 14:30 minutes
constexpr base::TimeDelta kBackgroundAdvertisementRotationDelayMax =
    base::Seconds(870);
constexpr base::TimeDelta kInvalidateSurfaceStateDelayAfterTransferDone =
    base::Milliseconds(3000);
constexpr base::TimeDelta kProcessShutdownPendingTimerDelay = base::Seconds(15);
constexpr base::TimeDelta kProcessNetworkChangeTimerDelay = base::Seconds(1);

// Cooldown period after a successful incoming share before we allow the "Device
// nearby is sharing" notification to appear again.
constexpr base::TimeDelta kFastInitiationScannerCooldown = base::Seconds(8);

// The maximum number of certificate downloads that can be performed during a
// discovery session.
constexpr size_t kMaxCertificateDownloadsDuringDiscovery = 3u;
// The time between certificate downloads during a discovery session. The
// download is only attempted if there are discovered, contact-based
// advertisements that cannot decrypt any currently stored public certificates.
constexpr base::TimeDelta kCertificateDownloadDuringDiscoveryPeriod =
    base::Seconds(10);

// Used to hash a token into a 4 digit string.
constexpr int kHashModulo = 9973;
constexpr int kHashBaseMultiplier = 31;

// Length of the window during which we count the amount of times the nearby
// process stops unexpectedly.
constexpr base::TimeDelta kClearNearbyProcessUnexpectedShutdownCountDelay =
    base::Minutes(1);

// The length of window during which we display visibility reminder
// notification to users. The real length set for timer should be calculated
// by (180 - kNearbySharingVisibilityReminderLastShownTimePrefName set in
// nearby_share_prefs).
constexpr base::TimeDelta kNearbyVisibilityReminderTimerDelay = base::Days(180);

// Whether or not WifiLan is supported for advertising (mDNS). Support as
// a bandwidth upgrade medium is behind a feature flag. Currently unsupported.
constexpr bool kIsWifiLanAdvertisingSupported = false;

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

std::string PowerLevelToString(NearbyConnectionsManager::PowerLevel level) {
  switch (level) {
    case NearbyConnectionsManager::PowerLevel::kLowPower:
      return "LOW_POWER";
    case NearbyConnectionsManager::PowerLevel::kMediumPower:
      return "MEDIUM_POWER";
    case NearbyConnectionsManager::PowerLevel::kHighPower:
      return "HIGH_POWER";
    case NearbyConnectionsManager::PowerLevel::kUnknown:
      return "UNKNOWN";
  }
}

std::optional<std::vector<uint8_t>> GetBluetoothMacAddressFromCertificate(
    const NearbyShareDecryptedPublicCertificate& certificate) {
  if (!certificate.unencrypted_metadata().has_bluetooth_mac_address()) {
    RecordNearbyShareError(
        NearbyShareError::kPublicCertificateHasNoBluetoothMacAddress);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Public certificate "
        << base::HexEncode(certificate.id()) << " did not contain "
        << "a Bluetooth mac address.";
    return std::nullopt;
  }

  std::string mac_address =
      certificate.unencrypted_metadata().bluetooth_mac_address();
  if (mac_address.size() != 6) {
    RecordNearbyShareError(
        NearbyShareError::kPublicCertificateHasInvalidBluetoothMacAddress);
    CD_LOG(ERROR, Feature::NS)
        << __func__ << ": Invalid bluetooth mac address: '" << mac_address
        << "'";
    return std::nullopt;
  }

  return std::vector<uint8_t>(mac_address.begin(), mac_address.end());
}

std::optional<std::string> GetDeviceName(
    const sharing::mojom::AdvertisementPtr& advertisement,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate) {
  DCHECK(advertisement);

  // Device name is always included when visible to everyone.
  if (advertisement->device_name) {
    return *(advertisement->device_name);
  }

  // For contacts only advertisements, we can't do anything without the
  // certificate.
  if (!certificate || !certificate->unencrypted_metadata().has_device_name()) {
    return std::nullopt;
  }

  return certificate->unencrypted_metadata().device_name();
}

// Return the most stable device identifier with the following priority:
//   1. Hash of Bluetooth MAC address.
//   2. Certificate ID.
//   3. Endpoint ID.
std::string GetDeviceId(
    const std::string& endpoint_id,
    const std::optional<NearbyShareDecryptedPublicCertificate>& certificate) {
  if (!certificate) {
    return endpoint_id;
  }

  std::optional<std::vector<uint8_t>> mac_address =
      GetBluetoothMacAddressFromCertificate(*certificate);
  if (mac_address) {
    return base::NumberToString(base::FastHash(base::make_span(*mac_address)));
  }

  if (!certificate->id().empty()) {
    return std::string(certificate->id().begin(), certificate->id().end());
  }

  return endpoint_id;
}

std::optional<std::string> ToFourDigitString(
    const std::optional<std::vector<uint8_t>>& bytes) {
  if (!bytes) {
    return std::nullopt;
  }

  int hash = 0;
  int multiplier = 1;
  for (uint8_t byte : *bytes) {
    // Java bytes are signed two's complement so cast to use the correct sign.
    hash = (hash + static_cast<int8_t>(byte) * multiplier) % kHashModulo;
    multiplier = (multiplier * kHashBaseMultiplier) % kHashModulo;
  }

  return base::StringPrintf("%04d", std::abs(hash));
}

bool IsOutOfStorage(const base::FilePath& file_path,
                    int64_t storage_required,
                    std::optional<int64_t> free_disk_space_for_testing) {
  int64_t free_space = free_disk_space_for_testing.value_or(
      base::SysInfo::AmountOfFreeDiskSpace(file_path));
  return free_space < storage_required;
}

int64_t GeneratePayloadId() {
  int64_t payload_id = 0;
  crypto::RandBytes(base::byte_span_from_ref(payload_id));
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
    if (got_final_status_) {
      // If we already got a final status, we can ignore any subsequent final
      // statuses caused by race conditions.
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Transfer update decorator swallowed "
          << "status update because a final status was already received: "
          << share_target.id << ": "
          << TransferMetadata::StatusToString(transfer_metadata.status());
      return;
    }
    got_final_status_ = transfer_metadata.is_final_status();
    callback_.Run(share_target, transfer_metadata);
  }

 private:
  bool got_final_status_ = false;
  Callback callback_;
};

bool isVisibleForAdvertising(nearby_share::mojom::Visibility visibility) {
  return visibility == nearby_share::mojom::Visibility::kAllContacts ||
         visibility == nearby_share::mojom::Visibility::kSelectedContacts ||
         visibility == nearby_share::mojom::Visibility::kYourDevices;
}

}  // namespace

NearbySharingServiceImpl::NearbySharingServiceImpl(
    PrefService* prefs,
    NotificationDisplayService* notification_display_service,
    Profile* profile,
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager,
    ash::nearby::NearbyProcessManager* process_manager,
    std::unique_ptr<PowerClient> power_client,
    std::unique_ptr<WifiNetworkConfigurationHandler> wifi_network_handler)
    : prefs_(prefs),
      profile_(profile),
      nearby_connections_manager_(std::move(nearby_connections_manager)),
      process_manager_(process_manager),
      power_client_(std::move(power_client)),
      wifi_network_handler_(std::move(wifi_network_handler)),
      http_client_factory_(std::make_unique<NearbyShareClientFactoryImpl>(
          IdentityManagerFactory::GetForProfile(profile),
          profile->GetURLLoaderFactory(),
          &nearby_share_http_notifier_)),
      profile_info_provider_(
          std::make_unique<NearbyShareProfileInfoProviderImpl>(profile_)),
      local_device_data_manager_(
          NearbyShareLocalDeviceDataManagerImpl::Factory::Create(
              prefs,
              http_client_factory_.get(),
              profile_info_provider_.get())),
      contact_manager_(NearbyShareContactManagerImpl::Factory::Create(
          prefs,
          http_client_factory_.get(),
          local_device_data_manager_.get(),
          profile_info_provider_.get())),
      certificate_manager_(NearbyShareCertificateManagerImpl::Factory::Create(
          local_device_data_manager_.get(),
          contact_manager_.get(),
          profile_info_provider_.get(),
          prefs,
          profile->GetDefaultStoragePartition()->GetProtoDatabaseProvider(),
          profile->GetPath(),
          http_client_factory_.get())),
      transfer_profiler_(std::make_unique<NearbyShareTransferProfiler>()),
      logger_(std::make_unique<NearbyShareLogger>()),
      settings_(prefs, local_device_data_manager_.get()),
      feature_usage_metrics_(prefs),
      on_network_changed_delay_timer_(
          FROM_HERE,
          kProcessNetworkChangeTimerDelay,
          base::BindRepeating(&NearbySharingServiceImpl::
                                  StopAdvertisingAndInvalidateSurfaceState,
                              base::Unretained(this))),
      visibility_reminder_timer_delay_(kNearbyVisibilityReminderTimerDelay),
      discovery_metric_logger_(
          std::make_unique<nearby::share::metrics::DiscoveryMetricLogger>()),
      throughput_metric_logger_(
          std::make_unique<nearby::share::metrics::ThroughputMetricLogger>()),
      attachment_metric_logger_(
          std::make_unique<nearby::share::metrics::AttachmentMetricLogger>()),
      neaby_share_metric_logger_(
          std::make_unique<nearby::share::metrics::NearbyShareMetricLogger>()) {
  DCHECK(profile_);
  DCHECK(nearby_connections_manager_);
  DCHECK(power_client_);

  nearby_connections_manager_->RegisterBandwidthUpgradeListener(
      weak_ptr_factory_.GetWeakPtr());

  fast_initiation_scanning_metrics_ =
      std::make_unique<FastInitiationScannerFeatureUsageMetrics>(prefs_);

  RecordNearbyShareEnabledMetric(GetNearbyShareEnabledState(prefs_));

  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    is_screen_locked_ = session_controller->IsScreenLocked();
    session_controller->AddObserver(this);
  }

  power_client_->AddObserver(this);
  certificate_manager_->AddObserver(this);

  settings_.AddSettingsObserver(settings_receiver_.BindNewPipeAndPassRemote());

  // Register logging observers.
  AddObserver(logger_.get());
  AddObserver(discovery_metric_logger_.get());
  AddObserver(throughput_metric_logger_.get());
  AddObserver(attachment_metric_logger_.get());
  AddObserver(neaby_share_metric_logger_.get());

  GetBluetoothAdapter();

  nearby_notification_manager_ = std::make_unique<NearbyNotificationManager>(
      notification_display_service, this, prefs, profile_);

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);

  if (settings_.GetEnabled()) {
    local_device_data_manager_->Start();
    contact_manager_->Start();
    certificate_manager_->Start();
    BindToNearbyProcess();
  }
  UpdateVisibilityReminderTimer(/*reset_timestamp=*/false);
  user_visibility_ = settings_.GetVisibility();
}

NearbySharingServiceImpl::~NearbySharingServiceImpl() {
  // Make sure the service has been shut down properly before.
  DCHECK(!nearby_notification_manager_);

  if (bluetooth_adapter_) {
    DCHECK(!bluetooth_adapter_->HasObserver(this));
  }

  // Unregister observers.
  RemoveObserver(logger_.get());
  RemoveObserver(discovery_metric_logger_.get());
  RemoveObserver(throughput_metric_logger_.get());
  RemoveObserver(attachment_metric_logger_.get());
  RemoveObserver(neaby_share_metric_logger_.get());
}

void NearbySharingServiceImpl::Shutdown() {
  // Before we clean up, lets give observers a heads up we are shutting down.
  for (auto& observer : observers_) {
    observer.OnShutdown();
  }
  observers_.Clear();

  StopAdvertising();
  StopFastInitiationScanning();
  StopFastInitiationAdvertising();
  StopScanning();
  nearby_connections_manager_->Shutdown();

  // Destroy NearbyNotificationManager as its profile has been shut down.
  nearby_notification_manager_.reset();

  // On shutdown, we want to do all the same clean up as happens when
  // the nearby process stops.
  CleanupAfterNearbyProcessStopped();

  power_client_->RemoveObserver(this);
  certificate_manager_->RemoveObserver(this);

  // TODO(crbug/1147652): The call to update the advertising interval is
  // removed to prevent a Bluez crash. We need to either reduce the global
  // advertising interval asynchronously and wait for the result or use the
  // updated API referenced in the bug which allows setting a per-advertisement
  // interval.

  if (bluetooth_adapter_) {
    bluetooth_adapter_->RemoveObserver(this);
    bluetooth_adapter_.reset();
  }

  auto* session_controller = ash::SessionController::Get();
  if (session_controller) {
    session_controller->RemoveObserver(this);
  }

  foreground_receive_callbacks_.Clear();
  background_receive_callbacks_.Clear();

  settings_receiver_.reset();

  if (settings_.GetEnabled()) {
    local_device_data_manager_->Stop();
    contact_manager_->Stop();
    certificate_manager_->Stop();
  }

  // |profile_| has now been shut down so we shouldn't use it anymore.
  profile_ = nullptr;

  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
  on_network_changed_delay_timer_.Stop();
  fast_initiation_scanner_cooldown_timer_.Stop();
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
    RecordNearbyShareError(
        NearbyShareError::kRegisterSendSurfaceAlreadyRegistered);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": RegisterSendSurface failed. Already registered for a "
           "different state.";
    return StatusCodes::kError;
  }

  if (state == SendSurfaceState::kForeground) {
    // Only check this error case for foreground senders
    if (!HasAvailableDiscoveryMediums()) {
      RecordNearbyShareError(
          NearbyShareError::kRegisterSendSurfaceNoAvailableConnectionMedium);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": No available connection medium.";
      return StatusCodes::kNoAvailableConnectionMedium;
    }

    foreground_send_transfer_callbacks_.AddObserver(transfer_callback);
    foreground_send_discovery_callbacks_.AddObserver(discovery_callback);
  } else {
    background_send_transfer_callbacks_.AddObserver(transfer_callback);
    background_send_discovery_callbacks_.AddObserver(discovery_callback);
  }

  if (is_receiving_files_) {
    UnregisterSendSurface(transfer_callback, discovery_callback);
    CD_LOG(VERBOSE, Feature::NS)
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

  // Sync down data from Nearby server when the sending flow starts, making our
  // best effort to have fresh contact and certificate data. There is no need to
  // wait for these calls to finish. The periodic server requests will typically
  // be sufficient, but we don't want the user to be blocked for hours waiting
  // for a periodic sync.
  if (state == SendSurfaceState::kForeground && !last_outgoing_metadata_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Downloading local device data, contacts, and certificates from "
        << "Nearby server at start of sending flow.";
    local_device_data_manager_->DownloadDeviceData();
    contact_manager_->DownloadContacts();
    certificate_manager_->DownloadPublicCertificates();
  }

  // Let newly registered send surface catch up with discovered share targets
  // from current scanning session.
  for (const std::pair<std::string, ShareTarget>& item :
       outgoing_share_target_map_) {
    discovery_callback->OnShareTargetDiscovered(item.second);
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": A SendSurface has been registered for state: "
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
    RecordNearbyShareError(
        NearbyShareError::kUnregisterSendSurfaceUnknownTransferUpdateCallback);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": unregisterSendSurface failed. Unknown TransferUpdateCallback";
    return StatusCodes::kError;
  }

  if (!foreground_send_transfer_callbacks_.empty() && last_outgoing_metadata_ &&
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
  if (foreground_send_transfer_callbacks_.empty() && last_outgoing_metadata_) {
    for (TransferUpdateCallback& background_transfer_callback :
         background_send_transfer_callbacks_) {
      background_transfer_callback.OnTransferUpdate(
          last_outgoing_metadata_->first, last_outgoing_metadata_->second);
    }
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": A SendSurface has been unregistered: "
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

  // Only check these errors cases for foreground receivers.
  if (state == ReceiveSurfaceState::kForeground) {
    if (is_scanning_ || is_transferring_) {
      UnregisterReceiveSurface(transfer_callback);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": Ignore registering (and unregistering if registered) receive "
             "surface, because we're currently sending or receiving files.";
      return StatusCodes::kTransferAlreadyInProgress;
    }

    if (!HasAvailableAdvertisingMediums()) {
      RecordNearbyShareError(
          NearbyShareError::kRegisterReceiveSurfaceNoAvailableConnectionMedium);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": No available connection medium.";
      return StatusCodes::kNoAvailableConnectionMedium;
    }
  }

  // We specifically allow re-registring with out error so it is clear to caller
  // that the transfer_callback is currently registered.
  if (GetReceiveCallbacksFromState(state).HasObserver(transfer_callback)) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": transfer callback already registered, ignoring";
    return StatusCodes::kOk;
  } else if (foreground_receive_callbacks_.HasObserver(transfer_callback) ||
             background_receive_callbacks_.HasObserver(transfer_callback)) {
    RecordNearbyShareError(
        NearbyShareError::
            kRegisterReceiveSurfaceTransferCallbackAlreadyRegisteredDifferentState);
    CD_LOG(ERROR, Feature::NS)
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

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": A ReceiveSurface(" << ReceiveSurfaceStateToString(state)
      << ") has been registered";

  // TODO(crbug.com/40753805): Remove these logs. They are only needed to help
  // debug crbug.com/1186559.
  if (state == ReceiveSurfaceState::kForeground) {
    if (!IsBluetoothPresent()) {
      CD_LOG(ERROR, Feature::NS) << __func__ << ": Bluetooth is not present.";
    } else if (!IsBluetoothPowered()) {
      CD_LOG(WARNING, Feature::NS) << __func__ << ": Bluetooth is not powered.";
    } else {
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": This device's MAC address is: "
          << bluetooth_adapter_->GetAddress();
    }
  }

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
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Unknown transfer callback was un-registered, ignoring.";
    // We intentionally allow this be successful so the caller can be sure
    // they are not registered anymore.
    return StatusCodes::kOk;
  }

  if (!foreground_receive_callbacks_.empty() && last_incoming_metadata_ &&
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
  if (foreground_receive_callbacks_.empty() && last_incoming_metadata_) {
    for (TransferUpdateCallback& background_callback :
         background_receive_callbacks_) {
      background_callback.OnTransferUpdate(last_incoming_metadata_->first,
                                           last_incoming_metadata_->second);
    }
  }

  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": A ReceiveSurface("
                               << (is_foreground ? "foreground" : "background")
                               << ") has been unregistered";
  InvalidateSurfaceState();
  return StatusCodes::kOk;
}

NearbySharingService::StatusCodes
NearbySharingServiceImpl::ClearForegroundReceiveSurfaces() {
  std::vector<TransferUpdateCallback*> fg_receivers;
  for (auto& callback : foreground_receive_callbacks_) {
    fg_receivers.push_back(&callback);
  }

  StatusCodes status = StatusCodes::kOk;
  for (TransferUpdateCallback* callback : fg_receivers) {
    if (UnregisterReceiveSurface(callback) != StatusCodes::kOk) {
      status = StatusCodes::kError;
    }
  }
  return status;
}

bool NearbySharingServiceImpl::IsInHighVisibility() const {
  if (chromeos::features::IsQuickShareV2Enabled()) {
    return prefs_->GetBoolean(prefs::kNearbySharingInHighVisibilityPrefName);
  }

  return in_high_visibility_;
}

bool NearbySharingServiceImpl::IsTransferring() const {
  return is_transferring_;
}

bool NearbySharingServiceImpl::IsReceivingFile() const {
  return is_receiving_files_;
}

bool NearbySharingServiceImpl::IsSendingFile() const {
  return is_sending_files_;
}

bool NearbySharingServiceImpl::IsScanning() const {
  return is_scanning_;
}

bool NearbySharingServiceImpl::IsConnecting() const {
  return is_connecting_;
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::SendAttachments(
    const ShareTarget& share_target,
    std::vector<std::unique_ptr<Attachment>> attachments) {
  if (!is_scanning_) {
    RecordNearbyShareError(NearbyShareError::kSendAttachmentsNotScanning);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to send attachments. Not scanning.";
    return StatusCodes::kError;
  }

  // |is_scanning_| means at least one send transfer callback.
  DCHECK(!foreground_send_transfer_callbacks_.empty() ||
         !background_send_transfer_callbacks_.empty());
  // |is_scanning_| and |is_transferring_| are mutually exclusive.
  DCHECK(!is_transferring_);

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->endpoint_id()) {
    // TODO(crbug.com/1119276): Support scanning for unknown share targets.
    RecordNearbyShareError(
        NearbyShareError::kSendAttachmentsUnknownShareTarget);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to send attachments. Unknown ShareTarget.";
    return StatusCodes::kError;
  }

  ShareTarget share_target_copy = share_target;
  for (std::unique_ptr<Attachment>& attachment : attachments) {
    DCHECK(attachment);
    attachment->MoveToShareTarget(share_target_copy);
  }

  if (!share_target_copy.has_attachments()) {
    RecordNearbyShareError(NearbyShareError::kSendAttachmentsNoAttachments);
    CD_LOG(WARNING, Feature::NS) << __func__ << ": No attachments to send.";
    return StatusCodes::kError;
  }

  // For sending advertisement from scanner, the request advertisement should
  // always be visible to everyone.
  std::optional<std::vector<uint8_t>> endpoint_info =
      CreateEndpointInfo(local_device_data_manager_->GetDeviceName());
  if (!endpoint_info) {
    RecordNearbyShareError(
        NearbyShareError::kSendAttachmentsCouldNotCreateLocalEndpointInfo);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Could not create local endpoint info.";
    return StatusCodes::kError;
  }

  info->set_transfer_update_callback(std::make_unique<TransferUpdateDecorator>(
      base::BindRepeating(&NearbySharingServiceImpl::OnOutgoingTransferUpdate,
                          weak_ptr_factory_.GetWeakPtr())));
  send_attachments_timestamp_ = base::TimeTicks::Now();
  OnTransferStarted(/*is_incoming=*/false);

  CHECK(info->endpoint_id().has_value());
  transfer_profiler_->OnShareTargetSelected(info->endpoint_id().value());
  for (auto& observer : observers_) {
    observer.OnShareTargetSelected(share_target);
  }

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
    RecordNearbyShareError(NearbyShareError::kAcceptUnknownShareTarget);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Accept invoked for unknown share target";
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  std::optional<std::pair<ShareTarget, TransferMetadata>> metadata =
      share_target.is_incoming ? last_incoming_metadata_
                               : last_outgoing_metadata_;
  if (!metadata || metadata->second.status() !=
                       TransferMetadata::Status::kAwaitingLocalConfirmation) {
    RecordNearbyShareError(
        NearbyShareError::kAcceptNotAwaitingLocalConfirmation);
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  is_waiting_to_record_accept_to_transfer_start_metric_ =
      share_target.is_incoming;

  for (auto& observer : observers_) {
    observer.OnTransferAccepted(share_target);
  }

  // This should probably always evaluate to true, since a sender will
  // never accept a transfer.
  DCHECK(share_target.is_incoming);
  if (share_target.is_incoming) {
    incoming_share_accepted_timestamp_ = base::TimeTicks::Now();

    CHECK(info->endpoint_id().has_value());
    transfer_profiler_->OnTransferAccepted(info->endpoint_id().value());

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
    RecordNearbyShareError(NearbyShareError::kRejectUnknownShareTarget);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Reject invoked for unknown share target";
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }
  NearbyConnection* connection = info->connection();

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NearbySharingServiceImpl::CloseConnection,
                     weak_ptr_factory_.GetWeakPtr(), share_target),
      kIncomingRejectionDelay);

  connection->SetDisconnectionListener(
      base::BindOnce(&NearbySharingServiceImpl::UnregisterShareTarget,
                     weak_ptr_factory_.GetWeakPtr(), share_target));

  WriteResponse(*connection, sharing::nearby::ConnectionResponseFrame::REJECT);
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Successfully wrote a rejection response frame";

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
  CD_LOG(INFO, Feature::NS) << __func__ << ": User cancelled transfer";
  locally_cancelled_share_target_ids_.insert(share_target.id);
  DoCancel(share_target, std::move(status_codes_callback),
           /*is_initiator_of_cancellation=*/true);
}

void NearbySharingServiceImpl::DoCancel(
    ShareTarget share_target,
    StatusCodesCallback status_codes_callback,
    bool is_initiator_of_cancellation) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->endpoint_id()) {
    RecordNearbyShareError(NearbyShareError::kCancelUnknownShareTarget);
    CD_LOG(ERROR, Feature::NS)
        << __func__
        << ": Cancel invoked for unknown share target, returning "
           "kOutOfOrderApiCall";
    // Make sure to clean up files just in case.
    RemoveIncomingPayloads(share_target);
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  // For metrics.
  all_cancelled_share_target_ids_.insert(share_target.id);

  // Cancel all ongoing payload transfers before invoking the transfer update
  // callback. Invoking the transfer update callback first could result in
  // payload cleanup before we have a chance to cancel the payload via Nearby
  // Connections, and the payload tracker might not receive the expected
  // cancellation signals. Also, note that there might not be any ongoing
  // payload transfer, for example, if a connection has not been established
  // yet.
  for (int64_t attachment_id : share_target.GetAttachmentIds()) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(attachment_id);
    if (payload_id) {
      nearby_connections_manager_->Cancel(*payload_id);
    }
  }

  // Inform the user that the transfer has been cancelled before disconnecting
  // because subsequent disconnections might be interpreted as failure. The
  // TransferUpdateDecorator will ignore subsequent statuses in favor of this
  // cancelled status. Note that the transfer update callback might have already
  // been invoked as a result of the payload cancellations above, but again,
  // superfluous status updates are handled gracefully by the
  // TransferUpdateDecorator.
  if (info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(
        share_target, TransferMetadataBuilder()
                          .set_status(TransferMetadata::Status::kCancelled)
                          .build());
  }

  // If a connection exists, close the connection. Note: The initiator of a
  // cancellation waits for a short delay before closing the connection,
  // allowing for final processing by the other device. Otherwise, disconnect
  // from endpoint id directly. Note: A share attempt can be cancelled by the
  // user before a connection is fully established, in which case,
  // info->connection() will be null.
  if (info->connection()) {
    if (is_initiator_of_cancellation) {
      info->connection()->SetDisconnectionListener(
          base::BindOnce(&NearbySharingServiceImpl::UnregisterShareTarget,
                         weak_ptr_factory_.GetWeakPtr(), share_target));
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&NearbySharingServiceImpl::CloseConnection,
                         weak_ptr_factory_.GetWeakPtr(), share_target),
          kInitiatorCancelDelay);
      WriteCancel(*info->connection());
    } else {
      info->connection()->Close();
    }
  } else {
    nearby_connections_manager_->Disconnect(*info->endpoint_id());
    UnregisterShareTarget(share_target);
  }

  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

bool NearbySharingServiceImpl::DidLocalUserCancelTransfer(
    const ShareTarget& share_target) {
  return base::Contains(locally_cancelled_share_target_ids_, share_target.id);
}

void NearbySharingServiceImpl::Open(const ShareTarget& share_target,
                                    StatusCodesCallback status_codes_callback) {
  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::OpenURL(GURL url) {
  DCHECK(profile_);
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

void NearbySharingServiceImpl::SetArcTransferCleanupCallback(
    base::OnceCallback<void()> callback) {
  // In the case where multiple Nearby Share sessions are started, successive
  // Nearby Share bubbles shown will prevent the user from sharing while the
  // initial bubble is still active. For the successive bubble(s), we want to
  // make sure only the original cleanup callback is valid.
  // Also in the following case:
  // 1. CrOS starts a receive transfer.
  // 2. ARC starts a send transfer and |arc_transfer_cleanup_callback_| is set
  //    erroneously if |is_transferring_| check is missing.
  // As multiple transfers cannot occur at the same time, a "Can't Share" error
  // will occur. When the transfer in [1] finishes and another ARC Nearby Share
  // session starts, the |arc_transfer_cleanup_callback_| can't be set if a
  // value is already set to ensure all clean up is performed. Hence, check if
  // not |is_transferring_| before setting |arc_transfer_cleanup_callback_|.
  if (!is_transferring_ && arc_transfer_cleanup_callback_.is_null()) {
    arc_transfer_cleanup_callback_ = std::move(callback);
  }
}

NearbyNotificationDelegate* NearbySharingServiceImpl::GetNotificationDelegate(
    const std::string& notification_id) {
  if (!nearby_notification_manager_) {
    return nullptr;
  }

  return nearby_notification_manager_->GetNotificationDelegate(notification_id);
}

void NearbySharingServiceImpl::RecordFastInitiationNotificationUsage(
    bool success) {
  fast_initiation_scanning_metrics_->RecordUsage(success);
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

NearbyNotificationManager* NearbySharingServiceImpl::GetNotificationManager() {
  return nearby_notification_manager_.get();
}

void NearbySharingServiceImpl::OnNearbyProcessStopped(
    NearbyProcessShutdownReason shutdown_reason) {
  DCHECK(process_reference_);
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Shutdown reason: " << shutdown_reason;
  CleanupAfterNearbyProcessStopped();
  ClearForegroundReceiveSurfaces();
  RestartNearbyProcessIfAppropriate(shutdown_reason);
  InvalidateSurfaceState();
  for (auto& observer : observers_) {
    observer.OnNearbyProcessStopped();
  }
}

void NearbySharingServiceImpl::CleanupAfterNearbyProcessStopped() {
  if (process_reference_) {
    process_reference_.reset();
  }

  SetInHighVisibility(false);

  endpoint_discovery_weak_ptr_factory_.InvalidateWeakPtrs();
  endpoint_discovery_events_ = base::queue<base::OnceClosure>();

  ClearOutgoingShareTargetInfoMap();
  incoming_share_target_info_map_.clear();
  discovered_advertisements_to_retry_map_.clear();

  foreground_send_transfer_callbacks_.Clear();
  background_send_transfer_callbacks_.Clear();
  foreground_send_discovery_callbacks_.Clear();
  background_send_discovery_callbacks_.Clear();

  last_incoming_metadata_.reset();
  last_outgoing_metadata_.reset();
  attachment_info_map_.clear();
  locally_cancelled_share_target_ids_.clear();

  mutual_acceptance_timeout_alarm_.Cancel();
  disconnection_timeout_alarms_.clear();

  is_scanning_ = false;
  is_transferring_ = false;
  is_receiving_files_ = false;
  is_sending_files_ = false;
  is_connecting_ = false;
  advertising_power_level_ = NearbyConnectionsManager::PowerLevel::kUnknown;

  process_shutdown_pending_timer_.Stop();
  certificate_download_during_discovery_timer_.Stop();
  rotate_background_advertisement_timer_.Stop();

  if (arc_transfer_cleanup_callback_) {
    // Cleanup send transfer resources where the user started ARC Nearby Share
    // but did not complete (i.e. cancel, abort, utility process stopped, etc.)
    // prior to shutdown.
    std::move(arc_transfer_cleanup_callback_).Run();
  }
}

void NearbySharingServiceImpl::RestartNearbyProcessIfAppropriate(
    NearbyProcessShutdownReason shutdown_reason) {
  if (!ShouldRestartNearbyProcess(shutdown_reason)) {
    return;
  }

  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Attempting to restart nearby process after shutdown: "
      << shutdown_reason;

  BindToNearbyProcess();

  // Track the number of process shutdowns that occur in a fixed time window.
  recent_nearby_process_unexpected_shutdown_count_++;
  if (!clear_recent_nearby_process_shutdown_count_timer_.IsRunning()) {
    clear_recent_nearby_process_shutdown_count_timer_.Start(
        FROM_HERE, kClearNearbyProcessUnexpectedShutdownCountDelay,
        base::BindOnce(&NearbySharingServiceImpl::
                           ClearRecentNearbyProcessUnexpectedShutdownCount,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

bool NearbySharingServiceImpl::ShouldRestartNearbyProcess(
    NearbyProcessShutdownReason shutdown_reason) {
  // Ensure Nearby Share is still enabled.
  if (!settings_.GetEnabled()) {
    CD_LOG(INFO, Feature::NS)
        << __func__
        << ": Choosing to not restart process because Nearby Share is "
           "disabled.";
    return false;
  }

  // Check if the current shutdown reason is one which we want to restart after.
  switch (shutdown_reason) {
    case NearbyProcessShutdownReason::kNormal:
      return false;
    case NearbyProcessShutdownReason::kCrash:
    case NearbyProcessShutdownReason::kConnectionsMojoPipeDisconnection:
    case NearbyProcessShutdownReason::kPresenceMojoPipeDisconnection:
    case NearbyProcessShutdownReason::kDecoderMojoPipeDisconnection:
      break;
  }

  // Check if the process shutdown count is above the allowed threshold.
  if (recent_nearby_process_unexpected_shutdown_count_ >
      NearbySharingServiceImpl::
          kMaxRecentNearbyProcessUnexpectedShutdownCount) {
    RecordNearbyShareError(NearbyShareError::kMaxNearbyProcessRestart);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Choosing to not restart process because the recent stop "
           "count has exceeded the threshold.";
    return false;
  }

  return true;
}

void NearbySharingServiceImpl::
    ClearRecentNearbyProcessUnexpectedShutdownCount() {
  recent_nearby_process_unexpected_shutdown_count_ = 0;
}

void NearbySharingServiceImpl::BindToNearbyProcess() {
  if (process_reference_ || !settings_.GetEnabled()) {
    RecordNearbyShareError(
        NearbyShareError::kBindToNearbyProcessReferenceExistsOrDisabled);
    return;
  }

  process_reference_ = process_manager_->GetNearbyProcessReference(
      base::BindOnce(&NearbySharingServiceImpl::OnNearbyProcessStopped,
                     base::Unretained(this)));

  if (!process_reference_) {
    RecordNearbyShareError(
        NearbyShareError::kBindToNearbyProcessFailedToGetReference);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to get a reference to the nearby process.";
  }
}

sharing::mojom::NearbySharingDecoder*
NearbySharingServiceImpl::GetNearbySharingDecoder() {
  BindToNearbyProcess();

  if (!process_reference_) {
    return nullptr;
  }

  sharing::mojom::NearbySharingDecoder* decoder =
      process_reference_->GetNearbySharingDecoder().get();

  if (!decoder) {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to get decoder from process reference.";
  }

  return decoder;
}

void NearbySharingServiceImpl::OnIncomingConnectionAccepted(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    NearbyConnection* connection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(connection);
  DCHECK(process_reference_);

  sharing::mojom::NearbySharingDecoder* decoder = GetNearbySharingDecoder();
  if (!decoder) {
    RecordNearbyShareError(
        NearbyShareError::kOnIncomingConnectionAcceptedFailedToGetDecoder);
    return;
  }

  // Sync down data from Nearby server when the receiving flow starts, making
  // our best effort to have fresh contact and certificate data. There is no
  // need to wait for these calls to finish. The periodic server requests will
  // typically be sufficient, but we don't want the user to be blocked for hours
  // waiting for a periodic sync.
  CD_LOG(VERBOSE, Feature::NS)
      << __func__
      << ": Downloading local device data, contacts, and certificates from "
      << "Nearby server at start of receiving flow.";
  local_device_data_manager_->DownloadDeviceData();
  contact_manager_->DownloadContacts();
  certificate_manager_->DownloadPublicCertificates();

  ShareTarget placeholder_share_target;
  placeholder_share_target.is_incoming = true;
  ShareTargetInfo& share_target_info =
      GetOrCreateShareTargetInfo(placeholder_share_target, endpoint_id);
  share_target_info.set_connection(connection);

  connection->SetDisconnectionListener(
      base::BindOnce(&NearbySharingServiceImpl::RefreshUIOnDisconnection,
                     weak_ptr_factory_.GetWeakPtr(), placeholder_share_target));

  decoder->DecodeAdvertisement(
      endpoint_info,
      base::BindOnce(&NearbySharingServiceImpl::OnIncomingAdvertisementDecoded,
                     weak_ptr_factory_.GetWeakPtr(), endpoint_id,
                     std::move(placeholder_share_target)));
}

void NearbySharingServiceImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": ConnectionType = " << type;
  on_network_changed_delay_timer_.Reset();
}

void NearbySharingServiceImpl::FlushMojoForTesting() {
  settings_receiver_.FlushForTesting();
}

void NearbySharingServiceImpl::OnEnabledChanged(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordNearbyShareEnabledMetric(GetNearbyShareEnabledState(prefs_));

  if (settings_.IsOnboardingComplete()) {
    base::UmaHistogramBoolean("Nearby.Share.EnabledStateChanged", enabled);
  }

  if (enabled) {
    CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Nearby sharing enabled!";
    local_device_data_manager_->Start();
    contact_manager_->Start();
    certificate_manager_->Start();
    BindToNearbyProcess();
  } else {
    CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Nearby sharing disabled!";
    StopAdvertising();
    StopScanning();
    nearby_connections_manager_->Shutdown();
    local_device_data_manager_->Stop();
    contact_manager_->Stop();
    certificate_manager_->Stop();
    process_reference_.reset();
  }

  UpdateVisibilityReminderTimer(/*reset_timestamp=*/false);
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::OnFastInitiationNotificationStateChanged(
    nearby_share::mojom::FastInitiationNotificationState state) {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Fast Initiation Notification state: " << state;
  // Runs through a series of checks to determine if background scanning should
  // be started or stopped.
  InvalidateReceiveSurfaceState();
}

void NearbySharingServiceImpl::OnDeviceNameChanged(
    const std::string& device_name) {
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Nearby sharing device name changed";
  // TODO(vecore): handle device name change
}

void NearbySharingServiceImpl::OnDataUsageChanged(
    nearby_share::mojom::DataUsage data_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Nearby sharing data usage changed to " << data_usage;
  StopAdvertisingAndInvalidateSurfaceState();
}

void NearbySharingServiceImpl::OnVisibilityChanged(
    nearby_share::mojom::Visibility new_visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Nearby sharing visibility changed to "
      << new_visibility;

  UpdateVisibilityReminderTimer(/*reset_timestamp=*/true);

  StopAdvertisingAndInvalidateSurfaceState();
}

void NearbySharingServiceImpl::OnAllowedContactsChanged(
    const std::vector<std::string>& allowed_contacts) {
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Nearby sharing visible contacts changed";
  // TODO(vecore): handle visible contacts change
}

void NearbySharingServiceImpl::OnPublicCertificatesDownloaded() {
  if (!is_scanning_ || discovered_advertisements_to_retry_map_.empty()) {
    return;
  }

  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Public certificates downloaded while scanning. "
      << "Retrying decryption with "
      << discovered_advertisements_to_retry_map_.size()
      << " previously discovered advertisements.";
  const auto map_copy = discovered_advertisements_to_retry_map_;
  discovered_advertisements_to_retry_map_.clear();
  for (const auto& id_info_pair : map_copy) {
    OnEndpointDiscovered(id_info_pair.first, id_info_pair.second);
  }
}

void NearbySharingServiceImpl::OnPrivateCertificatesChanged() {
  // If we are currently advertising, restart advertising using the updated
  // private certificates.
  if (rotate_background_advertisement_timer_.IsRunning()) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Private certificates changed; rotating background advertisement.";
    rotate_background_advertisement_timer_.FireNow();
  }
}

void NearbySharingServiceImpl::OnEndpointDiscovered(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info) {
  AddEndpointDiscoveryEvent(
      base::BindOnce(&NearbySharingServiceImpl::HandleEndpointDiscovered,
                     base::Unretained(this), endpoint_id, endpoint_info));
}

void NearbySharingServiceImpl::OnEndpointLost(const std::string& endpoint_id) {
  AddEndpointDiscoveryEvent(
      base::BindOnce(&NearbySharingServiceImpl::HandleEndpointLost,
                     base::Unretained(this), endpoint_id));
}

void NearbySharingServiceImpl::OnInitialMedium(const std::string& endpoint_id,
                                               const Medium medium) {
  // Our |share_target_map_| is populated in CreateShareTarget. This
  // is deterministically called *before* this method when sending,
  // and *after* this method when receiving. In other words, we can
  // expect to *not* record the initial medium when receiving.
  // We determined this acceptable as the initial medium when receiving
  // will always be Bluetooth, until other mediums are supported (Wifi LAN
  // can be an initial medium when sending due to mDNS discovery.)
  if (!share_target_map_.contains(endpoint_id)) {
    return;
  }
  RecordNearbyShareInitialConnectionMedium(medium);
  auto share_target = share_target_map_[endpoint_id];
  for (auto& observer : observers_) {
    observer.OnInitialMedium(share_target, medium);
  }
}

void NearbySharingServiceImpl::OnBandwidthUpgrade(
    const std::string& endpoint_id,
    const Medium medium) {
  transfer_profiler_->OnBandwidthUpgrade(endpoint_id, medium);

  // This gets triggered when connecting as a receiver.
  CHECK(share_target_map_.contains(endpoint_id));
  auto share_target = share_target_map_[endpoint_id];
  for (auto& observer : observers_) {
    observer.OnBandwidthUpgrade(share_target, medium);
  }
}

void NearbySharingServiceImpl::OnBandwidthUpgradeV3(
    nearby::presence::PresenceDevice remote_device,
    const Medium medium) {
  // Because `NearbySharingServiceImpl` is currently only consuming V1 APIs from
  // `NearbyConnections`, this function is to be left as `NOTIMPLEMENTED()` as
  // only Nearby Presence is using V3 APIs.
  NOTIMPLEMENTED();
}

void NearbySharingServiceImpl::OnLockStateChanged(bool locked) {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Screen lock state changed. (" << locked << ")";
  is_screen_locked_ = locked;

  // Set visibility to 'Your Devices' if the screen is locked and visibility is
  // not Hidden.
  nearby_share::mojom::Visibility current_visibility =
      settings_.GetVisibility();
  if (current_visibility != nearby_share::mojom::Visibility::kNoOne) {
    if (locked) {
      // Store old visibility setting.
      user_visibility_ = current_visibility;
      user_allowed_contacts_ = contact_manager_->GetAllowedContacts();

      // Set visibility to Your Devices.
      settings_.SetVisibility(nearby_share::mojom::Visibility::kYourDevices);
      contact_manager_->SetAllowedContacts(std::set<std::string>());
    } else {
      settings_.SetVisibility(user_visibility_);
      contact_manager_->SetAllowedContacts(user_allowed_contacts_);
    }
  }

  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::AdapterPresentChanged(
    device::BluetoothAdapter* adapter,
    bool present) {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Bluetooth present changed: " << present;
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Bluetooth powered changed: " << powered;
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::
    LowEnergyScanSessionHardwareOffloadingStatusChanged(
        device::BluetoothAdapter::LowEnergyScanSessionHardwareOffloadingStatus
            status) {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__
      << ": Bluetooth low energy scan session hardware offloading status : "
      << (status == device::BluetoothAdapter::
                        LowEnergyScanSessionHardwareOffloadingStatus::kSupported
              ? "enabled"
              : "disabled");
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::SuspendImminent() {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Suspend imminent.";
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::SuspendDone() {
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Suspend done.";
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
      NOTREACHED_IN_MIGRATION();
      return foreground_receive_callbacks_;
  }
}

bool NearbySharingServiceImpl::IsVisibleInBackground(
    nearby_share::mojom::Visibility visibility) {
  return isVisibleForAdvertising(visibility);
}

const std::optional<std::vector<uint8_t>>
NearbySharingServiceImpl::CreateEndpointInfo(
    const std::optional<std::string>& device_name) {
  std::vector<uint8_t> salt;
  std::vector<uint8_t> encrypted_key;

  nearby_share::mojom::Visibility visibility = settings_.GetVisibility();
  if (isVisibleForAdvertising(visibility)) {
    std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
        certificate_manager_->EncryptPrivateCertificateMetadataKey(visibility);
    if (encrypted_metadata_key) {
      salt = encrypted_metadata_key->salt();
      encrypted_key = encrypted_metadata_key->encrypted_key();
    } else {
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": Failed to encrypt private certificate metadata key "
          << "for advertisement.";
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
    return std::nullopt;
  }
}

void NearbySharingServiceImpl::GetBluetoothAdapter() {
  auto* adapter_factory = device::BluetoothAdapterFactory::Get();
  if (!adapter_factory->IsBluetoothSupported()) {
    RecordNearbyShareError(NearbyShareError::kGetBluetoothAdapterUnsupported);
    return;
  }

  // Because this will be called from the constructor, GetAdapter() may call
  // OnGetBluetoothAdapter() immediately which can cause problems during tests
  // since the class is not fully constructed yet.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
  fast_initiation_scanning_metrics_->SetBluetoothAdapter(adapter);

  // TODO(crbug/1147652): The call to update the advertising interval is
  // removed to prevent a Bluez crash. We need to either reduce the global
  // advertising interval asynchronously and wait for the result or use the
  // updated API referenced in the bug which allows setting a per-advertisement
  // interval.

  // TODO(crbug.com/1132469): This was added to fix an issue where advertising
  // was not starting on sign-in. Add a unit test to cover this case.
  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::StartFastInitiationAdvertising() {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Starting fast initiation advertising.";

  fast_initiation_advertiser_ =
      FastInitiationAdvertiser::Factory::Create(bluetooth_adapter_);

  // TODO(crbug/1147652): The call to update the advertising interval is
  // removed to prevent a Bluez crash. We need to either reduce the global
  // advertising interval asynchronously and wait for the result or use the
  // updated API referenced in the bug which allows setting a per-advertisement
  // interval.

  // TODO(crbug.com/1100686): Determine whether to call StartAdvertising() with
  // kNotify or kSilent.
  fast_initiation_advertiser_->StartAdvertising(
      FastInitiationAdvertiser::FastInitType::kNotify,
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
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Started advertising FastInitiation.";
}

void NearbySharingServiceImpl::OnStartFastInitiationAdvertisingError() {
  fast_initiation_advertiser_.reset();
  RecordNearbyShareError(
      NearbyShareError::kStartFastInitiationAdvertisingFailed);
  CD_LOG(ERROR, Feature::NS)
      << __func__ << ": Failed to start FastInitiation advertising.";
}

void NearbySharingServiceImpl::StopFastInitiationAdvertising() {
  if (!fast_initiation_advertiser_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Not advertising FastInitiation, ignoring.";
    return;
  }

  fast_initiation_advertiser_->StopAdvertising(
      base::BindOnce(&NearbySharingServiceImpl::OnStopFastInitiationAdvertising,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::OnStopFastInitiationAdvertising() {
  fast_initiation_advertiser_.reset();
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Stopped advertising FastInitiation";

  // TODO(crbug/1147652): The call to update the advertising interval is
  // removed to prevent a Bluez crash. We need to either reduce the global
  // advertising interval asynchronously and wait for the result or use the
  // updated API referenced in the bug which allows setting a per-advertisement
  // interval.
}

void NearbySharingServiceImpl::AddEndpointDiscoveryEvent(
    base::OnceClosure event) {
  endpoint_discovery_events_.push(std::move(event));
  if (endpoint_discovery_events_.size() == 1u) {
    std::move(endpoint_discovery_events_.front()).Run();
  }
}

void NearbySharingServiceImpl::HandleEndpointDiscovered(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": endpoint_id=" << endpoint_id
      << ", endpoint_info=" << base::HexEncode(endpoint_info);
  transfer_profiler_->OnEndpointDiscovered(endpoint_id);

  if (!is_scanning_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Ignoring discovered endpoint because we're no longer scanning";
    FinishEndpointDiscoveryEvent();
    return;
  }

  sharing::mojom::NearbySharingDecoder* decoder = GetNearbySharingDecoder();
  if (!decoder) {
    RecordNearbyShareError(
        NearbyShareError::HandleEndpointDiscoveredFailedToGetDecoder);
    FinishEndpointDiscoveryEvent();
    return;
  }

  decoder->DecodeAdvertisement(
      endpoint_info,
      base::BindOnce(&NearbySharingServiceImpl::OnOutgoingAdvertisementDecoded,
                     endpoint_discovery_weak_ptr_factory_.GetWeakPtr(),
                     endpoint_id, endpoint_info));
}

void NearbySharingServiceImpl::HandleEndpointLost(
    const std::string& endpoint_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": endpoint_id=" << endpoint_id;
  transfer_profiler_->OnEndpointLost(endpoint_id);

  if (!is_scanning_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Ignoring lost endpoint because we're no longer scanning";
    FinishEndpointDiscoveryEvent();
    return;
  }

  discovered_advertisements_to_retry_map_.erase(endpoint_id);
  RemoveOutgoingShareTargetWithEndpointId(endpoint_id);
  FinishEndpointDiscoveryEvent();
}

void NearbySharingServiceImpl::FinishEndpointDiscoveryEvent() {
  DCHECK(!endpoint_discovery_events_.empty());
  DCHECK(endpoint_discovery_events_.front().is_null());
  endpoint_discovery_events_.pop();

  // Handle the next queued up endpoint discovered/lost event.
  if (!endpoint_discovery_events_.empty()) {
    DCHECK(!endpoint_discovery_events_.front().is_null());
    std::move(endpoint_discovery_events_.front()).Run();
  }
}

void NearbySharingServiceImpl::OnOutgoingAdvertisementDecoded(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    sharing::mojom::AdvertisementPtr advertisement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!advertisement) {
    RecordNearbyShareError(
        NearbyShareError::kOutgoingAdvertisementDecodedFailedToParse);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to parse discovered advertisement.";
    FinishEndpointDiscoveryEvent();
    return;
  }

  transfer_profiler_->OnOutgoingEndpointDecoded(endpoint_id);

  // Now we will report endpoints met before in NearbyConnectionsManager.
  // Check outgoingShareTargetInfoMap first and pass the same shareTarget if we
  // found one.

  // Looking for the ShareTarget based on endpoint id.
  if (outgoing_share_target_map_.find(endpoint_id) !=
      outgoing_share_target_map_.end()) {
    FinishEndpointDiscoveryEvent();
    return;
  }

  // Once we get the advertisement, the first thing to do is decrypt the
  // certificate.
  NearbyShareEncryptedMetadataKey encrypted_metadata_key(
      advertisement->salt, advertisement->encrypted_metadata_key);
  GetCertificateManager()->GetDecryptedPublicCertificate(
      std::move(encrypted_metadata_key),
      base::BindOnce(&NearbySharingServiceImpl::OnOutgoingDecryptedCertificate,
                     endpoint_discovery_weak_ptr_factory_.GetWeakPtr(),
                     endpoint_id, endpoint_info, std::move(advertisement)));
}

void NearbySharingServiceImpl::OnOutgoingDecryptedCertificate(
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    sharing::mojom::AdvertisementPtr advertisement,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  // Check again for this endpoint id, to avoid race conditions.
  if (outgoing_share_target_map_.find(endpoint_id) !=
      outgoing_share_target_map_.end()) {
    FinishEndpointDiscoveryEvent();
    return;
  }

  // The certificate provides the device name, in order to create a ShareTarget
  // to represent this remote device.
  std::optional<ShareTarget> share_target = CreateShareTarget(
      endpoint_id, std::move(advertisement), std::move(certificate),
      /*is_incoming=*/false);
  if (!share_target) {
    RecordNearbyShareError(
        NearbyShareError::
            kOutgoingDecryptedCertificateFailedToCreateShareTarget);
    CD_LOG(INFO, Feature::NS)
        << __func__ << ": Failed to convert discovered advertisement to share "
        << "target. Ignoring endpoint until next certificate download.";
    discovered_advertisements_to_retry_map_[endpoint_id] = endpoint_info;
    FinishEndpointDiscoveryEvent();
    return;
  }

  // Update the endpoint id for the share target.
  CD_LOG(INFO, Feature::NS)
      << __func__
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

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Reported OnShareTargetDiscovered "
      << (base::Time::Now() - scanning_start_timestamp_);

  // TODO(crbug/1108348) CachingManager should cache known and non-external
  // share targets.

  FinishEndpointDiscoveryEvent();
}

void NearbySharingServiceImpl::ScheduleCertificateDownloadDuringDiscovery(
    size_t download_count) {
  if (download_count >= kMaxCertificateDownloadsDuringDiscovery) {
    return;
  }

  certificate_download_during_discovery_timer_.Start(
      FROM_HERE, kCertificateDownloadDuringDiscoveryPeriod,
      base::BindOnce(&NearbySharingServiceImpl::
                         OnCertificateDownloadDuringDiscoveryTimerFired,
                     weak_ptr_factory_.GetWeakPtr(), download_count));
}

void NearbySharingServiceImpl::OnCertificateDownloadDuringDiscoveryTimerFired(
    size_t download_count) {
  if (!is_scanning_) {
    return;
  }

  if (!discovered_advertisements_to_retry_map_.empty()) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Detected "
        << discovered_advertisements_to_retry_map_.size()
        << " discovered advertisements that could not decrypt any "
        << "public certificates. Re-downloading certificates.";
    certificate_manager_->DownloadPublicCertificates();
    ++download_count;
  }
  ScheduleCertificateDownloadDuringDiscovery(download_count);
}

bool NearbySharingServiceImpl::IsBluetoothPresent() const {
  return bluetooth_adapter_.get() && bluetooth_adapter_->IsPresent();
}

bool NearbySharingServiceImpl::IsBluetoothPowered() const {
  return IsBluetoothPresent() && bluetooth_adapter_->IsPowered();
}

bool NearbySharingServiceImpl::HasAvailableAdvertisingMediums() {
  // Advertising is currently unsupported unless bluetooth is known to be
  // enabled. When Wifi LAN advertising (mDNS) is supported, we also need
  // to check network conditions.
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  bool hasNetworkConnection =
      connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI ||
      connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET;
  return IsBluetoothPowered() ||
         (hasNetworkConnection && kIsWifiLanAdvertisingSupported);
}

bool NearbySharingServiceImpl::HasAvailableDiscoveryMediums() {
  // Discovery is supported over both Bluetooth and Wifi LAN (mDNS),
  // so either of those mediums must be enabled. mDNS discovery
  // additionally needs a network connection.
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::GetConnectionType();
  bool hasNetworkConnection =
      connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI ||
      connection_type ==
          net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET;
  return IsBluetoothPowered() ||
         (hasNetworkConnection && ::features::IsNearbyMdnsEnabled());
}

void NearbySharingServiceImpl::InvalidateSurfaceState() {
  InvalidateSendSurfaceState();
  InvalidateReceiveSurfaceState();
  if (process_reference_ && ShouldStopNearbyProcess()) {
    // We need to debounce the call to shut down the process in case this state
    // is temporary (we don't want to the thrash the process). Any
    // advertisement, scanning or transferring will stop this timer from
    // triggering.
    if (!process_shutdown_pending_timer_.IsRunning()) {
      CD_LOG(INFO, Feature::NS)
          << __func__
          << ": Scheduling process shutdown if not needed in 15 seconds";
      // NOTE: Using base::Unretained is safe because if shutdown_pending_timer_
      // goes out of scope the timer will be cancelled.
      process_shutdown_pending_timer_.Start(
          FROM_HERE, kProcessShutdownPendingTimerDelay,
          base::BindOnce(&NearbySharingServiceImpl::OnProcessShutdownTimerFired,
                         base::Unretained(this)));
    }
  } else {
    process_shutdown_pending_timer_.Stop();
  }
}

bool NearbySharingServiceImpl::ShouldStopNearbyProcess() {
  // Nothing to do if we're shutting down the profile.
  if (!profile_) {
    return false;
  }

  // We're currently advertising.
  if (advertising_power_level_ !=
      NearbyConnectionsManager::PowerLevel::kUnknown) {
    return false;
  }

  // We're currently discovering.
  if (is_scanning_) {
    return false;
  }

  // We're currently attempting to connect to a remote device.
  if (is_connecting_) {
    return false;
  }

  // We're currently sending or receiving a file.
  if (is_transferring_) {
    return false;
  }

  // We're not using NearbyConnections, should stop the process.
  return true;
}

void NearbySharingServiceImpl::OnProcessShutdownTimerFired() {
  if (ShouldStopNearbyProcess() && process_reference_) {
    CD_LOG(INFO, Feature::NS)
        << __func__
        << ": Shutdown Process timer fired, releasing process reference";

    // Manually firing this callback will handle destroying
    // |process_reference_|.
    //
    // The NearbyProcessManager would ordinarily be responsible for firing this
    // callback, but it assumes that is unnecessary if the owner destroys the
    // process reference, so we're responsible for calling it to ensure that
    // downstream listeners are notified.
    OnNearbyProcessStopped(NearbyProcessShutdownReason::kNormal);
  }
}

void NearbySharingServiceImpl::InvalidateSendSurfaceState() {
  InvalidateScanningState();
  InvalidateFastInitiationAdvertising();
}

void NearbySharingServiceImpl::InvalidateScanningState() {
  // Nothing to do if we're shutting down the profile.
  if (!profile_) {
    return;
  }

  if (power_client_->IsSuspended()) {
    StopScanning();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Stopping discovery because the system is suspended.";
    return;
  }

  // Screen is off. Do no work.
  if (is_screen_locked_) {
    StopScanning();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Stopping discovery because the screen is locked.";
    return;
  }

  if (!HasAvailableDiscoveryMediums()) {
    StopScanning();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping scanning because both bluetooth and wifi LAN are "
           "disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't advertise.
  if (!settings_.GetEnabled()) {
    StopScanning();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping discovery because Nearby Sharing is disabled.";
    return;
  }

  if (is_transferring_ || is_connecting_) {
    StopScanning();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping discovery because we're currently in the midst of a "
           "transfer.";
    return;
  }

  if (foreground_send_transfer_callbacks_.empty()) {
    StopScanning();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping discovery because no scanning surface has been "
           "registered.";
    return;
  }

  process_shutdown_pending_timer_.Stop();
  // Screen is on, Bluetooth is enabled, and Nearby Sharing is enabled! Start
  // discovery.
  StartScanning();
}

void NearbySharingServiceImpl::InvalidateFastInitiationAdvertising() {
  // Nothing to do if we're shutting down the profile.
  if (!profile_) {
    return;
  }

  if (power_client_->IsSuspended()) {
    StopFastInitiationAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping fast init advertising because the system is suspended.";
    return;
  }

  // Screen is off. Do no work.
  if (is_screen_locked_) {
    StopFastInitiationAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping fast init advertising because the screen is locked.";
    return;
  }

  if (!IsBluetoothPowered()) {
    StopFastInitiationAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping fast init advertising because both "
           "bluetooth is disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't fast init advertise.
  if (!settings_.GetEnabled()) {
    StopFastInitiationAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping fast init advertising because Nearby "
           "Sharing is disabled.";
    return;
  }

  if (foreground_send_transfer_callbacks_.empty()) {
    StopFastInitiationAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping fast init advertising because no send "
           "surface is registered.";
    return;
  }

  if (fast_initiation_advertiser_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Already advertising fast init, ignoring.";
    return;
  }

  process_shutdown_pending_timer_.Stop();

  StartFastInitiationAdvertising();
}

void NearbySharingServiceImpl::InvalidateReceiveSurfaceState() {
  InvalidateAdvertisingState();
  InvalidateFastInitiationScanning();
}

void NearbySharingServiceImpl::InvalidateAdvertisingState() {
  // Nothing to do if we're shutting down the profile.
  if (!profile_) {
    return;
  }

  if (power_client_->IsSuspended()) {
    StopAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping advertising because the system is suspended.";
    return;
  }

  if (!HasAvailableAdvertisingMediums()) {
    StopAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping advertising because both bluetooth and wifi LAN are "
           "disabled.";
    return;
  }

  // Nearby Sharing is disabled. Don't advertise.
  if (!settings_.GetEnabled()) {
    StopAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping advertising because Nearby Sharing is disabled.";
    return;
  }

  // We're scanning for other nearby devices. Don't advertise.
  if (is_scanning_) {
    StopAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping advertising because we're scanning for other devices.";
    return;
  }

  if (is_transferring_) {
    StopAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping advertising because we're currently in the midst of "
           "a transfer.";
    return;
  }

  if (foreground_receive_callbacks_.empty() &&
      background_receive_callbacks_.empty()) {
    StopAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping advertising because no receive surface is registered.";
    return;
  }

  if (!IsVisibleInBackground(settings_.GetVisibility()) &&
      foreground_receive_callbacks_.empty()) {
    StopAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping advertising because no high power receive surface "
           "is registered and device is visible to NO_ONE.";
    return;
  }

  process_shutdown_pending_timer_.Stop();

  NearbyConnectionsManager::PowerLevel power_level;
  if (!foreground_receive_callbacks_.empty()) {
    power_level = NearbyConnectionsManager::PowerLevel::kHighPower;
    // TODO(crbug/1100367) handle fast init
    // } else if (isFastInitDeviceNearby) {
    //   power_level = NearbyConnectionsManager::PowerLevel::kMediumPower;
  } else {
    power_level = NearbyConnectionsManager::PowerLevel::kLowPower;
  }

  nearby_share::mojom::DataUsage data_usage = settings_.GetDataUsage();
  if (advertising_power_level_ !=
      NearbyConnectionsManager::PowerLevel::kUnknown) {
    if (power_level == advertising_power_level_) {
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Ignoring, already advertising with power level "
          << PowerLevelToString(advertising_power_level_)
          << " and data usage preference " << data_usage;
      return;
    }

    StopAdvertising();
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Restart advertising with power level "
        << PowerLevelToString(power_level) << " and data usage preference "
        << data_usage;
  }

  std::optional<std::string> device_name;
  if (!foreground_receive_callbacks_.empty()) {
    device_name = local_device_data_manager_->GetDeviceName();
  }

  // Starts advertising through Nearby Connections. Caller is expected to ensure
  // |listener| remains valid until StopAdvertising is called.
  std::optional<std::vector<uint8_t>> endpoint_info =
      CreateEndpointInfo(device_name);
  if (!endpoint_info) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Unable to advertise since could not parse the "
           "endpoint info from the advertisement.";
    return;
  }

  // TODO(crbug/1147652): The call to update the advertising interval is
  // removed to prevent a Bluez crash. We need to either reduce the global
  // advertising interval asynchronously and wait for the result or use the
  // updated API referenced in the bug which allows setting a per-advertisement
  // interval.

  // TODO(crbug/1155669): This will suppress the system notification that
  // alerts the user that their device is discoverable, but it exposes Nearby
  // Share logic to external components. We should clean this up with a better
  // abstraction.
  bool used_device_name = device_name.has_value();
  if (used_device_name) {
    for (auto& observer : observers_) {
      observer.OnHighVisibilityChangeRequested();
    }
  }

  nearby_connections_manager_->StartAdvertising(
      *endpoint_info,
      /*listener=*/this, power_level, data_usage,
      base::BindOnce(&NearbySharingServiceImpl::OnStartAdvertisingResult,
                     weak_ptr_factory_.GetWeakPtr(), used_device_name));

  advertising_power_level_ = power_level;
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": StartAdvertising requested over Nearby Connections: "
      << " power level: " << PowerLevelToString(power_level)
      << " visibility: " << settings_.GetVisibility()
      << " data usage: " << data_usage << " advertise device name?: "
      << (device_name.has_value() ? "yes" : "no");

  ScheduleRotateBackgroundAdvertisementTimer();
}

void NearbySharingServiceImpl::StopAdvertising() {
  if (advertising_power_level_ ==
      NearbyConnectionsManager::PowerLevel::kUnknown) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Not currently advertising, ignoring.";
    return;
  }

  nearby_connections_manager_->StopAdvertising(
      base::BindOnce(&NearbySharingServiceImpl::OnStopAdvertisingResult,
                     weak_ptr_factory_.GetWeakPtr()));

  // TODO(crbug/1147652): The call to update the advertising interval is
  // removed to prevent a Bluez crash. We need to either reduce the global
  // advertising interval asynchronously and wait for the result or use the
  // updated API referenced in the bug which allows setting a per-advertisement
  // interval.
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Stop advertising requested";

  // Set power level to unknown immediately instead of waiting for the callback.
  // In the case of restarting advertising (e.g. turning off high visibility
  // with contact-based enabled), StartAdvertising will be called
  // immediately after StopAdvertising and will fail if the power level
  // indicates already advertising.
  advertising_power_level_ = NearbyConnectionsManager::PowerLevel::kUnknown;
}

void NearbySharingServiceImpl::StartScanning() {
  DCHECK(profile_);
  DCHECK(!power_client_->IsSuspended());
  DCHECK(settings_.GetEnabled());
  DCHECK(!is_screen_locked_);
  DCHECK(HasAvailableDiscoveryMediums());
  DCHECK(!foreground_send_transfer_callbacks_.empty());

  if (is_scanning_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": We're currently scanning, ignoring.";
    return;
  }

  scanning_start_timestamp_ = base::Time::Now();
  is_scanning_ = true;
  InvalidateReceiveSurfaceState();

  ClearOutgoingShareTargetInfoMap();
  discovered_advertisements_to_retry_map_.clear();

  nearby_connections_manager_->StartDiscovery(
      /*listener=*/this, settings_.GetDataUsage(),
      base::BindOnce(&NearbySharingServiceImpl::OnStartDiscoveryResult,
                     weak_ptr_factory_.GetWeakPtr()));

  InvalidateSendSurfaceState();
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Scanning has started";
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::StopScanning() {
  if (!is_scanning_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Not currently scanning, ignoring.";
    return StatusCodes::kStatusAlreadyStopped;
  }

  nearby_connections_manager_->StopDiscovery();
  is_scanning_ = false;

  // TODO(b/313950374): This should really happen when NC actually stops
  // discovery, which could happen outside of this function.
  for (auto& observer : observers_) {
    observer.OnShareTargetDiscoveryStopped();
  }

  certificate_download_during_discovery_timer_.Stop();
  discovered_advertisements_to_retry_map_.clear();

  // Note: We don't know if we stopped scanning in preparation to send a file,
  // or we stopped because the user left the page. We'll invalidate after a
  // short delay.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NearbySharingServiceImpl::InvalidateSurfaceState,
                     weak_ptr_factory_.GetWeakPtr()),
      kInvalidateDelay);

  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Scanning has stopped.";
  return StatusCodes::kOk;
}

void NearbySharingServiceImpl::StopAdvertisingAndInvalidateSurfaceState() {
  if (advertising_power_level_ !=
      NearbyConnectionsManager::PowerLevel::kUnknown) {
    StopAdvertising();
  }

  InvalidateSurfaceState();
}

void NearbySharingServiceImpl::InvalidateFastInitiationScanning() {
  bool is_hardware_offloading_supported =
      IsBluetoothPresent() &&
      FastInitiationScanner::Factory::IsHardwareSupportAvailable(
          bluetooth_adapter_.get());

  // Hardware offloading support is computed when the bluetooth adapter becomes
  // available. We set the hardware supported state on |settings_| to notify the
  // UI of state changes. InvalidateFastInitiationScanning gets triggered on
  // adapter change events.
  settings_.SetIsFastInitiationHardwareSupported(
      is_hardware_offloading_supported);

  // Nothing to do if we're shutting down the profile.
  if (!profile_) {
    return;
  }

  if (fast_initiation_scanner_cooldown_timer_.IsRunning()) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning due to post-transfer "
           "cooldown period";
    StopFastInitiationScanning();
    return;
  }

  if (settings_.GetFastInitiationNotificationState() !=
      nearby_share::mojom::FastInitiationNotificationState::kEnabled) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning; fast initiation "
           "notification is disabled";
    StopFastInitiationScanning();
    return;
  }

  if (GetNearbyShareEnabledState(prefs_) ==
      NearbyShareEnabledState::kDisallowedByPolicy) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning because Nearby Sharing "
           "is disallowed by policy ";
    StopFastInitiationScanning();
    return;
  }

  if (power_client_->IsSuspended()) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning because the system is suspended.";
    StopFastInitiationScanning();
    return;
  }

  // Screen is off. Do no work.
  if (is_screen_locked_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning because the screen is locked.";
    StopFastInitiationScanning();
    return;
  }

  if (!IsBluetoothPowered()) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning because bluetooth is powered down.";
    StopFastInitiationScanning();
    return;
  }

  // We're scanning for other nearby devices. Don't background scan.
  if (is_scanning_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning because we're scanning "
           "for other devices.";
    StopFastInitiationScanning();
    return;
  }

  if (is_transferring_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning because we're currently "
           "in the midst of "
           "a transfer.";
    StopFastInitiationScanning();
    return;
  }

  if (advertising_power_level_ ==
      NearbyConnectionsManager::PowerLevel::kHighPower) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning because we're already "
           "in high visibility mode.";
    StopFastInitiationScanning();
    return;
  }

  if (!is_hardware_offloading_supported) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Stopping background scanning because hardware "
           "support is not available or not ready.";
    StopFastInitiationScanning();
    return;
  }

  process_shutdown_pending_timer_.Stop();

  if (fast_initiation_scanner_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Ignoring, already background scanning.";
    return;
  }

  StartFastInitiationScanning();
}

void NearbySharingServiceImpl::StartFastInitiationScanning() {
  DCHECK(!fast_initiation_scanner_);
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Starting background scanning.";
  fast_initiation_scanner_ =
      FastInitiationScanner::Factory::Create(bluetooth_adapter_);
  fast_initiation_scanner_->StartScanning(
      base::BindRepeating(
          &NearbySharingServiceImpl::OnFastInitiationDevicesDetected,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(
          &NearbySharingServiceImpl::OnFastInitiationDevicesNotDetected,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&NearbySharingServiceImpl::StopFastInitiationScanning,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::OnFastInitiationDevicesDetected() {
  CD_LOG(VERBOSE, Feature::NS) << __func__;
  for (auto& observer : observers_) {
    observer.OnFastInitiationDevicesDetected();
  }
}

void NearbySharingServiceImpl::OnFastInitiationDevicesNotDetected() {
  CD_LOG(VERBOSE, Feature::NS) << __func__;
  for (auto& observer : observers_) {
    observer.OnFastInitiationDevicesNotDetected();
  }
}

void NearbySharingServiceImpl::StopFastInitiationScanning() {
  if (!fast_initiation_scanner_) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Ignoring, not background scanning.";
    return;
  }

  fast_initiation_scanner_.reset();
  for (auto& observer : observers_) {
    observer.OnFastInitiationScanningStopped();
  }
  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Stopped background scanning.";
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
      base::Milliseconds(base::checked_cast<uint64_t>(delayMilliseconds)),
      base::BindOnce(
          &NearbySharingServiceImpl::OnRotateBackgroundAdvertisementTimerFired,
          weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::OnRotateBackgroundAdvertisementTimerFired() {
  if (!foreground_receive_callbacks_.empty()) {
    ScheduleRotateBackgroundAdvertisementTimer();
  } else {
    StopAdvertising();
    InvalidateSurfaceState();
  }
}

void NearbySharingServiceImpl::RemoveOutgoingShareTargetWithEndpointId(
    const std::string& endpoint_id) {
  auto it = outgoing_share_target_map_.find(endpoint_id);
  if (it == outgoing_share_target_map_.end()) {
    return;
  }

  // Share target state needs to be cleared before the move below.
  share_target_map_.erase(endpoint_id);
  transfer_size_map_.erase(endpoint_id);

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Removing (endpoint_id=" << it->first
      << ", share_target.id=" << it->second.id
      << ") from outgoing share target map";
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

  for (auto& observer : observers_) {
    observer.OnShareTargetRemoved(share_target);
  }

  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Reported OnShareTargetLost";
}

void NearbySharingServiceImpl::OnTransferComplete() {
  bool was_sending_files = is_sending_files_;
  is_receiving_files_ = false;
  is_transferring_ = false;
  is_sending_files_ = false;

  // Cleanup ARC after send transfer completes since reading from file
  // descriptor(s) are done at this point even though there could be Nearby
  // Connection frames cached that are not yet sent to the remote device.
  if (was_sending_files && arc_transfer_cleanup_callback_) {
    std::move(arc_transfer_cleanup_callback_).Run();
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": NearbySharing state change transfer finished";
  // Files transfer is done! Receivers can immediately cancel, but senders
  // should add a short delay to ensure the final in-flight packet(s) make
  // it to the remote device.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&NearbySharingServiceImpl::InvalidateSurfaceState,
                     weak_ptr_factory_.GetWeakPtr()),
      was_sending_files ? kInvalidateSurfaceStateDelayAfterTransferDone
                        : base::TimeDelta());
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
      DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
          ->DownloadPath();

  // Register payload path for all valid file payloads.
  base::flat_map<int64_t, base::FilePath> valid_file_payloads;
  for (auto& file : share_target.file_attachments) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(file.id());
    if (!payload_id) {
      CD_LOG(WARNING, Feature::NS)
          << __func__
          << ": Failed to register payload path for attachment id - "
          << file.id();
      continue;
    }

    base::FilePath file_path = download_path.Append(file.file_name());
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
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(payload.first);
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
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Preparing to send payloads to " << share_target.id;
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(NearbyShareError::kSendPayloadsMissingConnection);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to send payload due to missing connection.";
    return StatusCodes::kOutOfOrderApiCall;
  }
  if (!info->transfer_update_callback()) {
    RecordNearbyShareError(
        NearbyShareError::kSendPayloadsMissingTransferUpdateCallback);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Failed to send payload due to missing transfer update "
           "callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return StatusCodes::kOutOfOrderApiCall;
  }

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_token(info->token())
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .build());

  if (!info->endpoint_id()) {
    RecordNearbyShareError(NearbyShareError::kSendPayloadsMissingEndpointId);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to send payload due to missing endpoint id.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingEndpointId, share_target);
    return StatusCodes::kOutOfOrderApiCall;
  }

  ReceiveConnectionResponse(share_target);
  return StatusCodes::kOk;
}

void NearbySharingServiceImpl::OnUniquePathFetched(
    int64_t attachment_id,
    int64_t payload_id,
    base::OnceCallback<void(nearby::connections::mojom::Status)> callback,
    base::FilePath path) {
  attachment_info_map_[attachment_id].file_path = path;
  nearby_connections_manager_->RegisterPayloadPath(payload_id, path,
                                                   std::move(callback));
}

void NearbySharingServiceImpl::OnPayloadPathRegistered(
    base::ScopedClosureRunner closure_runner,
    bool* aggregated_success,
    ::nearby::connections::mojom::Status status) {
  if (status != ::nearby::connections::mojom::Status::kSuccess) {
    *aggregated_success = false;
  }
}

void NearbySharingServiceImpl::OnPayloadPathsRegistered(
    const ShareTarget& share_target,
    std::unique_ptr<bool> aggregated_success,
    StatusCodesCallback status_codes_callback) {
  DCHECK(aggregated_success);
  if (!*aggregated_success) {
    RecordNearbyShareError(NearbyShareError::kPayloadPathsRegisteredFailed);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Not all payload paths could be registered successfully.";
    std::move(status_codes_callback).Run(StatusCodes::kError);
    return;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(
        NearbyShareError::kPayloadPathsRegisteredUnknownShareTarget);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Accept invoked for unknown share target";
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    RecordNearbyShareError(
        NearbyShareError::kPayloadPathsRegisteredMissingTransferUpdateCallback);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Accept invoked for share target without transfer "
           "update callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  info->set_payload_tracker(std::make_unique<PayloadTracker>(
      share_target, attachment_info_map_,
      base::BindRepeating(&NearbySharingServiceImpl::OnPayloadTransferUpdate,
                          weak_ptr_factory_.GetWeakPtr())));

  // Register status listener for all payloads.
  for (int64_t attachment_id : share_target.GetAttachmentIds()) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(attachment_id);
    if (!payload_id) {
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": Failed to retrieve payload for attachment id - "
          << attachment_id;
      continue;
    }

    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Started listening for progress on payload - "
        << *payload_id;

    nearby_connections_manager_->RegisterPayloadStatusListener(
        *payload_id, info->payload_tracker());

    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Accepted incoming files from share target - "
        << share_target.id;
  }

  WriteResponse(*connection, sharing::nearby::ConnectionResponseFrame::ACCEPT);
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Successfully wrote response frame";

  // Receiver event
  for (auto& observer : observers_) {
    observer.OnTransferStarted(share_target,
                               transfer_size_map_[info->endpoint_id().value()]);
  }

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
          .set_token(info->token())
          .build());

  std::optional<std::string> endpoint_id = info->endpoint_id();
  if (endpoint_id) {
    // Upgrade bandwidth regardless of advertising visibility because either
    // the system or the user has verified the sender's identity; the
    // stable identifiers potentially exposed by performing a bandwidth
    // upgrade are no longer a concern.
    nearby_connections_manager_->UpgradeBandwidth(*endpoint_id);
  } else {
    RecordNearbyShareError(
        NearbyShareError::kPayloadPathsRegisteredMissingEndpointId);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Failed to initiate bandwidth upgrade. No endpoint_id "
           "found for target - "
        << share_target.id;
    std::move(status_codes_callback).Run(StatusCodes::kOutOfOrderApiCall);
    return;
  }

  std::move(status_codes_callback).Run(StatusCodes::kOk);
}

void NearbySharingServiceImpl::OnOutgoingConnection(
    const ShareTarget& share_target,
    base::TimeTicks connect_start_time,
    NearbyConnection* connection) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  bool success = info && info->endpoint_id() && connection;
  RecordNearbyShareEstablishConnectionMetrics(
      success, /*cancelled=*/
      base::Contains(all_cancelled_share_target_ids_, share_target.id),
      base::TimeTicks::Now() - connect_start_time);

  if (!success) {
    RecordNearbyShareError(
        NearbyShareError::kOutgoingConnectionFailedtoInitiateConnection);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to initate connection to share target "
        << share_target.id;
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kFailedToInitiateOutgoingConnection,
        share_target);
    return;
  }

  info->set_connection(connection);

  CHECK(info->endpoint_id().has_value());
  transfer_profiler_->OnConnectionEstablished(info->endpoint_id().value());
  for (auto& observer : observers_) {
    observer.OnShareTargetConnected(share_target);
  }

  connection->SetDisconnectionListener(base::BindOnce(
      &NearbySharingServiceImpl::OnOutgoingConnectionDisconnected,
      weak_ptr_factory_.GetWeakPtr(), share_target));

  std::optional<std::string> four_digit_token =
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
    std::optional<std::string> four_digit_token) {
  // We successfully connected! Now lets build up Payloads for all the files we
  // want to send them. We won't send any just yet, but we'll send the Payload
  // IDs in our our introduction frame so that they know what to expect if they
  // accept.
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Preparing to send introduction to " << share_target.id;

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(
        NearbyShareError::kSendIntroductionFailedToGetShareTarget);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": No NearbyConnection tied to " << share_target.id;
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    RecordNearbyShareError(
        NearbyShareError::kSendIntroductionMissingTransferUpdateCallback);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": No transfer update callback, disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return;
  }

  if (foreground_send_transfer_callbacks_.empty() &&
      background_send_transfer_callbacks_.empty()) {
    RecordNearbyShareError(
        NearbyShareError::kSendIntroductionNoSendTransferCallbacks);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": No transfer callbacks, disconnecting.";
    connection->Close();
    return;
  }

  // Build the introduction.
  auto introduction = std::make_unique<sharing::nearby::IntroductionFrame>();
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Sending attachments to " << share_target.id;

  // Write introduction of file payloads.
  int64_t transfer_size = 0;
  for (const auto& file : share_target.file_attachments) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(file.id());
    if (!payload_id) {
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Skipping unknown file attachment";
      continue;
    }
    auto* file_metadata = introduction->add_file_metadata();
    file_metadata->set_id(file.id());
    file_metadata->set_name(file.file_name());
    file_metadata->set_payload_id(*payload_id);
    file_metadata->set_type(sharing::ConvertFileMetadataType(file.type()));
    file_metadata->set_mime_type(file.mime_type());
    file_metadata->set_size(file.size());
    transfer_size += file.size();
  }

  // Write introduction of text payloads.
  for (const auto& text : share_target.text_attachments) {
    std::optional<int64_t> payload_id = GetAttachmentPayloadId(text.id());
    if (!payload_id) {
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Skipping unknown text attachment";
      continue;
    }
    auto* text_metadata = introduction->add_text_metadata();
    text_metadata->set_id(text.id());
    text_metadata->set_text_title(text.text_title());
    text_metadata->set_type(sharing::ConvertTextMetadataType(text.type()));
    text_metadata->set_size(text.size());
    text_metadata->set_payload_id(*payload_id);
    transfer_size += text.size();
  }

  if (introduction->file_metadata_size() == 0 &&
      introduction->text_metadata_size() == 0) {
    RecordNearbyShareError(NearbyShareError::kSendIntroductionNoPayloads);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": No payloads tied to transfer, disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingPayloads, share_target);
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
  RecordNearbyShareTimeFromInitiateSendToRemoteDeviceNotificationMetric(
      base::TimeTicks::Now() - send_attachments_timestamp_);
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Successfully wrote the introduction frame";

  CHECK(info->endpoint_id().has_value());
  transfer_profiler_->OnIntroductionFrameSent(info->endpoint_id().value());

  // Store the file size for use when the transfer actually begins.
  transfer_size_map_[info->endpoint_id().value()] = transfer_size;

  mutual_acceptance_timeout_alarm_.Reset(base::BindOnce(
      &NearbySharingServiceImpl::OnOutgoingMutualAcceptanceTimeout,
      weak_ptr_factory_.GetWeakPtr(), share_target));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
    RecordNearbyShareError(NearbyShareError::kCreatePayloadsNoAttachments);
    std::move(callback).Run(std::move(share_target), /*success=*/false);
    return;
  }

  if (!info->file_payloads().empty() || !info->text_payloads().empty()) {
    // We may have already created the payloads in the case of retry, so we can
    // skip this step.
    RecordNearbyShareError(
        NearbyShareError::kCreatePayloadsNoFileOrTextPayloads);
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
      RecordNearbyShareError(
          NearbyShareError::kCreatePayloadsFilePayloadWithoutPath);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": Got file attachment without path";
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
    RecordNearbyShareError(NearbyShareError::kOnCreatePayloadsFailed);
    CD_LOG(WARNING, Feature::NS)
        << __func__
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

  std::optional<std::vector<uint8_t>> bluetooth_mac_address =
      GetBluetoothMacAddressForShareTarget(share_target);

  // For metrics.
  all_cancelled_share_target_ids_.clear();

  // TODO(crbug.com/1111458): Add preferred transfer type.
  nearby_connections_manager_->Connect(
      std::move(endpoint_info), *info->endpoint_id(),
      std::move(bluetooth_mac_address), settings_.GetDataUsage(),
      base::BindOnce(&NearbySharingServiceImpl::OnOutgoingConnection,
                     weak_ptr_factory_.GetWeakPtr(), share_target,
                     base::TimeTicks::Now()));
}

void NearbySharingServiceImpl::OnOpenFiles(
    ShareTarget share_target,
    base::OnceCallback<void(ShareTarget, bool)> callback,
    std::vector<NearbyFileHandler::FileInfo> files) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  const bool files_open_success =
      (files.size() == share_target.file_attachments.size());
  RecordNearbySharePayloadFileOperationMetrics(
      profile_, share_target, PayloadFileOperation::kOpen, files_open_success);
  if (!info || !files_open_success) {
    RecordNearbyShareError(NearbyShareError::kOnOpenFilesFailed);
    std::move(callback).Run(std::move(share_target), /*success=*/false);
    return;
  }

  std::vector<nearby::connections::mojom::PayloadPtr> payloads;
  payloads.reserve(files.size());

  for (size_t i = 0; i < files.size(); ++i) {
    FileAttachment& attachment = share_target.file_attachments[i];
    attachment.set_size(files[i].size);
    base::File& file = files[i].file;
    int64_t payload_id = GeneratePayloadId();
    SetAttachmentPayloadId(attachment, payload_id);
    payloads.push_back(nearby::connections::mojom::Payload::New(
        payload_id,
        nearby::connections::mojom::PayloadContent::NewFile(
            nearby::connections::mojom::FilePayload::New(std::move(file)))));
  }

  info->set_file_payloads(std::move(payloads));
  std::move(callback).Run(std::move(share_target), /*success=*/true);
}

std::vector<nearby::connections::mojom::PayloadPtr>
NearbySharingServiceImpl::CreateTextPayloads(
    const std::vector<TextAttachment>& attachments) {
  std::vector<nearby::connections::mojom::PayloadPtr> payloads;
  payloads.reserve(attachments.size());
  for (const TextAttachment& attachment : attachments) {
    const std::string& body = attachment.text_body();
    std::vector<uint8_t> bytes(body.begin(), body.end());

    int64_t payload_id = GeneratePayloadId();
    SetAttachmentPayloadId(attachment, payload_id);
    payloads.push_back(nearby::connections::mojom::Payload::New(
        payload_id,
        nearby::connections::mojom::PayloadContent::NewBytes(
            nearby::connections::mojom::BytesPayload::New(std::move(bytes)))));
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

void NearbySharingServiceImpl::WriteCancel(NearbyConnection& connection) {
  CD_LOG(INFO, Feature::NS) << __func__ << ": Writing cancel frame.";

  sharing::nearby::Frame frame;
  frame.set_version(sharing::nearby::Frame::V1);
  sharing::nearby::V1Frame* v1_frame = frame.mutable_v1();
  v1_frame->set_type(sharing::nearby::V1Frame::CANCEL);

  std::vector<uint8_t> data(frame.ByteSize());
  frame.SerializeToArray(data.data(), frame.ByteSize());

  connection.Write(std::move(data));
}

void NearbySharingServiceImpl::Fail(const ShareTarget& share_target,
                                    TransferMetadata::Status status) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(NearbyShareError::kFailUnknownShareTarget);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Fail invoked for unknown share target.";
    return;
  }
  NearbyConnection* connection = info->connection();

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
    RecordNearbyShareError(
        NearbyShareError::kIncomingAdvertisementDecodedInvalidConnection);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Invalid connection for endoint id - " << endpoint_id;
    return;
  }

  if (!advertisement) {
    RecordNearbyShareError(
        NearbyShareError::kIncomingAdvertisementDecodedFailedToParse);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Failed to parse incoming connection from endpoint - "
        << endpoint_id << ", disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kDecodeAdvertisementFailed,
        placeholder_share_target);
    return;
  }

  transfer_profiler_->OnIncomingEndpointDecoded(endpoint_id,
                                                IsInHighVisibility());

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
  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (metadata.status() != TransferMetadata::Status::kInProgress) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Nearby Share service: "
        << "Incoming transfer update for share target with ID "
        << share_target.id << ": "
        << TransferMetadata::StatusToString(metadata.status());
  }
  if (metadata.status() != TransferMetadata::Status::kCancelled &&
      metadata.status() != TransferMetadata::Status::kRejected) {
    last_incoming_metadata_ =
        std::make_pair(share_target, TransferMetadataBuilder::Clone(metadata)
                                         .set_is_original(false)
                                         .build());
  } else {
    last_incoming_metadata_ = std::nullopt;
  }

  // Failed or cancelled transfers result in the progress being set to 0.
  if (!metadata.is_final_status()) {
    for (auto& observer : observers_) {
      observer.OnTransferUpdated(share_target, metadata.progress());
    }
  }

  if (metadata.is_final_status()) {
    RecordNearbyShareTransferFinalStatusMetric(
        &feature_usage_metrics_,
        /*is_incoming=*/true, share_target.type, metadata.status(),
        share_target.is_known, share_target.for_self_share, is_screen_locked_);

    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    CHECK(info->endpoint_id().has_value());
    transfer_profiler_->OnReceiveComplete(info->endpoint_id().value(),
                                          metadata.status());

    for (auto& observer : observers_) {
      observer.OnTransferCompleted(share_target, metadata.status());
    }

    OnTransferComplete();
    if (metadata.status() != TransferMetadata::Status::kComplete) {
      // For any type of failure, lets make sure any pending files get cleaned
      // up.
      RemoveIncomingPayloads(share_target);
    }
  } else if (metadata.status() ==
             TransferMetadata::Status::kAwaitingLocalConfirmation) {
    OnTransferStarted(/*is_incoming=*/true);
  }

  base::ObserverList<TransferUpdateCallback>& transfer_callbacks =
      foreground_receive_callbacks_.empty() ? background_receive_callbacks_
                                            : foreground_receive_callbacks_;

  for (TransferUpdateCallback& callback : transfer_callbacks) {
    callback.OnTransferUpdate(share_target, metadata);
  }
}

void NearbySharingServiceImpl::OnOutgoingTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& metadata) {
  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (metadata.status() != TransferMetadata::Status::kInProgress) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Nearby Share service: "
        << "Outgoing transfer update for share target with ID "
        << share_target.id << ": "
        << TransferMetadata::StatusToString(metadata.status());
  }

  // Failed or cancelled transfers result in the progress being set to 0.
  if (!metadata.is_final_status()) {
    for (auto& observer : observers_) {
      observer.OnTransferUpdated(share_target, metadata.progress());
    }
  }

  if (metadata.is_final_status()) {
    is_connecting_ = false;
    RecordNearbyShareTransferFinalStatusMetric(
        &feature_usage_metrics_,
        /*is_incoming=*/false, share_target.type, metadata.status(),
        share_target.is_known, share_target.for_self_share, is_screen_locked_);

    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    CHECK(info->endpoint_id().has_value());
    transfer_profiler_->OnSendComplete(info->endpoint_id().value(),
                                       metadata.status());

    for (auto& observer : observers_) {
      observer.OnTransferCompleted(share_target, metadata.status());
    }

    OnTransferComplete();
  } else if (metadata.status() == TransferMetadata::Status::kMediaDownloading ||
             metadata.status() ==
                 TransferMetadata::Status::kAwaitingLocalConfirmation) {
    is_connecting_ = false;
    OnTransferStarted(/*is_incoming=*/false);
  }

  bool has_foreground_send_surface =
      !foreground_send_transfer_callbacks_.empty();
  base::ObserverList<TransferUpdateCallback>& transfer_callbacks =
      has_foreground_send_surface ? foreground_send_transfer_callbacks_
                                  : background_send_transfer_callbacks_;

  for (TransferUpdateCallback& callback : transfer_callbacks) {
    callback.OnTransferUpdate(share_target, metadata);
  }

  if (has_foreground_send_surface && metadata.is_final_status()) {
    last_outgoing_metadata_ = std::nullopt;
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
    RecordNearbyShareError(NearbyShareError::kCloseConnectionInvalidConnection);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Invalid connection for target - " << share_target.id;
    return;
  }
  connection->Close();
}

void NearbySharingServiceImpl::OnIncomingDecryptedCertificate(
    const std::string& endpoint_id,
    sharing::mojom::AdvertisementPtr advertisement,
    ShareTarget placeholder_share_target,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate) {
  NearbyConnection* connection = GetConnection(placeholder_share_target);
  if (!connection) {
    RecordNearbyShareError(
        NearbyShareError::kIncomingDecryptedCertificateInvalidConnection);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Invalid connection for endpoint id - " << endpoint_id;
    return;
  }

  // Remove placeholder share target since we are creating the actual share
  // target below.
  incoming_share_target_info_map_.erase(placeholder_share_target.id);

  std::optional<ShareTarget> share_target = CreateShareTarget(
      endpoint_id, advertisement, std::move(certificate), /*is_incoming=*/true);

  if (!share_target) {
    RecordNearbyShareError(
        NearbyShareError::
            kIncomingDecryptedCertificateFailedToCreateShareTarget);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Failed to convert advertisement to share target for "
           "incoming connection, disconnecting";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingShareTarget,
        placeholder_share_target);
    return;
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Received incoming connection from " << share_target->id;

  for (auto& observer : observers_) {
    observer.OnShareTargetConnected(share_target.value());
  }

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

  std::optional<std::string> four_digit_token = ToFourDigitString(
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
  std::optional<std::vector<uint8_t>> token =
      nearby_connections_manager_->GetRawAuthenticationToken(endpoint_id);
  if (!token) {
    RecordNearbyShareError(
        NearbyShareError::
            kRunPairedKeyVerificationFailedToReadAuthenticationToken);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Failed to read authentication token from endpoint - "
        << endpoint_id;
    std::move(callback).Run(
        PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail);
    return;
  }

  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  DCHECK(share_target_info);

  share_target_info->set_frames_reader(std::make_unique<IncomingFramesReader>(
      process_manager_, share_target_info->connection()));

  bool restrict_to_contacts =
      features::IsRestrictToContactsEnabled() && share_target.is_incoming &&
      advertising_power_level_ !=
          NearbyConnectionsManager::PowerLevel::kHighPower;
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
    std::optional<std::string> four_digit_token,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection() || !info->endpoint_id()) {
    RecordNearbyShareError(
        NearbyShareError::
            kIncomingConnectionKeyVerificationInvalidConnectionOrEndpointId);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Invalid connection or endpoint id";
    return;
  }

  switch (result) {
    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail:
      RecordNearbyShareError(
          NearbyShareError::kIncomingConnectionKeyVerificationFailed);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Paired key handshake failed for target "
          << share_target.id << ". Disconnecting.";
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess:
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Paired key handshake succeeded for target - "
          << share_target.id;

      CHECK(info->endpoint_id().has_value());
      transfer_profiler_->OnPairedKeyHandshakeComplete(
          info->endpoint_id().value());

      // Upgrade bandwidth regardless of advertising visibility because the
      // sender's identity has been confirmed; the stable identifiers
      // potentially exposed by performing a bandwidth upgrade are no longer a
      // concern.
      nearby_connections_manager_->UpgradeBandwidth(*info->endpoint_id());
      ReceiveIntroduction(share_target, /*four_digit_token=*/std::nullopt);
      break;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable:
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": Unable to verify paired key encryption when "
             "receiving connection from target - "
          << share_target.id;

      CHECK(info->endpoint_id().has_value());
      transfer_profiler_->OnPairedKeyHandshakeComplete(
          info->endpoint_id().value());

      if (advertising_power_level_ ==
          NearbyConnectionsManager::PowerLevel::kHighPower) {
        // Upgrade bandwidth if advertising at high-visibility. Bandwidth
        // upgrades may expose stable identifiers, but this isn't a concern
        // here because high-visibility already leaks the device name.
        nearby_connections_manager_->UpgradeBandwidth(*info->endpoint_id());
      }

      if (four_digit_token) {
        info->set_token(*four_digit_token);
      }

      ReceiveIntroduction(share_target, std::move(four_digit_token));
      break;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown:
      RecordNearbyShareError(
          NearbyShareError::kIncomingConnectionKeyVerificationUnknownResult);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Unknown PairedKeyVerificationResult for target "
          << share_target.id << ". Disconnecting.";
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
      break;
  }
}

void NearbySharingServiceImpl::OnOutgoingConnectionKeyVerificationDone(
    const ShareTarget& share_target,
    std::optional<std::string> four_digit_token,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(
        NearbyShareError::kOutgoingConnectionKeyVerificationMissingConnection);
    return;
  }

  if (!info->transfer_update_callback()) {
    RecordNearbyShareError(
        NearbyShareError::
            kOutgoingConnectionKeyVerificationMissingTransferUpdateCallback);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": No transfer update callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return;
  }

  // TODO(crbug.com/1119279): Check if we need to set this to false for
  // Advanced Protection users.
  bool sender_skips_confirmation = true;

  switch (result) {
    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail:
      RecordNearbyShareError(
          NearbyShareError::kOutgoingConnectionKeyVerificationFailed);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Paired key handshake failed for target "
          << share_target.id << ". Disconnecting.";
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess:
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Paired key handshake succeeded for target - "
          << share_target.id;
      SendIntroduction(share_target, /*four_digit_token=*/std::nullopt);
      SendPayloads(share_target);
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable:
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": Unable to verify paired key encryption when "
             "initating connection to target - "
          << share_target.id;

      if (four_digit_token) {
        info->set_token(*four_digit_token);
      }

      if (sender_skips_confirmation) {
        CD_LOG(VERBOSE, Feature::NS)
            << __func__
            << ": Sender-side verification is disabled. Skipping "
               "token comparison with "
            << share_target.id;
        SendIntroduction(share_target, /*four_digit_token=*/std::nullopt);
        SendPayloads(share_target);
      } else {
        SendIntroduction(share_target, std::move(four_digit_token));
      }
      return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown:
      RecordNearbyShareError(
          NearbyShareError::kOutgoingConnectionKeyVerificationUnknownResult);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Unknown PairedKeyVerificationResult for target "
          << share_target.id << ". Disconnecting.";
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
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
    std::optional<std::string> four_digit_token) {
  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Receiving introduction from " << share_target.id;

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  DCHECK(info && info->connection());

  CHECK(info->endpoint_id().has_value());
  transfer_profiler_->OnIntroductionFrameReceived(info->endpoint_id().value());

  info->frames_reader()->ReadFrame(
      sharing::mojom::V1Frame::Tag::kIntroduction,
      base::BindOnce(&NearbySharingServiceImpl::OnReceivedIntroduction,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target),
                     std::move(four_digit_token)),
      kReadFramesTimeout);
}

void NearbySharingServiceImpl::OnReceivedIntroduction(
    ShareTarget share_target,
    std::optional<std::string> four_digit_token,
    std::optional<sharing::mojom::V1FramePtr> frame) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(
        NearbyShareError::kReceivedIntroductionMissingConnection);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Ignore received introduction, due to no connection established.";
    return;
  }

  DCHECK(profile_);

  if (!frame) {
    RecordNearbyShareError(NearbyShareError::kReceivedIntroductionInvalidFrame);
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kInvalidIntroductionFrame, share_target);
    CD_LOG(WARNING, Feature::NS) << __func__ << ": Invalid introduction frame";
    return;
  }

  CD_LOG(INFO, Feature::NS)
      << __func__ << ": Successfully read the introduction frame.";

  base::CheckedNumeric<int64_t> file_size_sum(0);
  int64_t transfer_size = 0;

  sharing::mojom::IntroductionFramePtr introduction_frame =
      std::move((*frame)->get_introduction());
  for (const auto& file : introduction_frame->file_metadata) {
    if (file->size <= 0) {
      RecordNearbyShareError(
          NearbyShareError::kReceivedIntroductionInvalidAttachmentSize);
      Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);
      CD_LOG(WARNING, Feature::NS)
          << __func__
          << ": Ignore introduction, due to invalid attachment size";
      return;
    }

    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Found file attachment: id=" << file->id
        << ", type= " << file->type << ", size=" << file->size
        << ", payload_id=" << file->payload_id
        << ", mime_type=" << file->mime_type;
    FileAttachment attachment(file->id, file->size, file->name, file->mime_type,
                              file->type);
    SetAttachmentPayloadId(attachment, file->payload_id);
    share_target.file_attachments.push_back(std::move(attachment));

    file_size_sum += file->size;
    transfer_size += file->size;
    if (!file_size_sum.IsValid()) {
      RecordNearbyShareError(
          NearbyShareError::kReceivedIntroductionTotalFileSizeOverflow);
      Fail(share_target, TransferMetadata::Status::kNotEnoughSpace);
      CD_LOG(WARNING, Feature::NS)
          << __func__
          << ": Ignoring introduction, total file size overflowed "
             "64 bit integer.";
      return;
    }
  }

  for (const auto& text : introduction_frame->text_metadata) {
    transfer_size += text->size;
    if (text->size <= 0) {
      RecordNearbyShareError(
          NearbyShareError::kReceivedIntroductionInvalidTextAttachmentSize);
      Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);
      CD_LOG(WARNING, Feature::NS)
          << __func__
          << ": Ignore introduction, due to invalid attachment size";
      return;
    }

    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Found text attachment: id=" << text->id
        << ", type= " << text->type << ", size=" << text->size
        << ", payload_id=" << text->payload_id;
    TextAttachment attachment(text->id, text->type, text->text_title,
                              text->size);
    SetAttachmentPayloadId(attachment, text->payload_id);
    share_target.text_attachments.push_back(std::move(attachment));
  }

  for (const auto& wifi_credentials :
       introduction_frame->wifi_credentials_metadata) {
    if (wifi_credentials->ssid.empty()) {
      RecordNearbyShareError(
          NearbyShareError::kReceivedIntroductionInvalidWifiSSID);
      Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": Ignore introduction, due to invalid Wi-Fi SSID";
      return;
    }

    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Found Wi-Fi Credentials: id=" << wifi_credentials->id
        << ", payload_id=" << wifi_credentials->payload_id
        << ", security_type=" << wifi_credentials->security_type;

    WifiCredentialsAttachment attachment(wifi_credentials->id,
                                         wifi_credentials->security_type,
                                         wifi_credentials->ssid);
    SetAttachmentPayloadId(attachment, wifi_credentials->payload_id);
    share_target.wifi_credentials_attachments.push_back(std::move(attachment));
  }

  if (!share_target.has_attachments()) {
    RecordNearbyShareError(
        NearbyShareError::kReceivedIntroductionShareTargetNoAttachment);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": No attachment is found for this share target. It can "
           "be result of unrecognizable attachment type";
    Fail(share_target, TransferMetadata::Status::kUnsupportedAttachmentType);

    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": We don't support the attachments sent by the sender. "
           "We have informed "
        << share_target.id;
    return;
  }

  if (file_size_sum.ValueOrDie() == 0) {
    OnStorageCheckCompleted(std::move(share_target),
                            std::move(four_digit_token),
                            /*is_out_of_storage=*/false);
    return;
  }

  CHECK(info->endpoint_id().has_value());
  transfer_size_map_[info->endpoint_id().value()] = transfer_size;

  base::FilePath download_path =
      DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
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
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Receiving response frame from " << share_target.id;
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  DCHECK(info && info->connection());

  info->frames_reader()->ReadFrame(
      sharing::mojom::V1Frame::Tag::kConnectionResponse,
      base::BindOnce(&NearbySharingServiceImpl::OnReceiveConnectionResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target)),
      kReadResponseFrameTimeout);
}

void NearbySharingServiceImpl::OnReceiveConnectionResponse(
    ShareTarget share_target,
    std::optional<sharing::mojom::V1FramePtr> frame) {
  OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(
        NearbyShareError::kReceiveConnectionResponseMissingConnection);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Ignore received connection response, due to no "
           "connection established.";
    return;
  }

  if (!info->transfer_update_callback()) {
    RecordNearbyShareError(
        NearbyShareError::
            kReceiveConnectionResponseMissingTransferUpdateCallback);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": No transfer update callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return;
  }

  if (!frame) {
    RecordNearbyShareError(
        NearbyShareError::kReceiveConnectionResponseInvalidFrame);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Failed to read a response from the remote device. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kFailedToReadOutgoingConnectionResponse,
        share_target);
    return;
  }

  mutual_acceptance_timeout_alarm_.Cancel();

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Successfully read the connection response frame.";

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
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": The connection was accepted. Payloads are now being sent.";

      CHECK(info->endpoint_id().has_value());
      transfer_profiler_->OnSendStart(info->endpoint_id().value());

      // Sender events
      for (auto& observer : observers_) {
        observer.OnTransferAccepted(share_target);
        observer.OnTransferStarted(
            share_target, transfer_size_map_[info->endpoint_id().value()]);
      }
      break;
    }
    case sharing::mojom::ConnectionResponseFrame::Status::kReject:
      AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kRejected,
                                         share_target);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": The connection was rejected. The connection has been closed.";
      break;
    case sharing::mojom::ConnectionResponseFrame::Status::kNotEnoughSpace:
      RecordNearbyShareError(
          NearbyShareError::kReceiveConnectionResponseNotEnoughSpace);
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kNotEnoughSpace, share_target);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": The connection was rejected because the remote device "
             "does not have enough space for our attachments. The "
             "connection has been closed.";
      break;
    case sharing::mojom::ConnectionResponseFrame::Status::
        kUnsupportedAttachmentType:
      RecordNearbyShareError(
          NearbyShareError::
              kReceiveConnectionResponseUnsupportedAttachmentType);
      AbortAndCloseConnectionIfNecessary(
          TransferMetadata::Status::kUnsupportedAttachmentType, share_target);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": The connection was rejected because the remote device "
             "does not support the attachments we were sending. The "
             "connection has been closed.";
      break;
    case sharing::mojom::ConnectionResponseFrame::Status::kTimedOut:
      RecordNearbyShareError(
          NearbyShareError::kReceiveConnectionResponseTimedOut);
      AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kTimedOut,
                                         share_target);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": The connection was rejected because the remote device "
             "timed out. The connection has been closed.";
      break;
    default:
      RecordNearbyShareError(
          NearbyShareError::kReceiveConnectionResponseConnectionFailed);
      AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kFailed,
                                         share_target);
      CD_LOG(VERBOSE, Feature::NS)
          << __func__
          << ": The connection failed. The connection has been closed.";
      break;
  }
}

void NearbySharingServiceImpl::OnStorageCheckCompleted(
    ShareTarget share_target,
    std::optional<std::string> four_digit_token,
    bool is_out_of_storage) {
  if (is_out_of_storage) {
    RecordNearbyShareError(
        NearbyShareError::kStorageCheckCompletedNotEnoughSpace);
    Fail(share_target, TransferMetadata::Status::kNotEnoughSpace);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Not enough space on the receiver. We have informed "
        << share_target.id;
    return;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(
        NearbyShareError::kStorageCheckCompletedMissingConnection);
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": Invalid connection for share target - "
        << share_target.id;
    return;
  }
  NearbyConnection* connection = info->connection();

  if (!info->transfer_update_callback()) {
    RecordNearbyShareError(
        NearbyShareError::kStorageCheckCompletedMissingTransferUpdateCallback);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": No transfer update callback. Disconnecting.";
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
    return;
  }

  mutual_acceptance_timeout_alarm_.Reset(base::BindOnce(
      &NearbySharingServiceImpl::OnIncomingMutualAcceptanceTimeout,
      weak_ptr_factory_.GetWeakPtr(), share_target));

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(mutual_acceptance_timeout_alarm_.callback()),
      kReadResponseFrameTimeout);

  info->transfer_update_callback()->OnTransferUpdate(
      share_target,
      TransferMetadataBuilder()
          .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
          .set_token(std::move(four_digit_token))
          .build());

  if (!incoming_share_target_info_map_.count(share_target.id)) {
    RecordNearbyShareError(
        NearbyShareError::kStorageCheckCompletedNoIncomingShareTarget);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": IncomingShareTarget not found, disconnecting "
        << share_target.id;
    AbortAndCloseConnectionIfNecessary(
        TransferMetadata::Status::kMissingShareTarget, share_target);
    return;
  }

  connection->SetDisconnectionListener(base::BindOnce(
      &NearbySharingServiceImpl::OnIncomingConnectionDisconnected,
      weak_ptr_factory_.GetWeakPtr(), share_target));

  auto* frames_reader = info->frames_reader();
  if (!frames_reader) {
    RecordNearbyShareError(
        NearbyShareError::kStorageCheckCompletedNoFramesReader);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Stopped reading further frames, due to no connection "
           "established.";
    return;
  }

  // Auto-accept self shares when not in high-visibility mode, unless the
  // filetype includes WiFi credentials.
  if (share_target.CanAutoAccept() && !IsInHighVisibility()) {
    CD_LOG(INFO, Feature::NS) << __func__ << ": Auto-accepting self share.";
    Accept(share_target, base::DoNothing());
  } else {
    CD_LOG(INFO, Feature::NS) << __func__ << ": Can't auto-accept transfer.";
  }

  frames_reader->ReadFrame(
      base::BindOnce(&NearbySharingServiceImpl::OnFrameRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(share_target)));
}

void NearbySharingServiceImpl::OnFrameRead(
    ShareTarget share_target,
    std::optional<sharing::mojom::V1FramePtr> frame) {
  if (!frame) {
    // This is the case when the connection has been closed since we wait
    // indefinitely for incoming frames.
    return;
  }

  sharing::mojom::V1FramePtr v1_frame = std::move(*frame);
  switch (v1_frame->which()) {
    case sharing::mojom::V1Frame::Tag::kCancelFrame:
      CD_LOG(INFO, Feature::NS)
          << __func__ << ": Read the cancel frame, closing connection";
      DoCancel(share_target, base::DoNothing(),
               /*is_initiator_of_cancellation=*/false);
      break;

    case sharing::mojom::V1Frame::Tag::kCertificateInfo:
      HandleCertificateInfoFrame(v1_frame->get_certificate_info());
      break;

    default:
      CD_LOG(VERBOSE, Feature::NS)
          << __func__ << ": Discarding unknown frame of type";
      break;
  }

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->frames_reader()) {
    RecordNearbyShareError(NearbyShareError::kFrameReadNoFrameReader);
    CD_LOG(WARNING, Feature::NS)
        << __func__
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
        share_target,
        TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kUnexpectedDisconnection)
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
            .set_status(TransferMetadata::Status::kUnexpectedDisconnection)
            .build());
  }
  UnregisterShareTarget(share_target);
}

void NearbySharingServiceImpl::OnIncomingMutualAcceptanceTimeout(
    const ShareTarget& share_target) {
  DCHECK(share_target.is_incoming);

  RecordNearbyShareError(NearbyShareError::kIncomingMutualAcceptanceTimeout);
  CD_LOG(VERBOSE, Feature::NS)
      << __func__
      << ": Incoming mutual acceptance timed out, closing connection for "
      << share_target.id;

  Fail(share_target, TransferMetadata::Status::kTimedOut);
}

void NearbySharingServiceImpl::OnOutgoingMutualAcceptanceTimeout(
    const ShareTarget& share_target) {
  DCHECK(!share_target.is_incoming);

  RecordNearbyShareError(NearbyShareError::kOutgoingMutualAcceptanceTimeout);
  CD_LOG(VERBOSE, Feature::NS)
      << __func__
      << ": Outgoing mutual acceptance timed out, closing connection for "
      << share_target.id;

  AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kTimedOut,
                                     share_target);
}

std::optional<ShareTarget> NearbySharingServiceImpl::CreateShareTarget(
    const std::string& endpoint_id,
    const sharing::mojom::AdvertisementPtr& advertisement,
    std::optional<NearbyShareDecryptedPublicCertificate> certificate,
    bool is_incoming) {
  DCHECK(advertisement);

  if (!advertisement->device_name && !certificate) {
    RecordNearbyShareError(
        NearbyShareError::kCreateShareTargetFailedToRetreivePublicCertificate);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": Failed to retrieve public certificate for contact "
           "only advertisement.";
    return std::nullopt;
  }

  std::optional<std::string> device_name =
      GetDeviceName(advertisement, certificate);
  if (!device_name) {
    RecordNearbyShareError(
        NearbyShareError::kCreateShareTargetFailedToRetreiveDeviceName);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Failed to retrieve device name for advertisement.";
    return std::nullopt;
  }

  ShareTarget target;
  target.type = advertisement->device_type;
  target.device_name = std::move(*device_name);
  target.is_incoming = is_incoming;
  target.device_id = GetDeviceId(endpoint_id, certificate);
  target.for_self_share = certificate && certificate->for_self_share();

  ShareTargetInfo& info = GetOrCreateShareTargetInfo(target, endpoint_id);

  if (certificate) {
    if (certificate->unencrypted_metadata().has_full_name()) {
      target.full_name = certificate->unencrypted_metadata().full_name();
    }

    if (certificate->unencrypted_metadata().has_icon_url()) {
      target.image_url = GURL(certificate->unencrypted_metadata().icon_url());
    }

    target.is_known = true;
    info.set_certificate(std::move(*certificate));
  }

  share_target_map_[endpoint_id] = target;
  for (auto& observer : observers_) {
    observer.OnShareTargetAdded(target);
  }

  return target;
}

void NearbySharingServiceImpl::OnPayloadTransferUpdate(
    ShareTarget share_target,
    TransferMetadata metadata) {
  bool is_in_progress =
      metadata.status() == TransferMetadata::Status::kInProgress;

  if (is_in_progress && share_target.is_incoming &&
      is_waiting_to_record_accept_to_transfer_start_metric_) {
    RecordNearbyShareTimeFromLocalAcceptToTransferStartMetric(
        base::TimeTicks::Now() - incoming_share_accepted_timestamp_);
    is_waiting_to_record_accept_to_transfer_start_metric_ = false;
  }

  // kInProgress status is logged extensively elsewhere so avoid the spam.
  if (!is_in_progress) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Nearby Share service: "
        << "Payload transfer update for share target with ID "
        << share_target.id << ": "
        << TransferMetadata::StatusToString(metadata.status());
  }

  if (metadata.status() == TransferMetadata::Status::kComplete &&
      share_target.is_incoming) {
    if (!OnIncomingPayloadsComplete(share_target)) {
      metadata = TransferMetadataBuilder()
                     .set_status(TransferMetadata::Status::kIncompletePayloads)
                     .build();

      // Reset file paths for file attachments.
      for (auto& file : share_target.file_attachments) {
        file.set_file_path(std::nullopt);
      }

      // Reset body of text attachments.
      for (auto& text : share_target.text_attachments) {
        text.set_text_body(std::string());
      }
    }

    fast_initiation_scanner_cooldown_timer_.Start(
        FROM_HERE, kFastInitiationScannerCooldown,
        base::BindRepeating(
            &NearbySharingServiceImpl::InvalidateFastInitiationScanning,
            base::Unretained(this)));
  }

  // Make sure to call this before calling Disconnect or we risk losing some
  // transfer updates in the receive case due to the Disconnect call cleaning up
  // share targets.
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (info && info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(share_target, metadata);
  }

  // Cancellation has its own disconnection strategy, possibly adding a delay
  // before disconnection to provide the other party time to process the
  // cancellation.
  if (TransferMetadata::IsFinalStatus(metadata.status()) &&
      metadata.status() != TransferMetadata::Status::kCancelled) {
    if (share_target.has_attachments() &&
        share_target.file_attachments.size()) {
      // For file payloads, the |PayloadTracker| callback for updates is
      // |OnTransferUpdate| which will set status |kComplete| if payload reading
      // is successful.
      const bool files_read_success =
          (metadata.status() == TransferMetadata::Status::kComplete);
      RecordNearbySharePayloadFileOperationMetrics(profile_, share_target,
                                                   PayloadFileOperation::kRead,
                                                   files_read_success);
    }
    Disconnect(share_target, metadata);
  }
}

bool NearbySharingServiceImpl::OnIncomingPayloadsComplete(
    ShareTarget& share_target) {
  DCHECK(share_target.is_incoming);

  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info || !info->connection()) {
    RecordNearbyShareError(
        NearbyShareError::kIncomingPayloadsCompleteMissingConnection);
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Connection not found for target - "
        << share_target.id;

    return false;
  }
  NearbyConnection* connection = info->connection();

  connection->SetDisconnectionListener(
      base::BindOnce(&NearbySharingServiceImpl::UnregisterShareTarget,
                     weak_ptr_factory_.GetWeakPtr(), share_target));

  for (auto& file : share_target.file_attachments) {
    AttachmentInfo& attachment_info = attachment_info_map_[file.id()];
    std::optional<int64_t> payload_id = attachment_info.payload_id;
    if (!payload_id) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteMissingPayloadId);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": No payload id found for file - " << file.id();
      return false;
    }

    nearby::connections::mojom::Payload* incoming_payload =
        nearby_connections_manager_->GetIncomingPayload(*payload_id);
    if (!incoming_payload || !incoming_payload->content ||
        !incoming_payload->content->is_file()) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteMissingPayload);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": No payload found for file - " << file.id();
      return false;
    }

    file.set_file_path(attachment_info.file_path);
  }

  for (auto& text : share_target.text_attachments) {
    AttachmentInfo& attachment_info = attachment_info_map_[text.id()];
    std::optional<int64_t> payload_id = attachment_info.payload_id;
    if (!payload_id) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteMissingTextPayloadId);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": No payload id found for text - " << text.id();
      return false;
    }

    nearby::connections::mojom::Payload* incoming_payload =
        nearby_connections_manager_->GetIncomingPayload(*payload_id);
    if (!incoming_payload || !incoming_payload->content ||
        !incoming_payload->content->is_bytes()) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteMissingTextPayload);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": No payload found for text - " << text.id();
      return false;
    }

    std::vector<uint8_t>& bytes = incoming_payload->content->get_bytes()->bytes;
    if (bytes.empty()) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteTextPayloadEmptyBytes);
      CD_LOG(WARNING, Feature::NS)
          << __func__
          << ": Incoming bytes is empty for text payload with payload_id - "
          << *payload_id;
      return false;
    }

    std::string text_body(bytes.begin(), bytes.end());
    text.set_text_body(text_body);

    attachment_info.text_body = std::move(text_body);
  }

  for (auto& wifi_credentials : share_target.wifi_credentials_attachments) {
    AttachmentInfo& attachment_info =
        attachment_info_map_[wifi_credentials.id()];
    std::optional<int64_t> payload_id = attachment_info.payload_id;
    if (!payload_id) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteMissingWifiPayloadId);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": No payload id found for wifi credentials - "
          << wifi_credentials.id();
      return false;
    }

    nearby::connections::mojom::Payload* incoming_payload =
        nearby_connections_manager_->GetIncomingPayload(*payload_id);
    if (!incoming_payload || !incoming_payload->content ||
        !incoming_payload->content->is_bytes()) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteMissingWifiPayload);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": No payload found for Wi-Fi credentials - "
          << wifi_credentials.id();
      return false;
    }

    const std::vector<uint8_t>& bytes =
        incoming_payload->content->get_bytes()->bytes;
    if (bytes.empty()) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteWifiPayloadEmptyBytes);
      CD_LOG(WARNING, Feature::NS)
          << __func__
          << ": Incoming bytes is empty for Wi-Fi password with payload_id - "
          << *payload_id;
      return false;
    }

    sharing::nearby::WifiCredentials credentials_proto;
    if (!credentials_proto.ParseFromArray(bytes.data(), bytes.size())) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteWifiFailedToParse);
      CD_LOG(WARNING, Feature::NS)
          << __func__ << ": Failed to parse Wi-Fi credentials proto.";
      return false;
    }

    if (credentials_proto.password().empty()) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteWifiNoPassword);
      CD_LOG(WARNING, Feature::NS) << __func__ << ": No Wi-Fi password found.";
      return false;
    }

    if (credentials_proto.has_hidden_ssid() &&
        credentials_proto.hidden_ssid()) {
      RecordNearbyShareError(
          NearbyShareError::kIncomingPayloadsCompleteWifiHiddenNetwork);
      CD_LOG(WARNING, Feature::NS) << __func__ << ": Network is hidden.";
      return false;
    }

    std::string wifi_password(credentials_proto.password());
    wifi_credentials.set_wifi_password(wifi_password);

    // Automatically set up the Wi-Fi network for the user.
    wifi_network_handler_->ConfigureWifiNetwork(wifi_credentials,
                                                base::DoNothing());
  }
  return true;
}

void NearbySharingServiceImpl::RemoveIncomingPayloads(
    ShareTarget share_target) {
  if (!share_target.is_incoming) {
    return;
  }

  CD_LOG(INFO, Feature::NS)
      << __func__
      << ": Cleaning up payloads due to transfer cancelled or failure.";

  nearby_connections_manager_->ClearIncomingPayloads();
  std::vector<base::FilePath> files_for_deletion;
  for (const auto& file : share_target.file_attachments) {
    auto it = attachment_info_map_.find(file.id());
    if (it == attachment_info_map_.end()) {
      continue;
    }

    files_for_deletion.push_back(it->second.file_path);
  }

  file_handler_.DeleteFilesFromDisk(std::move(files_for_deletion));
}

void NearbySharingServiceImpl::Disconnect(const ShareTarget& share_target,
                                          TransferMetadata metadata) {
  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  if (!share_target_info) {
    RecordNearbyShareError(
        NearbyShareError::kDisconnectFailedToGetShareTargetInfo);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Failed to disconnect. No share target info found for target - "
        << share_target.id;
    return;
  }

  std::optional<std::string> endpoint_id = share_target_info->endpoint_id();
  if (!endpoint_id) {
    RecordNearbyShareError(NearbyShareError::kDisconnectMissingEndpointId);
    CD_LOG(WARNING, Feature::NS)
        << __func__
        << ": Failed to disconnect. No endpoint id found for share target - "
        << share_target.id;
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
    // We need to explicitly remove any previous share target for
    // |endpoint_id| if one exists, notifying observers that a share target is
    // lost.
    const auto it = outgoing_share_target_map_.find(endpoint_id);
    if (it != outgoing_share_target_map_.end() &&
        it->second.id != share_target.id) {
      RemoveOutgoingShareTargetWithEndpointId(endpoint_id);
    }

    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Adding (endpoint_id=" << endpoint_id
        << ", share_target_id=" << share_target.id
        << ") to outgoing share target map";
    outgoing_share_target_map_.insert_or_assign(endpoint_id, share_target);
    auto& info = outgoing_share_target_info_map_[share_target.id];
    info.set_endpoint_id(endpoint_id);
    return info;
  }
}

ShareTargetInfo* NearbySharingServiceImpl::GetShareTargetInfo(
    const ShareTarget& share_target) {
  if (share_target.is_incoming) {
    return GetIncomingShareTargetInfo(share_target);
  } else {
    return GetOutgoingShareTargetInfo(share_target);
  }
}

IncomingShareTargetInfo* NearbySharingServiceImpl::GetIncomingShareTargetInfo(
    const ShareTarget& share_target) {
  auto it = incoming_share_target_info_map_.find(share_target.id);
  if (it == incoming_share_target_info_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

OutgoingShareTargetInfo* NearbySharingServiceImpl::GetOutgoingShareTargetInfo(
    const ShareTarget& share_target) {
  auto it = outgoing_share_target_info_map_.find(share_target.id);
  if (it == outgoing_share_target_info_map_.end()) {
    return nullptr;
  }

  return &it->second;
}

NearbyConnection* NearbySharingServiceImpl::GetConnection(
    const ShareTarget& share_target) {
  ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
  return share_target_info ? share_target_info->connection() : nullptr;
}

std::optional<std::vector<uint8_t>>
NearbySharingServiceImpl::GetBluetoothMacAddressForShareTarget(
    const ShareTarget& share_target) {
  ShareTargetInfo* info = GetShareTargetInfo(share_target);
  if (!info) {
    RecordNearbyShareError(
        NearbyShareError::
            kGetBluetoothMacAddressForShareTargetNoShareTargetInfo);
    CD_LOG(ERROR, Feature::NS) << __func__ << ": No ShareTargetInfo found for "
                               << "share target id: " << share_target.id;
    return std::nullopt;
  }

  const std::optional<NearbyShareDecryptedPublicCertificate>& certificate =
      info->certificate();
  if (!certificate) {
    RecordNearbyShareError(
        NearbyShareError::
            kGetBluetoothMacAddressForShareTargetNoDecryptedPublicCertificate);
    CD_LOG(ERROR, Feature::NS)
        << __func__ << ": No decrypted public certificate found for "
        << "share target id: " << share_target.id;
    return std::nullopt;
  }

  return GetBluetoothMacAddressFromCertificate(*certificate);
}

void NearbySharingServiceImpl::ClearOutgoingShareTargetInfoMap() {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Clearing outgoing share target map.";
  while (!outgoing_share_target_map_.empty()) {
    RemoveOutgoingShareTargetWithEndpointId(
        /*endpoint_id=*/outgoing_share_target_map_.begin()->first);
  }
  DCHECK(outgoing_share_target_map_.empty());
  DCHECK(outgoing_share_target_info_map_.empty());
}

void NearbySharingServiceImpl::SetAttachmentPayloadId(
    const Attachment& attachment,
    int64_t payload_id) {
  attachment_info_map_[attachment.id()].payload_id = payload_id;
}

std::optional<int64_t> NearbySharingServiceImpl::GetAttachmentPayloadId(
    int64_t attachment_id) {
  auto it = attachment_info_map_.find(attachment_id);
  if (it == attachment_info_map_.end()) {
    return std::nullopt;
  }

  return it->second.payload_id;
}

void NearbySharingServiceImpl::UnregisterShareTarget(
    const ShareTarget& share_target) {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Unregistering share target - " << share_target.id;

  // For metrics.
  all_cancelled_share_target_ids_.erase(share_target.id);

  if (share_target.is_incoming) {
    if (last_incoming_metadata_ &&
        last_incoming_metadata_->first.id == share_target.id) {
      last_incoming_metadata_.reset();
    }

    // Clear legacy incoming payloads to release resource.
    nearby_connections_manager_->ClearIncomingPayloads();
    incoming_share_target_info_map_.erase(share_target.id);
  } else {
    if (last_outgoing_metadata_ &&
        last_outgoing_metadata_->first.id == share_target.id) {
      last_outgoing_metadata_.reset();
    }
    // Find the endpoint id that matches the given share target.
    std::optional<std::string> endpoint_id;
    auto it = outgoing_share_target_info_map_.find(share_target.id);
    if (it != outgoing_share_target_info_map_.end()) {
      endpoint_id = it->second.endpoint_id();
    }

    // Be careful not to clear out the share target info map if a new session
    // was started during the cancelation delay.
    if (!is_scanning_ && !is_transferring_) {
      // TODO(crbug/1108348): Support caching manager by keeping track of the
      // share_target/endpoint_id for next time.
      ClearOutgoingShareTargetInfoMap();
    }

    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Unregister share target: " << share_target.id;
  }
  mutual_acceptance_timeout_alarm_.Cancel();
}

void NearbySharingServiceImpl::OnStartAdvertisingResult(
    bool used_device_name,
    NearbyConnectionsManager::ConnectionsStatus status) {
  RecordNearbyShareStartAdvertisingResultMetric(
      /*is_high_visibility=*/used_device_name, status);

  if (status == NearbyConnectionsManager::ConnectionsStatus::kSuccess) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": StartAdvertising over Nearby Connections was successful.";
    SetInHighVisibility(used_device_name);
  } else {
    RecordNearbyShareError(NearbyShareError::kStartAdvertisingFailed);
    CD_LOG(ERROR, Feature::NS)
        << __func__ << ": StartAdvertising over Nearby Connections failed: "
        << NearbyConnectionsManager::ConnectionsStatusToString(status);
    SetInHighVisibility(false);
    for (auto& observer : observers_) {
      observer.OnStartAdvertisingFailure();
    }
  }
}

void NearbySharingServiceImpl::OnStopAdvertisingResult(
    NearbyConnectionsManager::ConnectionsStatus status) {
  if (status == NearbyConnectionsManager::ConnectionsStatus::kSuccess) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": StopAdvertising over Nearby Connections was successful.";
  } else {
    RecordNearbyShareError(NearbyShareError::kStopAdvertisingFailed);
    CD_LOG(ERROR, Feature::NS)
        << __func__ << ": StopAdvertising over Nearby Connections failed: "
        << NearbyConnectionsManager::ConnectionsStatusToString(status);
  }

  // The |advertising_power_level_| is set in |StopAdvertising| instead of here
  // at the callback because when restarting advertising, |StartAdvertising| is
  // called immediately after |StopAdvertising| without waiting for the
  // callback. Nearby Connections queues the requests and completes them in
  // order, so waiting for Stop to complete is unnecessary, but Start will fail
  // if the |advertising_power_level_| indicates we are already advertising.
  SetInHighVisibility(false);
}

void NearbySharingServiceImpl::OnStartDiscoveryResult(
    NearbyConnectionsManager::ConnectionsStatus status) {
  bool success =
      status == NearbyConnectionsManager::ConnectionsStatus::kSuccess;
  if (success) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__
        << ": StartDiscovery over Nearby Connections was successful.";

    // Periodically download certificates if there are discovered, contact-based
    // advertisements that cannot decrypt any currently stored certificates.
    ScheduleCertificateDownloadDuringDiscovery(/*attempt_count=*/0);
  } else {
    RecordNearbyShareError(NearbyShareError::kStartDiscoveryFailed);
    CD_LOG(ERROR, Feature::NS)
        << __func__ << ": StartDiscovery over Nearby Connections failed: "
        << NearbyConnectionsManager::ConnectionsStatusToString(status);
  }
  for (auto& observer : observers_) {
    observer.OnStartDiscoveryResult(success);
    if (success) {
      observer.OnShareTargetDiscoveryStarted();
    }
  }
}

void NearbySharingServiceImpl::SetInHighVisibility(
    bool new_in_high_visibility) {
  if (IsInHighVisibility() == new_in_high_visibility) {
    return;
  }

  if (chromeos::features::IsQuickShareV2Enabled()) {
    prefs_->SetBoolean(prefs::kNearbySharingInHighVisibilityPrefName,
                       /*value=*/new_in_high_visibility);
  } else {
    in_high_visibility_ = new_in_high_visibility;
  }

  for (auto& observer : observers_) {
    observer.OnHighVisibilityChanged(new_in_high_visibility);
  }
}

void NearbySharingServiceImpl::AbortAndCloseConnectionIfNecessary(
    const TransferMetadata::Status status,
    const ShareTarget& share_target) {
  TransferMetadata metadata =
      TransferMetadataBuilder().set_status(status).build();
  ShareTargetInfo* info = GetShareTargetInfo(share_target);

  // First invoke the appropriate transfer callback with the final |status|.
  if (info && info->transfer_update_callback()) {
    info->transfer_update_callback()->OnTransferUpdate(share_target, metadata);
  } else if (share_target.is_incoming) {
    OnIncomingTransferUpdate(share_target, metadata);
  } else {
    OnOutgoingTransferUpdate(share_target, metadata);
  }

  // Close connection if necessary.
  if (info && info->connection()) {
    // Ensure that the disconnect listener is set to UnregisterShareTarget
    // because the other listenrs also try to record a final status metric.
    info->connection()->SetDisconnectionListener(
        base::BindOnce(&NearbySharingServiceImpl::UnregisterShareTarget,
                       weak_ptr_factory_.GetWeakPtr(), share_target));

    info->connection()->Close();
  }
}

void NearbySharingServiceImpl::UpdateVisibilityReminderTimer(
    bool reset_timestamp) {
  if (!settings_.GetEnabled() ||
      !IsVisibleInBackground(settings_.GetVisibility())) {
    visibility_reminder_timer_.Stop();
    return;
  }

  if (reset_timestamp ||
      prefs_->GetTime(prefs::kNearbySharingNextVisibilityReminderTimePrefName)
          .is_null()) {
    prefs_->SetTime(prefs::kNearbySharingNextVisibilityReminderTimePrefName,
                    base::Time::Now() + visibility_reminder_timer_delay_);
  }

  visibility_reminder_timer_.Start(
      FROM_HERE, GetTimeUntilNextVisibilityReminder(),
      base::BindOnce(&NearbySharingServiceImpl::OnVisibilityReminderTimerFired,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbySharingServiceImpl::OnVisibilityReminderTimerFired() {
  nearby_notification_manager_->ShowVisibilityReminder();
  UpdateVisibilityReminderTimer(/*reset_timestamp=*/true);
}

// Calculate the actual time when next visibility reminder will be shown.
base::TimeDelta NearbySharingServiceImpl::GetTimeUntilNextVisibilityReminder() {
  base::Time next_visibility_reminder_time =
      prefs_->GetTime(prefs::kNearbySharingNextVisibilityReminderTimePrefName);
  base::TimeDelta time_until_next_reminder =
      next_visibility_reminder_time - base::Time::Now();

  // Immediately show visibility reminder if it's already passed 180 days since
  // last time user saw the reminder.
  return std::max(base::Seconds(0), time_until_next_reminder);
}
