// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/tether/tether_service.h"

#include "ash/constants/ash_features.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/tether/tether_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/network/tether_notification_presenter.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/tether/gms_core_notifications_state_tracker_impl.h"
#include "chromeos/ash/components/tether/tether_component.h"
#include "chromeos/ash/components/tether/tether_component_impl.h"
#include "chromeos/ash/components/tether/tether_host_fetcher_impl.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"

namespace ash {
namespace tether {

namespace {

constexpr int64_t kMetricFalsePositiveSeconds = 2;

}  // namespace

// static
TetherService* TetherService::Get(Profile* profile) {
  // Tether networks are only available for the primary user; thus, no
  // TetherService object should be created for secondary users. If multiple
  // instances were created for each user, inconsistencies could lead to browser
  // crashes. See https://crbug.com/809357.
  if (!ProfileHelper::Get()->IsPrimaryProfile(profile)) {
    return nullptr;
  }

  return TetherServiceFactory::GetForBrowserContext(profile);
}

// static
void TetherService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  TetherComponentImpl::RegisterProfilePrefs(registry);
  TetherNotificationPresenter::RegisterProfilePrefs(registry);
}

// static.
std::string TetherService::TetherFeatureStateToString(
    const TetherFeatureState& state) {
  switch (state) {
    case (TetherFeatureState::SHUT_DOWN):
      return "[TetherService shut down]";
    case (TetherFeatureState::NO_AVAILABLE_HOSTS):
      return "[no potential Tether hosts]";
    case (TetherFeatureState::CELLULAR_DISABLED):
      return "[Cellular setting disabled]";
    case (TetherFeatureState::PROHIBITED):
      return "[prohibited by device policy]";
    case (TetherFeatureState::BLUETOOTH_DISABLED):
      return "[Bluetooth is disabled]";
    case (TetherFeatureState::USER_PREFERENCE_DISABLED):
      return "[Instant Tethering preference is disabled]";
    case (TetherFeatureState::ENABLED):
      return "[Enabled]";
    case (TetherFeatureState::BLE_NOT_PRESENT):
      return "[BLE is not present on the device]";
    case (TetherFeatureState::WIFI_NOT_PRESENT):
      return "[Wi-Fi is not present on the device]";
    case (TetherFeatureState::SUSPENDED):
      return "[Suspended]";
    case (TetherFeatureState::BETTER_TOGETHER_SUITE_DISABLED):
      return "[Better Together suite is disabled]";
    case (TetherFeatureState::TETHER_FEATURE_STATE_MAX):
      // |previous_feature_state_| is initialized to TETHER_FEATURE_STATE_MAX,
      // and this value is never actually used in practice.
      return "[TetherService initializing]";
    default:
      NOTREACHED_IN_MIGRATION();
      return "[Invalid state]";
  }
}

TetherService::TetherService(
    Profile* profile,
    chromeos::PowerManagerClient* power_manager_client,
    device_sync::DeviceSyncClient* device_sync_client,
    secure_channel::SecureChannelClient* secure_channel_client,
    multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
    session_manager::SessionManager* session_manager)
    : profile_(profile),
      power_manager_client_(power_manager_client),
      device_sync_client_(device_sync_client),
      secure_channel_client_(secure_channel_client),
      multidevice_setup_client_(multidevice_setup_client),
      network_state_handler_(NetworkHandler::Get()->network_state_handler()),
      session_manager_(session_manager),
      notification_presenter_(
          std::make_unique<TetherNotificationPresenter>(profile_,
                                                        NetworkConnect::Get())),
      gms_core_notifications_state_tracker_(
          std::make_unique<GmsCoreNotificationsStateTrackerImpl>()),
      tether_host_fetcher_(
          TetherHostFetcherImpl::Factory::Create(device_sync_client_,
                                                 multidevice_setup_client_)),
      timer_(std::make_unique<base::OneShotTimer>()) {
  tether_host_fetcher_->AddObserver(this);
  power_manager_client_->AddObserver(this);
  network_state_handler_observer_.Observe(network_state_handler_.get());
  device_sync_client_->AddObserver(this);
  multidevice_setup_client_->AddObserver(this);

  UMA_HISTOGRAM_BOOLEAN("InstantTethering.UserPreference.OnStartup",
                        IsEnabledByPreference());
  PA_LOG(VERBOSE)
      << "TetherService has started. Initial user preference value: "
      << IsEnabledByPreference();

  if (device_sync_client_->is_ready()) {
    OnReady();
  }

  // Wait for OnReady() to be called. OnReady() will indirectly
  // call OnHostStatusChanged(), which will call GetAdapter().
}

TetherService::~TetherService() {
  if (tether_component_) {
    tether_component_->RemoveObserver(this);
  }
}

void TetherService::StartTetherIfPossible() {
  if (GetTetherTechnologyState() !=
      NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    return;
  }

  // Do not initialize the TetherComponent if it already exists.
  if (tether_component_) {
    return;
  }

  PA_LOG(VERBOSE) << "Starting up TetherComponent.";
  tether_component_ = TetherComponentImpl::Factory::Create(
      device_sync_client_, secure_channel_client_, tether_host_fetcher_.get(),
      notification_presenter_.get(),
      gms_core_notifications_state_tracker_.get(), profile_->GetPrefs(),
      NetworkHandler::Get(), NetworkConnect::Get(), adapter_, session_manager_);
}

GmsCoreNotificationsStateTracker*
TetherService::GetGmsCoreNotificationsStateTracker() {
  return gms_core_notifications_state_tracker_.get();
}

void TetherService::StopTetherIfNecessary() {
  if (!tether_component_ ||
      tether_component_->status() != TetherComponent::Status::ACTIVE) {
    return;
  }

  PA_LOG(VERBOSE) << "Shutting down TetherComponent.";

  TetherComponent::ShutdownReason shutdown_reason;
  switch (GetTetherFeatureState()) {
    case SHUT_DOWN:
      shutdown_reason = TetherComponent::ShutdownReason::USER_LOGGED_OUT;
      break;
    case SUSPENDED:
      shutdown_reason = TetherComponent::ShutdownReason::USER_CLOSED_LID;
      break;
    case CELLULAR_DISABLED:
      shutdown_reason = TetherComponent::ShutdownReason::CELLULAR_DISABLED;
      break;
    case BLUETOOTH_DISABLED:
      shutdown_reason = TetherComponent::ShutdownReason::BLUETOOTH_DISABLED;
      break;
    case USER_PREFERENCE_DISABLED:
      shutdown_reason = TetherComponent::ShutdownReason::PREF_DISABLED;
      break;
    case BLE_NOT_PRESENT:
      shutdown_reason =
          TetherComponent::ShutdownReason::BLUETOOTH_CONTROLLER_DISAPPEARED;
      break;
    case NO_AVAILABLE_HOSTS:
      // If |tether_component_| was previously active but now has been shut down
      // due to no longer having a host, this means that the host became
      // unverified.
      shutdown_reason =
          TetherComponent::ShutdownReason::MULTIDEVICE_HOST_UNVERIFIED;
      break;
    case BETTER_TOGETHER_SUITE_DISABLED:
      shutdown_reason =
          TetherComponent::ShutdownReason::BETTER_TOGETHER_SUITE_DISABLED;
      break;
    default:
      PA_LOG(ERROR) << "Unexpected shutdown reason. FeatureState is "
                    << GetTetherFeatureState() << ".";
      shutdown_reason = TetherComponent::ShutdownReason::OTHER;
      break;
  }

  tether_component_->AddObserver(this);
  tether_component_->RequestShutdown(shutdown_reason);
}

void TetherService::Shutdown() {
  if (shut_down_) {
    return;
  }

  shut_down_ = true;

  // Remove all observers. This ensures that once Shutdown() is called, no more
  // calls to UpdateTetherTechnologyState() will be triggered.
  tether_host_fetcher_->RemoveObserver(this);
  power_manager_client_->RemoveObserver(this);
  network_state_handler_observer_.Reset();
  device_sync_client_->RemoveObserver(this);
  multidevice_setup_client_->RemoveObserver(this);

  if (adapter_) {
    adapter_->RemoveObserver(this);
  }

  // Shut down the feature. Note that this does not change Tether's technology
  // state in NetworkStateHandler because doing so could cause visual jank just
  // as the user logs out.
  StopTetherIfNecessary();
  tether_component_.reset();

  tether_host_fetcher_.reset();
  notification_presenter_.reset();
}

void TetherService::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  suspended_ = true;
  UpdateTetherTechnologyState();
}

void TetherService::SuspendDone(base::TimeDelta sleep_duration) {
  suspended_ = false;

  // If there was a previous TetherComponent instance in the process of an
  // asynchronous shutdown, that session is stale by this point. Kill it now, so
  // that the next session can start up immediately.
  if (tether_component_) {
    tether_component_->RemoveObserver(this);
    tether_component_.reset();
  }

  UpdateTetherTechnologyState();
}

void TetherService::OnTetherHostUpdated() {
  UpdateTetherTechnologyState();
}

void TetherService::AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                                          bool powered) {
  UpdateTetherTechnologyState();
}

void TetherService::DeviceListChanged() {
  UpdateEnabledState();
}

void TetherService::DevicePropertiesUpdated(const DeviceState* device) {
  if (device->Matches(NetworkTypePattern::Tether() |
                      NetworkTypePattern::WiFi())) {
    UpdateEnabledState();
  }
}

void TetherService::UpdateEnabledState() {
  bool was_pref_enabled = IsEnabledByPreference();
  NetworkStateHandler::TechnologyState tether_technology_state =
      network_state_handler_->GetTechnologyState(NetworkTypePattern::Tether());

  // If |was_pref_enabled| differs from the new Tether TechnologyState, the
  // settings toggle has been changed. Update the kInstantTetheringEnabled user
  // pref accordingly.
  bool is_enabled;
  if (was_pref_enabled &&
      tether_technology_state ==
          NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE) {
    is_enabled = false;
  } else if (!was_pref_enabled &&
             tether_technology_state ==
                 NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    is_enabled = true;
  } else {
    is_enabled = was_pref_enabled;
  }

  if (is_enabled != was_pref_enabled) {
    multidevice_setup_client_->SetFeatureEnabledState(
        multidevice_setup::mojom::Feature::kInstantTethering, is_enabled,
        std::nullopt /* auth_token */, base::DoNothing());
  } else {
    UpdateTetherTechnologyState();
  }
}

void TetherService::OnShutdownComplete() {
  DCHECK(tether_component_->status() == TetherComponent::Status::SHUT_DOWN);
  tether_component_->RemoveObserver(this);
  tether_component_.reset();
  PA_LOG(VERBOSE) << "TetherComponent was shut down.";

  // It is possible that the Tether TechnologyState was set to ENABLED while the
  // previous TetherComponent instance was shutting down. If that was the case,
  // restart TetherComponent.
  if (!shut_down_) {
    StartTetherIfPossible();
  }
}

void TetherService::OnReady() {
  if (shut_down_) {
    return;
  }

  OnFeatureStatesChanged(multidevice_setup_client_->GetFeatureStates());
}

void TetherService::OnFeatureStatesChanged(
    const multidevice_setup::MultiDeviceSetupClient::FeatureStatesMap&
        feature_states_map) {
  const multidevice_setup::mojom::FeatureState new_state =
      feature_states_map
          .find(multidevice_setup::mojom::Feature::kInstantTethering)
          ->second;

  // If the feature changed from enabled to disabled or vice-versa, log the
  // associated metric.
  if (new_state == multidevice_setup::mojom::FeatureState::kEnabledByUser &&
      previous_feature_state_ == TetherFeatureState::USER_PREFERENCE_DISABLED) {
    LogUserPreferenceChanged(true /* is_now_enabled */);
  } else if (new_state ==
                 multidevice_setup::mojom::FeatureState::kDisabledByUser &&
             previous_feature_state_ == TetherFeatureState::ENABLED) {
    LogUserPreferenceChanged(false /* is_now_enabled */);
  }

  if (adapter_) {
    UpdateTetherTechnologyState();
  } else {
    GetBluetoothAdapter();
  }
}

bool TetherService::HasSyncedTetherHosts() const {
  return tether_host_fetcher_->GetTetherHost().has_value();
}

void TetherService::UpdateTetherTechnologyState() {
  if (!adapter_) {
    return;
  }

  NetworkStateHandler::TechnologyState new_tether_technology_state =
      GetTetherTechnologyState();

  if (new_tether_technology_state ==
      NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED) {
    // If Tether should be enabled, notify NetworkStateHandler before starting
    // up the component. This ensures that it is not possible to add Tether
    // networks before the network stack is ready for them.
    network_state_handler_->SetTetherTechnologyState(
        new_tether_technology_state);
    StartTetherIfPossible();
  } else {
    // If Tether should not be enabled, shut down the component before notifying
    // NetworkStateHandler. This ensures that nothing in TetherComponent
    // attempts to edit Tether networks or properties when the network stack is
    // not ready for them.
    StopTetherIfNecessary();
    network_state_handler_->SetTetherTechnologyState(
        new_tether_technology_state);
  }
}

NetworkStateHandler::TechnologyState TetherService::GetTetherTechnologyState() {
  TetherFeatureState new_feature_state = GetTetherFeatureState();
  if (new_feature_state != previous_feature_state_) {
    PA_LOG(INFO) << "Tether state has changed. New state: "
                 << TetherFeatureStateToString(new_feature_state)
                 << ", Old state: "
                 << TetherFeatureStateToString(previous_feature_state_);
    previous_feature_state_ = new_feature_state;

    RecordTetherFeatureStateIfPossible();
  }

  switch (new_feature_state) {
    case SHUT_DOWN:
    case SUSPENDED:
    case BLE_NOT_PRESENT:
    case WIFI_NOT_PRESENT:
    case NO_AVAILABLE_HOSTS:
    case CELLULAR_DISABLED:
    case BETTER_TOGETHER_SUITE_DISABLED:
      return NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE;

    case PROHIBITED:
      return NetworkStateHandler::TechnologyState::TECHNOLOGY_PROHIBITED;

    case BLUETOOTH_DISABLED:
      // Instant Tethering can sometimes be made available by enabling
      // Bluetooth, in which case we should return TECHNOLOGY_UNINITIALIZED.
      // However, if the user or policy has disabled the feature, then more
      // steps are needed by user (or policy needs to be changed). In this case,
      // return TECHNOLOGY_UNAVAILABLE.
      switch (multidevice_setup_client_->GetFeatureState(
          multidevice_setup::mojom::Feature::kInstantTethering)) {
        case multidevice_setup::mojom::FeatureState::kUnavailableSuiteDisabled:
        case multidevice_setup::mojom::FeatureState::kDisabledByUser:
        case multidevice_setup::mojom::FeatureState::kProhibitedByPolicy:
          return NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE;
        default:
          return NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED;
      }

    case USER_PREFERENCE_DISABLED:
      return NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE;

    case ENABLED:
      return NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED;

    default:
      return NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE;
  }
}

void TetherService::GetBluetoothAdapter() {
  if (adapter_ || is_adapter_being_fetched_) {
    return;
  }

  is_adapter_being_fetched_ = true;

  // In the case that this is indirectly called from the constructor,
  // GetAdapter() may call OnBluetoothAdapterFetched immediately which can cause
  // problems with the Fake implementation since the class is not fully
  // constructed yet. Post the GetAdapter call to avoid this.
  auto* factory = device::BluetoothAdapterFactory::Get();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&device::BluetoothAdapterFactory::GetAdapter,
                     base::Unretained(factory),
                     base::BindOnce(&TetherService::OnBluetoothAdapterFetched,
                                    weak_ptr_factory_.GetWeakPtr())));
}

void TetherService::OnBluetoothAdapterFetched(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  is_adapter_being_fetched_ = false;

  if (shut_down_) {
    return;
  }

  adapter_ = adapter;
  adapter_->AddObserver(this);

  // Update TechnologyState in case Tether is otherwise available but Bluetooth
  // is off.
  UpdateTetherTechnologyState();
}

bool TetherService::IsBluetoothPresent() const {
  return adapter_.get() && adapter_->IsPresent();
}

bool TetherService::IsBluetoothPowered() const {
  return IsBluetoothPresent() && adapter_->IsPowered();
}

bool TetherService::IsWifiPresent() const {
  return network_state_handler_->IsTechnologyAvailable(
      NetworkTypePattern::WiFi());
}

bool TetherService::IsCellularAvailableButNotEnabled() const {
  return (network_state_handler_->IsTechnologyAvailable(
              NetworkTypePattern::Cellular()) &&
          !network_state_handler_->IsTechnologyEnabled(
              NetworkTypePattern::Cellular()));
}

bool TetherService::IsAllowedByPolicy() const {
  return profile_->GetPrefs()->GetBoolean(
      multidevice_setup::kInstantTetheringAllowedPrefName);
}

bool TetherService::IsEnabledByPreference() const {
  return profile_->GetPrefs()->GetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName);
}

TetherService::TetherFeatureState TetherService::GetTetherFeatureState() {
  if (shut_down_) {
    return SHUT_DOWN;
  }

  if (suspended_) {
    return SUSPENDED;
  }

  if (!IsBluetoothPresent()) {
    return BLE_NOT_PRESENT;
  }

  if (!IsWifiPresent()) {
    return WIFI_NOT_PRESENT;
  }

  if (!HasSyncedTetherHosts()) {
    return NO_AVAILABLE_HOSTS;
  }

  // Don't treat Tether as a subset of Cellular if the Instant Hotspot Rebrand
  // feature flag is enabled.
  if (!features::IsInstantHotspotRebrandEnabled() &&
      IsCellularAvailableButNotEnabled()) {
    return CELLULAR_DISABLED;
  }

  if (!IsBluetoothPowered()) {
    return BLUETOOTH_DISABLED;
  }

  multidevice_setup::mojom::FeatureState tether_multidevice_state =
      multidevice_setup_client_->GetFeatureState(
          multidevice_setup::mojom::Feature::kInstantTethering);
  switch (tether_multidevice_state) {
    case multidevice_setup::mojom::FeatureState::kProhibitedByPolicy:
      return PROHIBITED;
    case multidevice_setup::mojom::FeatureState::kDisabledByUser:
      return USER_PREFERENCE_DISABLED;
    case multidevice_setup::mojom::FeatureState::kEnabledByUser:
      return ENABLED;
    case multidevice_setup::mojom::FeatureState::kUnavailableSuiteDisabled:
      return BETTER_TOGETHER_SUITE_DISABLED;
    case multidevice_setup::mojom::FeatureState::
        kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified:
      [[fallthrough]];
    case multidevice_setup::mojom::FeatureState::
        kUnavailableNoVerifiedHost_ClientNotReady:
      [[fallthrough]];
    case multidevice_setup::mojom::FeatureState::
        kUnavailableNoVerifiedHost_NoEligibleHosts:
      [[fallthrough]];
    case multidevice_setup::mojom::FeatureState::kNotSupportedByChromebook:
      // CryptAuth may not yet know that this device supports
      // MAGIC_TETHER_CLIENT (and the local device metadata is reflecting
      // that). This should be resolved shortly once DeviceReenroller realizes
      // reconciles the discrepancy. For now, fall through to mark as
      // unavailable.
      [[fallthrough]];
    case multidevice_setup::mojom::FeatureState::kNotSupportedByPhone:
      return NO_AVAILABLE_HOSTS;
    default:
      // Other FeatureStates:
      //   *kUnavailableInsufficientSecurity: Should never occur.
      PA_LOG(ERROR) << "Invalid MultiDevice FeatureState: "
                    << tether_multidevice_state;
      NOTREACHED_IN_MIGRATION();
      return NO_AVAILABLE_HOSTS;
  }
}

void TetherService::RecordTetherFeatureState() {
  TetherFeatureState tether_feature_state = GetTetherFeatureState();
  DCHECK(tether_feature_state != TetherFeatureState::TETHER_FEATURE_STATE_MAX);

  // If the feature is shut down, there is no need to log a metric. Since this
  // state occurs every time the user logs out (as of crbug.com/782879), logging
  // a metric here does not provide any value since it does not indicate
  // anything about how the user utilizes Instant Tethering and would dilute the
  // contributions of meaningful states.
  if (tether_feature_state == TetherFeatureState::SHUT_DOWN) {
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("InstantTethering.FeatureState",
                            tether_feature_state,
                            TetherFeatureState::TETHER_FEATURE_STATE_MAX);
}

void TetherService::RecordTetherFeatureStateIfPossible() {
  if (HandleFeatureStateMetricIfUninitialized()) {
    return;
  }

  // If the timer meant to record the initial
  // TetherFeatureState::BLE_NOT_PRESENT value is running, cancel it -- it is a
  // false positive report.
  if (timer_->IsRunning()) {
    timer_->Stop();
  }

  RecordTetherFeatureState();
}

bool TetherService::HandleFeatureStateMetricIfUninitialized() {
  TetherFeatureState current_state = GetTetherFeatureState();
  if (current_state != TetherFeatureState::BLE_NOT_PRESENT &&
      current_state != TetherFeatureState::NO_AVAILABLE_HOSTS) {
    // These are the only two possible false-positive states. If the current
    // state is another state, no processing is needed.
    return false;
  }

  // When TetherService starts up, we expect that BLE is not present.
  // Eventually, BLE starts up, but at that point, Tether hosts are not yet
  // fetched. During startup, these states are transient, so it would be
  // incorrect to log a metric stating that this was the state upon startup.
  bool should_start_timer = false;
  if (current_state == TetherFeatureState::BLE_NOT_PRESENT &&
      !ble_not_present_false_positive_encountered_) {
    ble_not_present_false_positive_encountered_ = true;
    should_start_timer = true;
  } else if (current_state == TetherFeatureState::NO_AVAILABLE_HOSTS &&
             !no_available_hosts_false_positive_encountered_) {
    no_available_hosts_false_positive_encountered_ = true;
    should_start_timer = true;
  }

  if (!should_start_timer) {
    return false;
  }

  // Start the timer. If it fires without being stopped, the metric will be
  // recorded. |kMetricFalsePositiveSeconds| is chosen such that it is long
  // enough that we can assume a false positive did not occur and that the
  // metric value is actually correct.
  timer_->Start(FROM_HERE, base::Seconds(kMetricFalsePositiveSeconds),
                base::BindOnce(&TetherService::RecordTetherFeatureState,
                               weak_ptr_factory_.GetWeakPtr()));

  return true;
}

void TetherService::LogUserPreferenceChanged(bool is_now_enabled) {
  UMA_HISTOGRAM_BOOLEAN("InstantTethering.UserPreference.OnToggle",
                        is_now_enabled);
  PA_LOG(VERBOSE) << "Tether user preference changed. New value: "
                  << is_now_enabled;
}

void TetherService::SetTestDoubles(
    std::unique_ptr<NotificationPresenter> notification_presenter,
    std::unique_ptr<base::OneShotTimer> timer) {
  notification_presenter_ = std::move(notification_presenter);
  timer_ = std::move(timer);
}

}  // namespace tether
}  // namespace ash
