// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/auto_enrollment_controller.h"

#include "ash/components/tpm/install_attributes.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client_impl.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/fake_private_membership_rlwe_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client_impl.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_id_provider_impl.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_sync_observation.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/userdataauth/install_attributes_client.h"
#include "chromeos/system/statistics_provider.h"
#include "components/device_event_log/device_event_log.h"
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

namespace ash {
namespace {

// Maximum number of bits of the identifier hash to send during initial
// enrollment check.
constexpr int kInitialEnrollmentModulusPowerLimit = 6;

const int kMaxRequestStateKeysTries = 10;

// Maximum time to wait for the auto-enrollment check to reach a decision.
// Note that this encompasses all steps `AutoEnrollmentController` performs in
// order to determine if the device should be auto-enrolled.
// If `kSafeguardTimeout` after `Start()` has been called,
// `AutoEnrollmentController::state()` is still AUTO_ENROLLMENT_STATE_PENDING,
// the AutoEnrollmentController will switch to
// AUTO_ENROLLMENT_STATE_NO_ENROLLMENT or AUTO_ENROLLMENT_STATE_CONNECTION_ERROR
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
  if (!command_line->HasSwitch(switch_name))
    return 0;
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
  if (int_value > policy::AutoEnrollmentClient::kMaximumPower) {
    LOG(ERROR) << "Switch \"" << switch_name << "\" can't be greater than "
               << policy::AutoEnrollmentClient::kMaximumPower << ". Using "
               << policy::AutoEnrollmentClient::kMaximumPower << ".";
    return policy::AutoEnrollmentClient::kMaximumPower;
  }
  return int_value;
}

std::string AutoEnrollmentStateToString(policy::AutoEnrollmentState state) {
  switch (state) {
    case policy::AutoEnrollmentState::AUTO_ENROLLMENT_STATE_IDLE:
      return "Not started";
    case policy::AutoEnrollmentState::AUTO_ENROLLMENT_STATE_PENDING:
      return "Pending";
    case policy::AutoEnrollmentState::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
      return "Connection error";
    case policy::AutoEnrollmentState::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
      return "Server error";
    case policy::AutoEnrollmentState::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
      return "Trigger enrollment";
    case policy::AutoEnrollmentState::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
      return "No enrollment";
    case policy::AutoEnrollmentState::AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH:
      return "Zero-Touch enrollment";
    case policy::AutoEnrollmentState::AUTO_ENROLLMENT_STATE_DISABLED:
      return "Device disabled";
  }
}

// Schedules immediate initialization of the `DeviceManagementService` and
// returns it.
policy::DeviceManagementService* InitializeAndGetDeviceManagementService() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceManagementService* service =
      connector->device_management_service();
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
  kTimeoutCancelled = 0,
  kTimeoutFRE,
  kTimeout,
  kMaxValue = kTimeout,
};

void ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport report) {
  base::UmaHistogramEnumeration("Enterprise.AutoEnrollmentControllerTimeout",
                                report);
}

}  // namespace

// static
bool AutoEnrollmentController::ShouldUseFakePsmRlweClient() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnterpriseUseFakePsmRlweClientForTesting);
}

AutoEnrollmentController::AutoEnrollmentController() {
  // Create the PSM (private set membership) RLWE client factory depending on
  // whether switches::kEnterpriseUseFakePsmRlweClient is set.
  if (ShouldUseFakePsmRlweClient()) {
    psm_rlwe_client_factory_ = std::make_unique<
        policy::FakePrivateMembershipRlweClient::FactoryImpl>();
  } else {
    psm_rlwe_client_factory_ = std::make_unique<
        policy::PrivateMembershipRlweClientImpl::FactoryImpl>();
  }
}

AutoEnrollmentController::~AutoEnrollmentController() {}

void AutoEnrollmentController::Start() {
  switch (state_) {
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
      // Abort re-start if the check is still running.
      return;
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH:
    case policy::AUTO_ENROLLMENT_STATE_DISABLED:
      // Abort re-start when there's already a final decision.
      return;

    case policy::AUTO_ENROLLMENT_STATE_IDLE:
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
      // Continue (re-)start.
      break;
  }

  // If a client is being created or already existing, bail out.
  if (client_start_weak_factory_.HasWeakPtrs() || client_) {
    LOG(ERROR) << "Auto-enrollment client is already running.";
    return;
  }

  // Arm the belts-and-suspenders timer to avoid hangs.
  safeguard_timer_.Start(FROM_HERE, kSafeguardTimeout,
                         base::BindOnce(&AutoEnrollmentController::Timeout,
                                        weak_ptr_factory_.GetWeakPtr()));
  request_state_keys_tries_ = 0;

  // The system clock sync state is not known yet, and this
  // `AutoEnrollmentController` could wait for it if requested.
  system_clock_sync_state_ = SystemClockSyncState::kCanWaitForSync;
  StartWithSystemClockSyncState();
}

void AutoEnrollmentController::StartWithSystemClockSyncState() {
  auto_enrollment_check_type_ =
      policy::AutoEnrollmentTypeChecker::DetermineAutoEnrollmentCheckType(
          IsSystemClockSynchronized(system_clock_sync_state_));
  if (auto_enrollment_check_type_ ==
      policy::AutoEnrollmentTypeChecker::CheckType::kNone) {
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    return;
  }
  // If waiting for system clock synchronization has been triggered, wait until
  // it finishes (this function will be called again when a result is
  // available).
  if (system_clock_sync_state_ == SystemClockSyncState::kWaitingForSync)
    return;

  if (auto_enrollment_check_type_ ==
      policy::AutoEnrollmentTypeChecker::CheckType::
          kUnknownDueToMissingSystemClockSync) {
    DCHECK_EQ(system_clock_sync_state_, SystemClockSyncState::kCanWaitForSync);
    system_clock_sync_state_ = SystemClockSyncState::kWaitingForSync;

    // Set state before waiting for the system clock sync, because
    // `WaitForSystemClockSync` may invoke its callback synchronously if the
    // system clock sync status is already known.
    UpdateState(policy::AUTO_ENROLLMENT_STATE_PENDING);

    // Use `client_start_weak_factory_` so the callback is not invoked if
    // `Timeout` has been called in the meantime (after `kSafeguardTimeout`).
    system_clock_sync_observation_ =
        SystemClockSyncObservation::WaitForSystemClockSync(
            SystemClockClient::Get(), kSystemClockSyncWaitTimeout,
            base::BindOnce(&AutoEnrollmentController::OnSystemClockSyncResult,
                           client_start_weak_factory_.GetWeakPtr()));
    return;
  }

  // Start by checking if the device has already been owned.
  UpdateState(policy::AUTO_ENROLLMENT_STATE_PENDING);
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::BindOnce(&AutoEnrollmentController::OnOwnershipStatusCheckDone,
                     client_start_weak_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::Retry() {
  if (client_)
    client_->Retry();
  else
    Start();
}

base::CallbackListSubscription
AutoEnrollmentController::RegisterProgressCallback(
    const ProgressCallbackList::CallbackType& callback) {
  return progress_callbacks_.Add(callback);
}

void AutoEnrollmentController::SetAutoEnrollmentClientFactoryForTesting(
    policy::AutoEnrollmentClient::Factory* auto_enrollment_client_factory) {
  testing_auto_enrollment_client_factory_ = auto_enrollment_client_factory;
}

void AutoEnrollmentController::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  switch (status) {
    case DeviceSettingsService::OWNERSHIP_NONE:
      switch (auto_enrollment_check_type_) {
        case policy::AutoEnrollmentTypeChecker::CheckType::
            kForcedReEnrollmentExplicitlyRequired:
          // [[fallthrough]];
        case policy::AutoEnrollmentTypeChecker::CheckType::
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
        case policy::AutoEnrollmentTypeChecker::CheckType::
            kInitialStateDetermination:
          StartClientForInitialEnrollment();
          break;
        case policy::AutoEnrollmentTypeChecker::CheckType::
            kUnknownDueToMissingSystemClockSync:
          // [[fallthrough]];
        case policy::AutoEnrollmentTypeChecker::CheckType::kNone:
          // The ownership check is only triggered if
          // `auto_enrollment_check_type_` indicates that an auto-enrollment
          // check should be done.
          NOTREACHED();
          break;
      }
      return;
    case DeviceSettingsService::OWNERSHIP_TAKEN:
      LOG(WARNING) << "Device already owned, skipping auto-enrollment check.";
      UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
      return;
    case DeviceSettingsService::OWNERSHIP_UNKNOWN:
      LOG(ERROR) << "Ownership unknown, skipping auto-enrollment check.";
      UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
      return;
  }
}

void AutoEnrollmentController::StartClientForFRE(
    const std::vector<std::string>& state_keys) {
  if (state_keys.empty()) {
    LOG(ERROR) << "No state keys available.";
    if (auto_enrollment_check_type_ ==
        policy::AutoEnrollmentTypeChecker::CheckType::
            kForcedReEnrollmentExplicitlyRequired) {
      if (request_state_keys_tries_ >= kMaxRequestStateKeysTries) {
        if (safeguard_timer_.IsRunning())
          safeguard_timer_.Stop();
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
      UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    }
    return;
  }

  policy::DeviceManagementService* service =
      InitializeAndGetDeviceManagementService();

  int power_initial =
      GetSanitizedArg(switches::kEnterpriseEnrollmentInitialModulus);
  int power_limit =
      GetSanitizedArg(switches::kEnterpriseEnrollmentModulusLimit);
  if (power_initial > power_limit) {
    LOG(ERROR) << "Initial auto-enrollment modulus is larger than the limit, "
                  "clamping to the limit.";
    power_initial = power_limit;
  }

  client_ = GetAutoEnrollmentClientFactory()->CreateForFRE(
      base::BindRepeating(&AutoEnrollmentController::UpdateState,
                          weak_ptr_factory_.GetWeakPtr()),
      service, g_browser_process->local_state(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
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
  StartWithSystemClockSyncState();
}

void AutoEnrollmentController::StartClientForInitialEnrollment() {
  policy::DeviceManagementService* service =
      InitializeAndGetDeviceManagementService();

  // Initial Enrollment does not transfer any data in the initial exchange, and
  // supports uploading up to `kInitialEnrollmentModulusPowerLimit` bits of the
  // identifier hash.
  const int power_initial = 0;
  const int power_limit = kInitialEnrollmentModulusPowerLimit;

  system::StatisticsProvider* provider =
      system::StatisticsProvider::GetInstance();
  std::string serial_number = provider->GetEnterpriseMachineID();
  std::string rlz_brand_code;
  const bool rlz_brand_code_found =
      provider->GetMachineStatistic(system::kRlzBrandCodeKey, &rlz_brand_code);
  // The Initial State Determination should not be started if the serial number
  // or brand code are missing. This is ensured in
  // `GetInitialStateDeterminationRequirement`.
  CHECK(!serial_number.empty() && rlz_brand_code_found &&
        !rlz_brand_code.empty());

  client_ = GetAutoEnrollmentClientFactory()->CreateForInitialEnrollment(
      base::BindRepeating(&AutoEnrollmentController::UpdateState,
                          weak_ptr_factory_.GetWeakPtr()),
      service, g_browser_process->local_state(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      serial_number, rlz_brand_code, power_initial, power_limit,
      psm_rlwe_client_factory_.get(), &psm_rlwe_id_provider_);

  LOG(WARNING) << "Starting auto-enrollment client for Initial Enrollment.";
  client_->Start();
}

void AutoEnrollmentController::UpdateState(
    policy::AutoEnrollmentState new_state) {
  LOG(WARNING) << "New auto-enrollment state: "
               << AutoEnrollmentStateToString(new_state);
  state_ = new_state;

  // Stop the safeguard timer once a result comes in.
  switch (state_) {
    case policy::AUTO_ENROLLMENT_STATE_IDLE:
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
      break;
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH:
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_DISABLED:
      safeguard_timer_.Stop();
      ReportTimeoutUMA(
          AutoEnrollmentControllerTimeoutReport::kTimeoutCancelled);
      break;
  }

  // Device disabling mode is relying on device state stored in install
  // attributes. In case that file is corrupted, this should prevent device
  // re-enabling.
  if (state_ == policy::AUTO_ENROLLMENT_STATE_DISABLED) {
    policy::DeviceMode device_mode = InstallAttributes::Get()->GetMode();
    if (device_mode == policy::DeviceMode::DEVICE_MODE_PENDING ||
        device_mode == policy::DeviceMode::DEVICE_MODE_NOT_SET) {
      DeviceSettingsService::Get()->SetDeviceMode(
          policy::DeviceMode::DEVICE_MODE_ENTERPRISE);
    }
  }

  if (state_ == policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT) {
    StartCleanupForcedReEnrollment();
  } else {
    progress_callbacks_.Notify(state_);
  }
}

void AutoEnrollmentController::StartCleanupForcedReEnrollment() {
  // D-Bus services may not be available yet, so we call
  // WaitForServiceToBeAvailable. See https://crbug.com/841627.
  InstallAttributesClient::Get()->WaitForServiceToBeAvailable(base::BindOnce(
      &AutoEnrollmentController::StartRemoveFirmwareManagementParameters,
      weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::StartRemoveFirmwareManagementParameters(
    bool service_is_ready) {
  DCHECK_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
  if (!service_is_ready) {
    LOG(ERROR) << "Failed waiting for cryptohome D-Bus service availability.";
    progress_callbacks_.Notify(state_);
    return;
  }

  user_data_auth::RemoveFirmwareManagementParametersRequest request;
  InstallAttributesClient::Get()->RemoveFirmwareManagementParameters(
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
  SessionManagerClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&AutoEnrollmentController::StartClearForcedReEnrollmentVpd,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::StartClearForcedReEnrollmentVpd(
    bool service_is_ready) {
  DCHECK_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT, state_);
  if (!service_is_ready) {
    LOG(ERROR)
        << "Failed waiting for session_manager D-Bus service availability.";
    progress_callbacks_.Notify(state_);
    return;
  }

  SessionManagerClient::Get()->ClearForcedReEnrollmentVpd(
      base::BindOnce(&AutoEnrollmentController::OnForcedReEnrollmentVpdCleared,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AutoEnrollmentController::OnForcedReEnrollmentVpdCleared(bool reply) {
  if (!reply)
    LOG(ERROR) << "Failed to clear forced re-enrollment flags in RW VPD.";

  progress_callbacks_.Notify(state_);
}

void AutoEnrollmentController::Timeout() {
  // When tightening the FRE flows, as a cautionary measure (to prevent
  // interference with consumer devices) timeout was chosen to only enforce FRE
  // for EXPLICITLY_REQUIRED.
  // TODO(igorcov): Investigate the remaining causes of hitting timeout and
  // potentially either remove the timeout altogether or enforce FRE in the
  // REQUIRED case as well.
  if (client_start_weak_factory_.HasWeakPtrs() &&
      auto_enrollment_check_type_ !=
          policy::AutoEnrollmentTypeChecker::CheckType::
              kForcedReEnrollmentExplicitlyRequired) {
    // If the callbacks to check ownership status or state keys are still
    // pending, there's a bug in the code running on the device. No use in
    // retrying anything, need to fix that bug.
    LOG(ERROR) << "Failed to start auto-enrollment check, fix the code!";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
    ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeout);
  } else {
    // This can actually happen in some cases, for example when state key
    // generation is waiting for time sync or the server just doesn't reply and
    // keeps the connection open.
    LOG(ERROR) << "AutoEnrollmentClient didn't complete within time limit.";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
    ReportTimeoutUMA(AutoEnrollmentControllerTimeoutReport::kTimeoutFRE);
  }

  // Reset state.
  if (client_) {
    // Cancelling the `client_` allows it to determine whether
    // its protocol finished before login was complete.
    client_.release()->CancelAndDeleteSoon();
  }

  // Make sure to nuke pending `client_` start sequences.
  client_start_weak_factory_.InvalidateWeakPtrs();
}

policy::AutoEnrollmentClient::Factory*
AutoEnrollmentController::GetAutoEnrollmentClientFactory() {
  static base::NoDestructor<policy::AutoEnrollmentClientImpl::FactoryImpl>
      default_factory;
  if (testing_auto_enrollment_client_factory_)
    return testing_auto_enrollment_client_factory_;

  return default_factory.get();
}

}  // namespace ash
