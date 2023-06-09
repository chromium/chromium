// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"

#include <memory>

#include "ash/constants/ash_switches.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client_impl.h"
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
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_sync_observation.h"
#include "chromeos/ash/components/dbus/userdataauth/install_attributes_client.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

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
// AutoEnrollmentState::kNoEnrollment or AutoEnrollmentState::kConnectionError
// (see `AutoEnrollmentController::Timeout`). Note that this timeout should not
// be too short, because one of the steps `AutoEnrollmentController` performs -
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

std::string AutoEnrollmentStateToString(AutoEnrollmentState state) {
  switch (state) {
    case AutoEnrollmentState::kIdle:
      return "Not started";
    case AutoEnrollmentState::kPending:
      return "Pending";
    case AutoEnrollmentState::kConnectionError:
      return "Connection error";
    case AutoEnrollmentState::kServerError:
      return "Server error";
    case AutoEnrollmentState::kEnrollment:
      return "Enrollment";
    case AutoEnrollmentState::kNoEnrollment:
      return "No enrollment";
    case AutoEnrollmentState::kDisabled:
      return "Device disabled";
  }
}

// Schedules immediate initialization of the `DeviceManagementService` and
// returns it.
DeviceManagementService* InitializeAndGetDeviceManagementService() {
  BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  DeviceManagementService* service = connector->device_management_service();
  service->ScheduleInitialization(0);
  return service;
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

  user_data_auth::GetFirmwareManagementParametersRequest request;
  install_attributes_client_->GetFirmwareManagementParameters(
      request,
      base::BindOnce(
          &EnrollmentFwmpHelper::OnGetFirmwareManagementParametersReceived,
          weak_ptr_factory_.GetWeakPtr(), std::move(result_callback)));
}

void EnrollmentFwmpHelper::OnGetFirmwareManagementParametersReceived(
    ResultCallback result_callback,
    absl::optional<user_data_auth::GetFirmwareManagementParametersReply>
        reply) {
  if (!reply.has_value() ||
      reply->error() !=
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
    LOG(ERROR) << "Failed to retrieve firmware management parameters.";
    return std::move(result_callback).Run(false);
  }

  const bool dev_disable_boot =
      (reply->fwmp().flags() & cryptohome::DEVELOPER_DISABLE_BOOT);

  std::move(result_callback).Run(dev_disable_boot);
}

AutoEnrollmentController::AutoEnrollmentController(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory)
    : enrollment_fwmp_helper_(ash::InstallAttributesClient::Get()),
      psm_rlwe_client_factory_(
          base::BindRepeating(&policy::psm::RlweDmserverClientImpl::Create)),
      enrollment_state_fetcher_factory_(
          base::BindRepeating(EnrollmentStateFetcher::Create)),
      shared_url_loader_factory_(shared_url_loader_factory) {}

AutoEnrollmentController::~AutoEnrollmentController() = default;

void AutoEnrollmentController::Start() {
  LOG(WARNING) << "Starting auto-enrollment controller.";
  switch (state_) {
    case AutoEnrollmentState::kPending:
      // Abort re-start if the check is still running.
      return;
    case AutoEnrollmentState::kNoEnrollment:
    case AutoEnrollmentState::kEnrollment:
    case AutoEnrollmentState::kDisabled:
      // Abort re-start when there's already a final decision.
      return;

    case AutoEnrollmentState::kIdle:
    case AutoEnrollmentState::kConnectionError:
    case AutoEnrollmentState::kServerError:
      // Continue (re-)start.
      break;
  }

  if (!AutoEnrollmentTypeChecker::Initialized()) {
    if (!auto_enrollment_check_type_init_started_) {
      auto_enrollment_check_type_init_started_ = true;
      AutoEnrollmentTypeChecker::Initialize(
          shared_url_loader_factory_,
          base::BindOnce(&AutoEnrollmentController::Start,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  if (AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled()) {
    // If a fetcher has already been created, bail out.
    if (enrollment_state_fetcher_) {
      LOG(ERROR) << "Enrollment state fetcher is already running.";
      return;
    }
  } else {
    // If a client is being created or already existing, bail out.
    if (client_start_weak_factory_.HasWeakPtrs() || client_) {
      LOG(ERROR) << "Enrollment state client is already running.";
      return;
    }
  }

  // Arm the belts-and-suspenders timer to avoid hangs.
  safeguard_timer_.Start(FROM_HERE, kSafeguardTimeout,
                         base::BindOnce(&AutoEnrollmentController::Timeout,
                                        weak_ptr_factory_.GetWeakPtr()));

  if (AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled()) {
    // Emulate required FRE to prevent users from skipping enrollment.
    auto_enrollment_check_type_ = AutoEnrollmentTypeChecker::CheckType::
        kForcedReEnrollmentExplicitlyRequired;
    // Set state to kPending since EnrollmentStateFetcher does not invoke update
    // state callback until final state is available.
    UpdateState(AutoEnrollmentState::kPending);

    enrollment_state_fetcher_ = enrollment_state_fetcher_factory_.Run(
        base::BindRepeating(&AutoEnrollmentController::UpdateState,
                            weak_ptr_factory_.GetWeakPtr()),
        g_browser_process->local_state(), psm_rlwe_client_factory_,
        InitializeAndGetDeviceManagementService(), shared_url_loader_factory_,
        ash::SystemClockClient::Get(),
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetStateKeysBroker(),
        ash::DeviceSettingsService::Get());

    enrollment_state_fetcher_->Start();
    return;
  }

  request_state_keys_tries_ = 0;

  // The system clock sync state is not known yet, and this
  // `AutoEnrollmentController` could wait for it if requested.
  system_clock_sync_state_ = SystemClockSyncState::kCanWaitForSync;

  enrollment_fwmp_helper_.DetermineDevDisableBoot(
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
    UpdateState(AutoEnrollmentState::kNoEnrollment);
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

    // Set state before waiting for the system clock sync, because
    // `WaitForSystemClockSync` may invoke its callback synchronously if the
    // system clock sync status is already known.
    UpdateState(AutoEnrollmentState::kPending);

    // Use `client_start_weak_factory_` so the callback is not invoked if
    // `Timeout` has been called in the meantime (after `kSafeguardTimeout`).
    system_clock_sync_observation_ =
        ash::SystemClockSyncObservation::WaitForSystemClockSync(
            ash::SystemClockClient::Get(), kSystemClockSyncWaitTimeout,
            base::BindOnce(&AutoEnrollmentController::OnSystemClockSyncResult,
                           client_start_weak_factory_.GetWeakPtr()));
    return;
  }

  // Start by checking if the device has already been owned.
  UpdateState(AutoEnrollmentState::kPending);
  ash::DeviceSettingsService::Get()->GetOwnershipStatusAsync(
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

void AutoEnrollmentController::SetRlweClientFactoryForTesting(
    RlweClientFactory test_factory) {
  CHECK_IS_TEST();
  psm_rlwe_client_factory_ = std::move(test_factory);
}

void AutoEnrollmentController::SetAutoEnrollmentClientFactoryForTesting(
    AutoEnrollmentClient::Factory* auto_enrollment_client_factory) {
  CHECK_IS_TEST();
  testing_auto_enrollment_client_factory_ = auto_enrollment_client_factory;
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
          g_browser_process->platform_part()
              ->browser_policy_connector_ash()
              ->GetStateKeysBroker()
              ->RequestStateKeys(
                  base::BindOnce(&AutoEnrollmentController::StartClientForFRE,
                                 client_start_weak_factory_.GetWeakPtr()));
          break;
        case AutoEnrollmentTypeChecker::CheckType::kInitialStateDetermination:
          StartClientForInitialEnrollment();
          break;
        case AutoEnrollmentTypeChecker::CheckType::
            kUnknownDueToMissingSystemClockSync:
        case AutoEnrollmentTypeChecker::CheckType::kNone:
          // The ownership check is only triggered if
          // `auto_enrollment_check_type_` indicates that an auto-enrollment
          // check should be done.
          NOTREACHED();
          break;
      }
      return;
    case ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken:
      LOG(WARNING) << "Device already owned, skipping auto-enrollment check.";
      UpdateState(AutoEnrollmentState::kNoEnrollment);
      return;
    case ash::DeviceSettingsService::OwnershipStatus::kOwnershipUnknown:
      LOG(ERROR) << "Ownership unknown, skipping auto-enrollment check.";
      UpdateState(AutoEnrollmentState::kNoEnrollment);
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
      g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->GetStateKeysBroker()
          ->RequestStateKeys(
              base::BindOnce(&AutoEnrollmentController::StartClientForFRE,
                             client_start_weak_factory_.GetWeakPtr()));
    } else {
      UpdateState(AutoEnrollmentState::kNoEnrollment);
    }
    return;
  }

  DeviceManagementService* service = InitializeAndGetDeviceManagementService();

  int power_initial =
      GetSanitizedArg(ash::switches::kEnterpriseEnrollmentInitialModulus);
  int power_limit =
      GetSanitizedArg(ash::switches::kEnterpriseEnrollmentModulusLimit);
  if (power_initial > power_limit) {
    LOG(ERROR) << "Initial auto-enrollment modulus is larger than the limit, "
                  "clamping to the limit.";
    power_initial = power_limit;
  }

  client_ = GetAutoEnrollmentClientFactory()->CreateForFRE(
      base::BindRepeating(&AutoEnrollmentController::UpdateState,
                          weak_ptr_factory_.GetWeakPtr()),
      service, g_browser_process->local_state(), shared_url_loader_factory_,
      state_keys.front(), power_initial, power_limit);

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
  // an AutoEnrollmentState::kConnectionError to show an error screen and not
  // proceeding with the auto-enrollment checks until
  // AutoEnrollmentController::Start() is called again by a network state
  // change or network selection.
  if (system_clock_sync_state_ == SystemClockSyncState::kSynchronized) {
    StartWithSystemClockSyncState();
  } else {
    UpdateState(AutoEnrollmentState::kConnectionError);
  }
}

void AutoEnrollmentController::StartClientForInitialEnrollment() {
  DeviceManagementService* service = InitializeAndGetDeviceManagementService();

  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  const absl::optional<base::StringPiece> serial_number =
      provider->GetMachineID();
  const absl::optional<base::StringPiece> rlz_brand_code =
      provider->GetMachineStatistic(ash::system::kRlzBrandCodeKey);
  // The Initial State Determination should not be started if the serial number
  // or brand code are missing. This is ensured in
  // `GetInitialStateDeterminationRequirement`.
  CHECK(serial_number);
  CHECK(!serial_number->empty());
  CHECK(rlz_brand_code);
  CHECK(!rlz_brand_code->empty());

  const auto plaintext_id = psm::ConstructRlweId();
  client_ = GetAutoEnrollmentClientFactory()->CreateForInitialEnrollment(
      base::BindRepeating(&AutoEnrollmentController::UpdateState,
                          weak_ptr_factory_.GetWeakPtr()),
      service, g_browser_process->local_state(), shared_url_loader_factory_,
      std::string(serial_number.value()), std::string(rlz_brand_code.value()),
      std::make_unique<psm::RlweDmserverClientImpl>(
          service, shared_url_loader_factory_, plaintext_id,
          psm_rlwe_client_factory_));

  LOG(WARNING) << "Starting auto-enrollment client for Initial Enrollment.";
  client_->Start();
}

void AutoEnrollmentController::UpdateState(AutoEnrollmentState new_state) {
  LOG(WARNING) << "New auto-enrollment state: "
               << AutoEnrollmentStateToString(new_state);
  state_ = new_state;

  switch (state_) {
    case AutoEnrollmentState::kIdle:
    case AutoEnrollmentState::kPending:
      break;
    case AutoEnrollmentState::kConnectionError:
    case AutoEnrollmentState::kServerError:
    case AutoEnrollmentState::kEnrollment:
    case AutoEnrollmentState::kNoEnrollment:
    case AutoEnrollmentState::kDisabled:
      // Stop the safeguard timer once a result comes in.
      safeguard_timer_.Stop();
      // Reset enrollment state fetcher to allow restarting.
      enrollment_state_fetcher_.reset();
      ReportTimeoutUMA(
          AutoEnrollmentControllerTimeoutReport::kTimeoutCancelled);
      break;
  }

  // Device disabling mode is relying on device state stored in install
  // attributes. In case that file is corrupted, this should prevent device
  // re-enabling.
  if (state_ == AutoEnrollmentState::kDisabled) {
    DeviceMode device_mode = ash::InstallAttributes::Get()->GetMode();
    if (device_mode == DeviceMode::DEVICE_MODE_PENDING ||
        device_mode == DeviceMode::DEVICE_MODE_NOT_SET) {
      ash::DeviceSettingsService::Get()->SetDeviceMode(
          DeviceMode::DEVICE_MODE_ENTERPRISE);
    }
  }

  if (state_ == AutoEnrollmentState::kNoEnrollment) {
    StartCleanupForcedReEnrollment();
  } else {
    progress_callbacks_.Notify(state_);
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
  DCHECK_EQ(AutoEnrollmentState::kNoEnrollment, state_);
  if (!service_is_ready) {
    LOG(ERROR) << "Failed waiting for cryptohome D-Bus service availability.";
    progress_callbacks_.Notify(state_);
    return;
  }

  user_data_auth::RemoveFirmwareManagementParametersRequest request;
  ash::InstallAttributesClient::Get()->RemoveFirmwareManagementParameters(
      request,
      base::BindOnce(
          &AutoEnrollmentController::OnFirmwareManagementParametersRemoved,
          weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::OnFirmwareManagementParametersRemoved(
    absl::optional<user_data_auth::RemoveFirmwareManagementParametersReply>
        reply) {
  if (!reply.has_value() ||
      reply->error() !=
          user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_NOT_SET) {
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
  DCHECK_EQ(AutoEnrollmentState::kNoEnrollment, state_);
  if (!service_is_ready) {
    LOG(ERROR)
        << "Failed waiting for session_manager D-Bus service availability.";
    progress_callbacks_.Notify(state_);
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

  progress_callbacks_.Notify(state_);
}

void AutoEnrollmentController::Timeout() {
  if (AutoEnrollmentTypeChecker::IsUnifiedStateDeterminationEnabled()) {
    // This can actually happen in some cases, for example when state key
    // generation is waiting for time sync or the server just doesn't reply and
    // keeps the connection open.
    LOG(ERROR) << "EnrollmentStateFetcher didn't complete within time limit.";
    UpdateState(AutoEnrollmentState::kConnectionError);
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
    UpdateState(AutoEnrollmentState::kNoEnrollment);
    ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeout);
  } else {
    // This can actually happen in some cases, for example when state key
    // generation is waiting for time sync or the server just doesn't reply and
    // keeps the connection open.
    LOG(ERROR) << "AutoEnrollmentClient didn't complete within time limit.";
    UpdateState(AutoEnrollmentState::kConnectionError);
    ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeoutFRE);
  }

  client_.reset();

  // Make sure to nuke pending `client_` start sequences.
  client_start_weak_factory_.InvalidateWeakPtrs();
}

AutoEnrollmentClient::Factory*
AutoEnrollmentController::GetAutoEnrollmentClientFactory() {
  static base::NoDestructor<AutoEnrollmentClientImpl::FactoryImpl>
      default_factory;
  if (testing_auto_enrollment_client_factory_) {
    return testing_auto_enrollment_client_factory_;
  }

  return default_factory.get();
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
