// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"

#include <memory>
#include <string_view>

#include "ash/constants/ash_switches.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client_impl.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/psm/construct_rlwe_id.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client_impl.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/device_management/install_attributes_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_sync_observation.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

// This is used for logs that may not be strictly necessary but are of great use
// because they will log whether determinations are needed or not, along with
// some context. The information used to be logged using VLOG(1), and therefore
// was not available in customer logs. Because the only other logs have some
// ambiguity (e.g. there will not be a log if the device decides it does not
// need to make a determination), troubleshooting is difficult. If this changes,
// this can be made VLOG(1) again.
//
// We use LOG(WARNING) to guarantee that the messages will be into feedback
// reports.
#define LOG_DETERMINATION() LOG(WARNING)

namespace policy {
namespace {

const int kMaxRequestStateKeysTries = 10;

// Maximum time to wait for the auto-enrollment check to reach a decision.
// Note that this encompasses all steps `AutoEnrollmentController` performs in
// order to determine if the device should be auto-enrolled.
// If `kSafeguardTimeout` after `Start()` has been called,
// `AutoEnrollmentController::state()` is still AutoEnrollmentState::kPending,
// the AutoEnrollmentController will switch to
// `AutoEnrollmentResult::kNoEnrollment` or
// `AutoEnrollmentSafeguardTimeoutError` (see
// `AutoEnrollmentController::Timeout`). Note that this timeout should not be
// too short, because one of the steps `AutoEnrollmentController` performs -
// downloading identifier hash buckets - can be non-negligible, especially on 2G
// connections.
constexpr base::TimeDelta kSafeguardTimeout = base::Seconds(90);

// Maximum time to wait for time sync before forcing a decision on whether
// Initial Enrollment should be performed. This corresponds to at least seven
// TCP retransmissions attempts to the remote server used to update the system
// clock.
constexpr base::TimeDelta kSystemClockSyncWaitTimeout = base::Seconds(45);

// A callback that will be invoked when the system clock has been synchronized,
// or if system clock synchronization has failed.
using SystemClockSyncCallback = base::OnceCallback<void(
    AutoEnrollmentController::SystemClockSyncState system_clock_sync_state)>;

// Returns the int value of the `switch_name` argument, clamped to the [0, 62]
// interval. Returns 0 if the argument doesn't exist or isn't an int value.
int GetSanitizedArg(const std::string& switch_name) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_name)) {
    return 0;
  }
  std::string value = command_line->GetSwitchValueASCII(switch_name);
  int int_value;
  if (!base::StringToInt(value, &int_value)) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" is not a valid int. "
               << "Defaulting to 0.";
    return 0;
  }
  if (int_value < 0) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" can't be negative. "
               << "Using 0";
    return 0;
  }
  if (int_value > AutoEnrollmentClient::kMaximumPower) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" can't be greater than "
               << AutoEnrollmentClient::kMaximumPower << ". Using "
               << AutoEnrollmentClient::kMaximumPower << ".";
    return AutoEnrollmentClient::kMaximumPower;
  }
  return int_value;
}

bool IsSystemClockSynchronized(
    AutoEnrollmentController::SystemClockSyncState state) {
  switch (state) {
    case AutoEnrollmentController::SystemClockSyncState::kSynchronized:
    case AutoEnrollmentController::SystemClockSyncState::kSyncFailed:
      return true;
    case AutoEnrollmentController::SystemClockSyncState::kCanWaitForSync:
    case AutoEnrollmentController::SystemClockSyncState::kWaitingForSync:
      return false;
  }
}

enum class AutoEnrollmentControllerTimeoutReport {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  kTimeoutCancelled = 0,
  kTimeoutFRE = 1,
  kTimeout = 2,
  kTimeoutUnified = 3,
  kMaxValue = kTimeoutUnified
};

void ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport report) {
  base::UmaHistogramEnumeration("Enterprise.AutoEnrollmentControllerTimeout",
                                report);
}

bool IsFinalAutoEnrollmentState(AutoEnrollmentState state) {
  return state.has_value();
}

}  // namespace

EnrollmentFwmpHelper::EnrollmentFwmpHelper(
    ash::InstallAttributesClient* install_attributes_client)
    : install_attributes_client_(install_attributes_client) {}

EnrollmentFwmpHelper::~EnrollmentFwmpHelper() = default;

void EnrollmentFwmpHelper::DetermineDevDisableBoot(
    ResultCallback result_callback) {
  // D-Bus services may not be available yet, so we call
  // WaitForServiceToBeAvailable. See https://crbug.com/841627.
  install_attributes_client_->WaitForServiceToBeAvailable(base::BindOnce(
      &EnrollmentFwmpHelper::RequestFirmwareManagementParameters,
      weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
}

void EnrollmentFwmpHelper::RequestFirmwareManagementParameters(
    ResultCallback result_callback,
    bool service_is_ready) {
  if (!service_is_ready) {
    LOG(ERROR) << "Failed waiting for cryptohome D-Bus service availability.";
    return std::move(result_callback).Run(false);
  }

  device_management::GetFirmwareManagementParametersRequest request;
  install_attributes_client_->GetFirmwareManagementParameters(
      request,
      base::BindOnce(
          &EnrollmentFwmpHelper::OnGetFirmwareManagementParametersReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
}

void EnrollmentFwmpHelper::OnGetFirmwareManagementParametersReceived(
    ResultCallback result_callback,
    std::optional<device_management::GetFirmwareManagementParametersReply>
        reply) {
  if (!reply.has_value() || reply->error() !=
                                device_management::DeviceManagementErrorCode::
                                    DEVICE_MANAGEMENT_ERROR_NOT_SET) {
    LOG(ERROR) << "Failed to retrieve firmware management parameters.";
    return std::move(result_callback).Run(false);
  }

  const bool dev_disable_boot =
      (reply->fwmp().flags() & cryptohome::DEVELOPER_DISABLE_BOOT);

  std::move(result_callback).Run(dev_disable_boot);
}

AutoEnrollmentController::AutoEnrollmentController(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : AutoEnrollmentController(
          ash::DeviceSettingsService::Get(),
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->device_management_service(),
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->GetStateKeysBroker(),
          ash::NetworkHandler::Get()->network_state_handler(),
          std::make_unique<AutoEnrollmentClientImpl::FactoryImpl>(),
          base::BindRepeating(&policy::psm::RlweDmserverClientImpl::Create),
          base::BindRepeating(EnrollmentStateFetcher::Create),
          shared_url_loader_factory) {}

AutoEnrollmentController::AutoEnrollmentController(
    ash::DeviceSettingsService* device_settings_service,
    DeviceManagementService* device_management_service,
    ServerBackedStateKeysBroker* state_keys_broker,
    ash::NetworkStateHandler* network_state_handler,
    std::unique_ptr<AutoEnrollmentClient::Factory>
        auto_enrollment_client_factory,
    RlweClientFactory psm_rlwe_client_factory,
    EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : device_settings_service_(device_settings_service),
      device_management_service_(device_management_service),
      state_keys_broker_(state_keys_broker),
      enrollment_fwmp_helper_(std::make_unique<EnrollmentFwmpHelper>(
          ash::InstallAttributesClient::Get())),
      auto_enrollment_client_factory_(
          std::move(auto_enrollment_client_factory)),
      psm_rlwe_client_factory_(std::move(psm_rlwe_client_factory)),
      enrollment_state_fetcher_factory_(
          std::move(enrollment_state_fetcher_factory)),
      shared_url_loader_factory_(shared_url_loader_factory),
      network_state_handler_(network_state_handler) {}

AutoEnrollmentController::~AutoEnrollmentController() = default;

void AutoEnrollmentController::Start() {
  if (state_.has_value() && IsFinalAutoEnrollmentState(state_.value())) {
    return;
  }

  if (!network_state_observation_.IsObserving()) {
    // The controller could have already subscribed on the start and now we're
    // restarting after an error.
    network_state_observation_.Observe(network_state_handler_);
  }

  if (IsInProgress()) {
    return;
  }

  // Arm the belts-and-suspenders timer to avoid hangs.
  safeguard_timer_.Start(FROM_HERE, kSafeguardTimeout,
                         base::BindOnce(&AutoEnrollmentController::Timeout,
                                        weak_ptr_factory_.GetWeakPtr()));

  if (AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled()) {
    // Emulate required FRE to prevent users from skipping enrollment.
    auto_enrollment_check_type_ = AutoEnrollmentTypeChecker::CheckType::
        kForcedReEnrollmentExplicitlyRequired;

    // TODO(b/353731379): BrowserPolicyConnector::ScheduleServiceInitialization.
    if (device_management_service_) {
      device_management_service_->ScheduleInitialization(0);
    } else {
      CHECK_IS_TEST();
    }

    LOG(WARNING) << "Starting state determination";
    enrollment_state_fetcher_ = enrollment_state_fetcher_factory_.Run(
        base::BindRepeating(&AutoEnrollmentController::UpdateState,
                            weak_ptr_factory_.GetWeakPtr()),
        g_browser_process->local_state(), psm_rlwe_client_factory_,
        device_management_service_, shared_url_loader_factory_,
        state_keys_broker_, device_settings_service_,
        ash::OobeConfiguration::Get());

    enrollment_state_fetcher_->Start();
    return;
  }

  request_state_keys_tries_ = 0;

  // The system clock sync state is not known yet, and this
  // `AutoEnrollmentController` could wait for it if requested.
  system_clock_sync_state_ = SystemClockSyncState::kCanWaitForSync;

  LOG(WARNING) << "Starting legacy state determination";
  enrollment_fwmp_helper_->DetermineDevDisableBoot(
      base::BindOnce(&AutoEnrollmentController::OnDevDisableBootDetermined,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::OnDevDisableBootDetermined(
    bool dev_disable_boot) {
  dev_disable_boot_ = dev_disable_boot;

  StartWithSystemClockSyncState();
}

void AutoEnrollmentController::StartWithSystemClockSyncState() {
  auto_enrollment_check_type_ =
      AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          IsSystemClockSynchronized(system_clock_sync_state_),
          ash::system::StatisticsProvider::GetInstance(), dev_disable_boot_);
  if (auto_enrollment_check_type_ ==
      AutoEnrollmentTypeChecker::CheckType::kNone) {
    UpdateState(AutoEnrollmentResult::kNoEnrollment);
    return;
  }
  // If waiting for system clock synchronization has been triggered, wait until
  // it finishes (this function will be called again when a result is
  // available).
  if (system_clock_sync_state_ == SystemClockSyncState::kWaitingForSync) {
    return;
  }

  if (auto_enrollment_check_type_ == AutoEnrollmentTypeChecker::CheckType::
                                         kUnknownDueToMissingSystemClockSync) {
    DCHECK_EQ(system_clock_sync_state_, SystemClockSyncState::kCanWaitForSync);
    system_clock_sync_state_ = SystemClockSyncState::kWaitingForSync;

    LOG(WARNING) << "Waiting for clock sync";
    // Use `client_start_weak_factory_` so the callback is not invoked if
    // `Timeout` has been called in the meantime (after `kSafeguardTimeout`).
    system_clock_sync_observation_ =
        ash::SystemClockSyncObservation::WaitForSystemClockSync(
            ash::SystemClockClient::Get(), kSystemClockSyncWaitTimeout,
            base::BindOnce(&AutoEnrollmentController::OnSystemClockSyncResult,
                           client_start_weak_factory_.GetWeakPtr()));
    return;
  }

  LOG(WARNING) << "Get ownership status to check if it's enrollment recovery";
  device_settings_service_->GetOwnershipStatusAsync(
      base::BindOnce(&AutoEnrollmentController::OnOwnershipStatusCheckDone,
                     client_start_weak_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::Retry() {
  if (client_) {
    client_->Retry();
  } else {
    Start();
  }
}

base::CallbackListSubscription
AutoEnrollmentController::RegisterProgressCallback(
    const ProgressCallbackList::CallbackType& callback) {
  return progress_callbacks_.Add(callback);
}

void AutoEnrollmentController::PortalStateChanged(
    const ash::NetworkState* /*default_network*/,
    const ash::NetworkState::PortalState portal_state) {
  // It is safe to retry regardless of the current state: if the check is idle
  // or failed, we will restart the check process. If the check is in progress,
  // the retry call will be ignored.
  if (portal_state == ash::NetworkState::PortalState::kOnline) {
    Retry();
  }
}

void AutoEnrollmentController::OnShuttingDown() {
  network_state_observation_.Reset();
}

void AutoEnrollmentController::SetRlweClientFactoryForTesting(
    RlweClientFactory test_factory) {
  CHECK_IS_TEST();
  psm_rlwe_client_factory_ = std::move(test_factory);
}

void AutoEnrollmentController::SetAutoEnrollmentClientFactoryForTesting(
    std::unique_ptr<AutoEnrollmentClient::Factory>
        auto_enrollment_client_factory) {
  CHECK_IS_TEST();
  auto_enrollment_client_factory_ = std::move(auto_enrollment_client_factory);
}

void AutoEnrollmentController::OnOwnershipStatusCheckDone(
    ash::DeviceSettingsService::OwnershipStatus status) {
  switch (status) {
    case ash::DeviceSettingsService::OwnershipStatus::kOwnershipNone:
      switch (auto_enrollment_check_type_) {
        case AutoEnrollmentTypeChecker::CheckType::
            kForcedReEnrollmentExplicitlyRequired:
        case AutoEnrollmentTypeChecker::CheckType::
            kForcedReEnrollmentImplicitlyRequired:
          ++request_state_keys_tries_;
          // For FRE, request state keys first.
          LOG(WARNING) << "Requesting state keys. Attempt "
                       << request_state_keys_tries_ << ".";
          state_keys_broker_->RequestStateKeys(
              base::BindOnce(&AutoEnrollmentController::StartClientForFRE,
                             client_start_weak_factory_.GetWeakPtr()));
          break;
        case AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination:
          LOG(WARNING) << "Start client for initial state determination.";
          StartClientForInitialEnrollment();
          break;
        case AutoEnrollmentTypeChecker::CheckType::
            kUnknownDueToMissingSystemClockSync:
        case AutoEnrollmentTypeChecker::CheckType::kNone:
          // The ownership check is only triggered if
          // `auto_enrollment_check_type_` indicates that an auto-enrollment
          // check should be done.
          NOTREACHED_IN_MIGRATION();
          break;
      }
      return;
    case ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken:
      LOG(WARNING) << "Device already owned, skipping auto-enrollment check.";
      UpdateState(AutoEnrollmentResult::kNoEnrollment);
      return;
    case ash::DeviceSettingsService::OwnershipStatus::kOwnershipUnknown:
      LOG(ERROR) << "Ownership unknown, skipping auto-enrollment check.";
      UpdateState(AutoEnrollmentResult::kNoEnrollment);
      return;
  }
}

void AutoEnrollmentController::StartClientForFRE(
    const std::vector<std::string>& state_keys) {
  if (state_keys.empty()) {
    LOG(ERROR) << "No state keys available.";
    if (auto_enrollment_check_type_ ==
        AutoEnrollmentTypeChecker::CheckType::
            kForcedReEnrollmentExplicitlyRequired) {
      if (request_state_keys_tries_ >= kMaxRequestStateKeysTries) {
        if (safeguard_timer_.IsRunning()) {
          safeguard_timer_.Stop();
        }
        Timeout();
        return;
      }
      ++request_state_keys_tries_;
      // Retry to fetch the state keys. For devices where FRE is required to be
      // checked, we can't proceed with empty state keys.
      state_keys_broker_->RequestStateKeys(
          base::BindOnce(&AutoEnrollmentController::StartClientForFRE,
                         client_start_weak_factory_.GetWeakPtr()));
    } else {
      UpdateState(AutoEnrollmentResult::kNoEnrollment);
    }
    return;
  }

  int power_initial =
      GetSanitizedArg(ash::switches::kEnterpriseEnrollmentInitialModulus);
  int power_limit =
      GetSanitizedArg(ash::switches::kEnterpriseEnrollmentModulusLimit);
  if (power_initial > power_limit) {
    LOG(ERROR) << "Initial auto-enrollment modulus is larger than the limit, "
                  "clamping to the limit.";
    power_initial = power_limit;
  }

  device_management_service_->ScheduleInitialization(0);

  client_ = auto_enrollment_client_factory_->CreateForFRE(
      base::BindRepeating(&AutoEnrollmentController::UpdateState,
                          weak_ptr_factory_.GetWeakPtr()),
      device_management_service_, g_browser_process->local_state(),
      shared_url_loader_factory_, state_keys.front(), power_initial,
      power_limit);

  LOG(WARNING) << "Starting auto-enrollment client for FRE.";
  client_->Start();
}

void AutoEnrollmentController::OnSystemClockSyncResult(
    bool system_clock_synchronized) {
  system_clock_sync_state_ = system_clock_synchronized
                                 ? SystemClockSyncState::kSynchronized
                                 : SystemClockSyncState::kSyncFailed;
  LOG(WARNING) << "System clock "
               << (system_clock_synchronized ? "synchronized"
                                             : "failed to synchronize");
  // Only call StartWithSystemClockSyncState() to determine the auto-enrollment
  // type if the system clock could synchronize successfully. Otherwise, return
  // an error to show to not to proceed with the auto-enrollment checks until
  // AutoEnrollmentController::Start() is called again by a network state
  // change or network selection.
  if (system_clock_sync_state_ == SystemClockSyncState::kSynchronized) {
    StartWithSystemClockSyncState();
  } else {
    UpdateState(base::unexpected(AutoEnrollmentSystemClockSyncError{}));
  }
}

void AutoEnrollmentController::StartClientForInitialEnrollment() {
  device_management_service_->ScheduleInitialization(0);

  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  const std::optional<std::string_view> serial_number =
      provider->GetMachineID();
  const std::optional<std::string_view> rlz_brand_code =
      provider->GetMachineStatistic(ash::system::kRlzBrandCodeKey);
  // The Initial State Determination should not be started if the serial number
  // or brand code are missing. This is ensured in
  // `GetInitialStateDeterminationRequirement`.
  CHECK(serial_number);
  CHECK(!serial_number->empty());
  CHECK(rlz_brand_code);
  CHECK(!rlz_brand_code->empty());

  const auto plaintext_id = psm::ConstructRlweId();
  client_ = auto_enrollment_client_factory_->CreateForInitialEnrollment(
      base::BindRepeating(&AutoEnrollmentController::UpdateState,
                          weak_ptr_factory_.GetWeakPtr()),
      device_management_service_, g_browser_process->local_state(),
      shared_url_loader_factory_, std::string(serial_number.value()),
      std::string(rlz_brand_code.value()),
      std::make_unique<psm::RlweDmserverClientImpl>(
          device_management_service_, shared_url_loader_factory_, plaintext_id,
          psm_rlwe_client_factory_),
      ash::OobeConfiguration::Get());

  LOG(WARNING) << "Starting auto-enrollment client for Initial Enrollment.";
  client_->Start();
}

void AutoEnrollmentController::UpdateState(AutoEnrollmentState new_state) {
  LOG(WARNING) << "New auto-enrollment state: "
               << AutoEnrollmentStateToString(new_state);
  state_ = new_state;

  if (IsFinalAutoEnrollmentState(state_.value())) {
    network_state_observation_.Reset();
  }

  // Stop the safeguard timer once a result comes in.
  safeguard_timer_.Stop();
  // Reset enrollment state fetcher to allow restarting.
  enrollment_state_fetcher_.reset();
  ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeoutCancelled);

  // Device disabling mode is relying on device state stored in install
  // attributes. In case that file is corrupted, this should prevent device
  // re-enabling.
  if (state_ == AutoEnrollmentResult::kDisabled) {
    DeviceMode device_mode = ash::InstallAttributes::Get()->GetMode();
    if (device_mode == DeviceMode::DEVICE_MODE_PENDING ||
        device_mode == DeviceMode::DEVICE_MODE_NOT_SET) {
      device_settings_service_->SetDeviceMode(
          DeviceMode::DEVICE_MODE_ENTERPRISE);
    }
  }

  if (state_ == AutoEnrollmentResult::kNoEnrollment ||
      state_ == AutoEnrollmentResult::kSuggestedEnrollment) {
    StartCleanupForcedReEnrollment();
  } else {
    progress_callbacks_.Notify(state_.value());
  }
}

void AutoEnrollmentController::StartCleanupForcedReEnrollment() {
  // D-Bus services may not be available yet, so we call
  // WaitForServiceToBeAvailable. See https://crbug.com/841627.
  ash::InstallAttributesClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(
          &AutoEnrollmentController::StartRemoveFirmwareManagementParameters,
          weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::StartRemoveFirmwareManagementParameters(
    bool service_is_ready) {
  DCHECK(state_ == AutoEnrollmentResult::kNoEnrollment ||
         state_ == AutoEnrollmentResult::kSuggestedEnrollment);
  if (!service_is_ready) {
    LOG(ERROR) << "Failed waiting for cryptohome D-Bus service availability.";
    progress_callbacks_.Notify(state_.value());
    return;
  }

  device_management::RemoveFirmwareManagementParametersRequest request;
  ash::InstallAttributesClient::Get()->RemoveFirmwareManagementParameters(
      request,
      base::BindOnce(
          &AutoEnrollmentController::OnFirmwareManagementParametersRemoved,
          weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::OnFirmwareManagementParametersRemoved(
    std::optional<device_management::RemoveFirmwareManagementParametersReply>
        reply) {
  if (!reply.has_value() || reply->error() !=
                                device_management::DeviceManagementErrorCode::
                                    DEVICE_MANAGEMENT_ERROR_NOT_SET) {
    LOG(ERROR) << "Failed to remove firmware management parameters.";
  }

  // D-Bus services may not be available yet, so we call
  // WaitForServiceToBeAvailable. See https://crbug.com/841627.
  ash::SessionManagerClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&AutoEnrollmentController::StartClearForcedReEnrollmentVpd,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::StartClearForcedReEnrollmentVpd(
    bool service_is_ready) {
  DCHECK(state_ == AutoEnrollmentResult::kNoEnrollment ||
         state_ == AutoEnrollmentResult::kSuggestedEnrollment);
  if (!service_is_ready) {
    LOG(ERROR)
        << "Failed waiting for session_manager D-Bus service availability.";
    progress_callbacks_.Notify(state_.value());
    return;
  }

  ash::SessionManagerClient::Get()->ClearForcedReEnrollmentVpd(
      base::BindOnce(&AutoEnrollmentController::OnForcedReEnrollmentVpdCleared,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::OnForcedReEnrollmentVpdCleared(bool reply) {
  if (!reply) {
    LOG(ERROR) << "Failed to clear forced re-enrollment flags in RW VPD.";
  }

  progress_callbacks_.Notify(state_.value());
}

void AutoEnrollmentController::Timeout() {
  if (AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled()) {
    // This can actually happen in some cases, for example when state key
    // generation is waiting for time sync or the server just doesn't reply and
    // keeps the connection open.
    LOG(ERROR) << "EnrollmentStateFetcher didn't complete within time limit.";
    UpdateState(base::unexpected(AutoEnrollmentSafeguardTimeoutError{}));
    ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeoutUnified);
    return;
  }

  // When tightening the FRE flows, as a cautionary measure (to prevent
  // interference with consumer devices) timeout was chosen to only enforce FRE
  // for EXPLICITLY_REQUIRED.
  // TODO(igorcov): Investigate the remaining causes of hitting timeout and
  // potentially either remove the timeout altogether or enforce FRE in the
  // REQUIRED case as well.
  if (client_start_weak_factory_.HasWeakPtrs() &&
      auto_enrollment_check_type_ !=
          AutoEnrollmentTypeChecker::CheckType::
              kForcedReEnrollmentExplicitlyRequired) {
    // If the callbacks to check ownership status or state keys are still
    // pending, there's a bug in the code running on the device. No use in
    // retrying anything, need to fix that bug.
    LOG(ERROR) << "Failed to start auto-enrollment check, fix the code!";
    UpdateState(AutoEnrollmentResult::kNoEnrollment);
    ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeout);
  } else {
    // This can actually happen in some cases, for example when state key
    // generation is waiting for time sync or the server just doesn't reply and
    // keeps the connection open.
    LOG(ERROR) << "AutoEnrollmentClient didn't complete within time limit.";
    UpdateState(base::unexpected(AutoEnrollmentSafeguardTimeoutError{}));
    ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeoutFRE);
  }

  client_.reset();

  // Make sure to nuke pending `client_` start sequences.
  client_start_weak_factory_.InvalidateWeakPtrs();
}

bool AutoEnrollmentController::IsInProgress() const {
  if (AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled()) {
    if (enrollment_state_fetcher_) {
      // If a fetcher has already been created, bail out.
      VLOG(1) << "Enrollment state fetcher is already running.";
      return true;
    }

    return false;
  }

  // If a client is being created or already existing, bail out.
  if (client_start_weak_factory_.HasWeakPtrs() || client_) {
    VLOG(1) << "Enrollment state client is already running.";
    return true;
  }

  // The timer runs from `Start()` where controller starts determining state,
  // till `UpdateState()` where the controller receives a state or an error.
  // Hence it can be used to decide whether the controller is running or not.
  // If any of steps between `Start()` and `UpdateState()` are excluded from
  // the timing, or the timer is extended to some other steps, the check will
  // become wrong.
  if (safeguard_timer_.IsRunning()) {
    VLOG(1) << "State determination is already running.";
    return true;
  }

  return false;
}

void AutoEnrollmentController::SetEnrollmentStateFetcherFactoryForTesting(
    EnrollmentStateFetcher::Factory enrollment_state_fetcher_factory) {
  CHECK_IS_TEST();
  if (enrollment_state_fetcher_factory) {
    enrollment_state_fetcher_factory_ = enrollment_state_fetcher_factory;
  } else {
    enrollment_state_fetcher_factory_ =
        base::BindRepeating(EnrollmentStateFetcher::Create);
  }
}

}  // namespace policy
