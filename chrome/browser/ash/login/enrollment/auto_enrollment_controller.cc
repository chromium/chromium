// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/auto_enrollment_controller.h"

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client_impl.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/dbus/userdataauth/install_attributes_client.h"
#include "chromeos/system/factory_ping_embargo_check.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/install_attributes.h"
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

namespace chromeos {

namespace {

// Maximum number of bits of the identifer hash to send during initial
// enrollment check.
constexpr int kInitialEnrollmentModulusPowerLimit = 6;

// If the modulus requested by the server is higher or equal to
// `1<<kInitialEnrollmentModulusPowerOutdatedServer`, assume that the server
// does not know initial enrollment yet.
// This is currently set to `14`, the server was requesting `16` for FRE on
// 2018-05-25.
// TODO(pmarko): Remove this mechanism when the server version supporting
// Initial Enrollment has been in production for a while
// (https://crbug.com/846645).
const int kInitialEnrollmentModulusPowerOutdatedServer = 14;

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
constexpr base::TimeDelta kSafeguardTimeout = base::TimeDelta::FromSeconds(90);

// Maximum time to wait for time sync before forcing a decision on whether
// Initial Enrollment should be performed. This corresponds to at least seven
// TCP retransmissions attempts to the remote server used to update the system
// clock.
constexpr base::TimeDelta kSystemClockSyncWaitTimeout =
    base::TimeDelta::FromSeconds(45);

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

std::string FRERequirementToString(
    AutoEnrollmentController::FRERequirement requirement) {
  using FRERequirement = AutoEnrollmentController::FRERequirement;
  switch (requirement) {
    case FRERequirement::kRequired:
      return "Forced Re-Enrollment required.";
    case FRERequirement::kNotRequired:
      return "Forced Re-Enrollment disabled: first setup.";
    case FRERequirement::kExplicitlyRequired:
      return "Forced Re-Enrollment required: flag in VPD.";
    case FRERequirement::kExplicitlyNotRequired:
      return "Forced Re-Enrollment disabled: flag in VPD.";
  }

  NOTREACHED();
  return std::string();
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

// Returns true if this is an official build and the device has Chrome firmware.
bool IsGoogleBrandedChrome() {
  std::string firmware_type;
  bool is_chrome_branded =
      !system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kFirmwareTypeKey, &firmware_type) ||
      firmware_type != system::kFirmwareTypeValueNonchrome;
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  is_chrome_branded = false;
#endif
  return is_chrome_branded;
}

// Schedules immediate initialization of the `DeviceManagementService` and
// returns it.
policy::DeviceManagementService* InitializeAndGetDeviceManagementService() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceManagementService* service =
      connector->device_management_service();
  service->ScheduleInitialization(0);
  return service;
}

}  // namespace

// Supports waiting for the system clock to become synchronized.
class AutoEnrollmentController::SystemClockSyncWaiter
    : public chromeos::SystemClockClient::Observer {
 public:
  SystemClockSyncWaiter() : weak_ptr_factory_(this) {
    chromeos::SystemClockClient::Get()->AddObserver(this);
  }

  ~SystemClockSyncWaiter() override {
    chromeos::SystemClockClient::Get()->RemoveObserver(this);
  }

  // Waits for the system clock to be synchronized. If it already is
  // synchronized, `callback` will be called immediately. Otherwise, `callback`
  // will be called when the system clock has been synchronized, or after
  // `kSystemClockSyncWaitTimeout`.
  void WaitForSystemClockSync(SystemClockSyncCallback callback) {
    if (state_ == SystemClockSyncState::kSyncFailed ||
        state_ == SystemClockSyncState::kSynchronized) {
      std::move(callback).Run(state_);
      return;
    }

    system_clock_sync_callbacks_.push_back(std::move(callback));

    if (state_ == SystemClockSyncState::kWaitingForSync)
      return;
    state_ = SystemClockSyncState::kWaitingForSync;

    timeout_timer_.Start(FROM_HERE, kSystemClockSyncWaitTimeout,
                         base::BindOnce(&SystemClockSyncWaiter::OnTimeout,
                                        weak_ptr_factory_.GetWeakPtr()));

    chromeos::SystemClockClient::Get()->WaitForServiceToBeAvailable(
        base::BindOnce(&SystemClockSyncWaiter::OnGotSystemClockServiceAvailable,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Called when the system clock D-Bus service is available, or when it is
  // known that the system clock D-Bus service is not available.
  void OnGotSystemClockServiceAvailable(bool service_is_available) {
    if (!service_is_available) {
      SetStateAndRunCallbacks(SystemClockSyncState::kSyncFailed);
      return;
    }

    chromeos::SystemClockClient::Get()->GetLastSyncInfo(
        base::BindOnce(&SystemClockSyncWaiter::OnGotLastSyncInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Called on initial fetch of the system clock sync state, and when the system
  // clock sync state has changed.
  void OnGotLastSyncInfo(bool network_synchronized) {
    if (!network_synchronized)
      return;

    SetStateAndRunCallbacks(SystemClockSyncState::kSynchronized);
  }

  // Called when the time out has been reached.
  void OnTimeout() {
    SetStateAndRunCallbacks(SystemClockSyncState::kSyncFailed);
  }

  // Runs all callbacks in `system_clock_sync_callbacks_` and clears the vector.
  void SetStateAndRunCallbacks(SystemClockSyncState state) {
    state_ = state;
    timeout_timer_.AbandonAndStop();

    std::vector<SystemClockSyncCallback> callbacks;
    callbacks.swap(system_clock_sync_callbacks_);
    for (auto& callback : callbacks) {
      std::move(callback).Run(state_);
    }
  }

  // chromeos::SystemClockClient::Observer:
  void SystemClockUpdated() override {
    chromeos::SystemClockClient::Get()->GetLastSyncInfo(
        base::BindOnce(&SystemClockSyncWaiter::OnGotLastSyncInfo,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Current state of the system clock.
  SystemClockSyncState state_ = SystemClockSyncState::kCanWaitForSync;

  // Pending callbacks to be called when the system clock has been synchronized
  // or a timeout has been reached.
  std::vector<SystemClockSyncCallback> system_clock_sync_callbacks_;

  base::OneShotTimer timeout_timer_;

  base::WeakPtrFactory<SystemClockSyncWaiter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SystemClockSyncWaiter);
};

const char AutoEnrollmentController::kForcedReEnrollmentAlways[] = "always";
const char AutoEnrollmentController::kForcedReEnrollmentNever[] = "never";
const char AutoEnrollmentController::kForcedReEnrollmentOfficialBuild[] =
    "official";

const char AutoEnrollmentController::kInitialEnrollmentAlways[] = "always";
const char AutoEnrollmentController::kInitialEnrollmentNever[] = "never";
const char AutoEnrollmentController::kInitialEnrollmentOfficialBuild[] =
    "official";

const char AutoEnrollmentController::kEnablePsmAlways[] = "always";
const char AutoEnrollmentController::kEnablePsmNever[] = "never";

// static
bool AutoEnrollmentController::IsFREEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      switches::kEnterpriseEnableForcedReEnrollment);
  if (command_line_mode == kForcedReEnrollmentAlways)
    return true;

  if (command_line_mode.empty() ||
      command_line_mode == kForcedReEnrollmentOfficialBuild) {
    return IsGoogleBrandedChrome();
  }

  if (command_line_mode == kForcedReEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown Forced Re-Enrollment mode: " << command_line_mode
             << ".";
  return false;
}

// static
bool AutoEnrollmentController::IsInitialEnrollmentEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kEnterpriseEnableInitialEnrollment))
    return IsGoogleBrandedChrome();

  std::string command_line_mode = command_line->GetSwitchValueASCII(
      switches::kEnterpriseEnableInitialEnrollment);
  if (command_line_mode == kInitialEnrollmentAlways)
    return true;

  if (command_line_mode.empty() ||
      command_line_mode == kInitialEnrollmentOfficialBuild) {
    return IsGoogleBrandedChrome();
  }

  if (command_line_mode == kInitialEnrollmentNever)
    return false;

  LOG(FATAL) << "Unknown Initial Enrollment mode: " << command_line_mode << ".";
  return false;
}

// static
bool AutoEnrollmentController::IsPsmEnabled() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kEnterpriseEnablePsm))
    return true;  // Enabled by default.

  std::string command_line_mode =
      command_line->GetSwitchValueASCII(switches::kEnterpriseEnablePsm);
  if (command_line_mode.empty() || command_line_mode == kEnablePsmAlways)
    return true;

  if (command_line_mode == kEnablePsmNever) {
    return false;
  }

  LOG(FATAL) << "Unknown PSM enablement mode: " << command_line_mode << ".";
  return false;
}

// static
bool AutoEnrollmentController::IsEnabled() {
  return IsFREEnabled() || IsInitialEnrollmentEnabled();
}

// static
AutoEnrollmentController::FRERequirement
AutoEnrollmentController::GetFRERequirement() {
  std::string check_enrollment_value;
  system::StatisticsProvider* provider =
      system::StatisticsProvider::GetInstance();
  bool fre_flag_found = provider->GetMachineStatistic(
      system::kCheckEnrollmentKey, &check_enrollment_value);

  if (fre_flag_found) {
    if (check_enrollment_value == "0")
      return FRERequirement::kExplicitlyNotRequired;
    if (check_enrollment_value == "1")
      return FRERequirement::kExplicitlyRequired;
  }
  // Assume that the presence of the machine serial number means that VPD has
  // been read successfully. Don't trust a missing ActivateDate if VPD could not
  // be read successfully.
  bool vpd_read_successfully = !provider->GetEnterpriseMachineID().empty();
  if (vpd_read_successfully &&
      !provider->GetMachineStatistic(system::kActivateDateKey, nullptr)) {
    // The device has never been activated (enterprise enrolled or
    // consumer-owned) so doing a FRE check is not necessary.
    return FRERequirement::kNotRequired;
  }
  return FRERequirement::kRequired;
}

AutoEnrollmentController::AutoEnrollmentController()
    : system_clock_sync_waiter_(std::make_unique<SystemClockSyncWaiter>()) {}

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
  bool may_request_system_clock_sync = !system_clock_sync_wait_requested_;

  DetermineAutoEnrollmentCheckType();
  if (auto_enrollment_check_type_ == AutoEnrollmentCheckType::kNone) {
    if (may_request_system_clock_sync && system_clock_sync_wait_requested_) {
      // Set state before waiting for the system clock sync, because
      // `WaitForSystemClockSync` may invoke its callback synchronously if the
      // system clock sync status is already known.
      UpdateState(policy::AUTO_ENROLLMENT_STATE_PENDING);

      // Use `client_start_weak_factory_` so the callback is not invoked if
      // `Timeout` has been called in the meantime (after `kSafeguardTimeout`).
      system_clock_sync_waiter_->WaitForSystemClockSync(
          base::BindOnce(&AutoEnrollmentController::OnSystemClockSyncResult,
                         client_start_weak_factory_.GetWeakPtr()));
      return;
    }
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
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

AutoEnrollmentController::InitialStateDeterminationRequirement
AutoEnrollmentController::GetInitialStateDeterminationRequirement() {
  system::StatisticsProvider* provider =
      system::StatisticsProvider::GetInstance();
  system::FactoryPingEmbargoState embargo_state =
      system::GetEnterpriseManagementPingEmbargoState(provider);
  if (provider->GetEnterpriseMachineID().empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing serial number.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  std::string rlz_brand_code;
  const bool rlz_brand_code_found =
      provider->GetMachineStatistic(system::kRlzBrandCodeKey, &rlz_brand_code);
  if (!rlz_brand_code_found || rlz_brand_code.empty()) {
    LOG(WARNING)
        << "Skip Initial State Determination due to missing brand code.";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  if (system_clock_sync_state_ == SystemClockSyncState::kCanWaitForSync &&
      (embargo_state == system::FactoryPingEmbargoState::kInvalid ||
       embargo_state == system::FactoryPingEmbargoState::kNotPassed)) {
    // Wait for the system clock to become synchronized and check again.
    LOG(WARNING)
        << "Skip Initial State Determination due to out of sync clock.";
    system_clock_sync_wait_requested_ = true;
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  const char* system_clock_log_info =
      system_clock_sync_state_ == SystemClockSyncState::kSynchronized
          ? "system clock in sync"
          : "system clock sync failed";
  if (embargo_state == system::FactoryPingEmbargoState::kInvalid) {
    LOG(WARNING)
        << "Skip Initial State Determination due to invalid embargo date ("
        << system_clock_log_info << ").";
    return InitialStateDeterminationRequirement::kNotRequired;
  }
  if (embargo_state == system::FactoryPingEmbargoState::kNotPassed) {
    LOG(WARNING) << "Skip Initial State Determination because the device is in "
                    "the embargo period ("
                 << system_clock_log_info << ").";
    return InitialStateDeterminationRequirement::kNotRequired;
  }

  return InitialStateDeterminationRequirement::kRequired;
}

void AutoEnrollmentController::DetermineAutoEnrollmentCheckType() {
  // Skip everything if neither FRE nor Initial Enrollment are enabled.
  if (!IsEnabled()) {
    LOG(WARNING) << "Auto-enrollment disabled.";
    auto_enrollment_check_type_ = AutoEnrollmentCheckType::kNone;
    return;
  }

  // Skip everything if GAIA is disabled.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableGaiaServices)) {
    LOG(WARNING) << "Auto-enrollment disabled: command line (gaia).";
    auto_enrollment_check_type_ = AutoEnrollmentCheckType::kNone;
    return;
  }

  // Determine whether to do an FRE check or an initial state determination.
  // FRE has precedence since managed devices must go through an FRE check.
  fre_requirement_ = GetFRERequirement();
  LOG_DETERMINATION() << FRERequirementToString(fre_requirement_);

  if (ShouldDoFRECheck(command_line, fre_requirement_)) {
    // FRE has precedence over Initial Enrollment.
    LOG(WARNING) << "Proceeding with FRE check.";
    auto_enrollment_check_type_ = AutoEnrollmentCheckType::kForcedReEnrollment;
    return;
  }

  // The device is in consumer mode, check whether an initial state
  // determination is in order.
  if (ShouldDoInitialEnrollmentCheck()) {
    LOG(WARNING) << "Proceeding with Initial State Determination.";
    auto_enrollment_check_type_ =
        AutoEnrollmentCheckType::kInitialStateDetermination;
    return;
  }

  // Neither FRE nor initial state determination checks are needed.
  auto_enrollment_check_type_ = AutoEnrollmentCheckType::kNone;
}

// static
bool AutoEnrollmentController::ShouldDoFRECheck(
    base::CommandLine* command_line,
    FRERequirement fre_requirement) {
  // Skip FRE check if modulus configuration is not present.
  if (!command_line->HasSwitch(switches::kEnterpriseEnrollmentInitialModulus) &&
      !command_line->HasSwitch(switches::kEnterpriseEnrollmentModulusLimit)) {
    LOG(WARNING) << "FRE disabled through command line (config).";
    return false;
  }

  // Skip FRE check if it is not enabled by command-line switches.
  if (!IsFREEnabled()) {
    LOG(WARNING) << "FRE disabled.";
    return false;
  }

  // Skip FRE check if explicitly not required to check.
  if (fre_requirement == FRERequirement::kExplicitlyNotRequired) {
    LOG(WARNING) << "FRE disabled for device in consumer mode.";
    return false;
  }

  // Skip FRE check if it is not required according to the device state.
  if (fre_requirement == FRERequirement::kNotRequired)
    return false;

  return true;
}

// static
bool AutoEnrollmentController::ShouldDoInitialEnrollmentCheck() {
  // Skip Initial State Determination if it is not enabled according to
  // command-line flags.
  if (!IsInitialEnrollmentEnabled()) {
    LOG(WARNING) << "Initial Enrollment is disabled.";
    return false;
  }

  // Skip Initial State Determination if it is not required according to the
  // device state.
  if (GetInitialStateDeterminationRequirement() ==
      InitialStateDeterminationRequirement::kNotRequired) {
    // Warnings have been logged for all the reasons not to do the check.
    LOG_DETERMINATION() << "Initial State Determination is not required.";
    return false;
  }

  // Nothing has been logged, but the caller will log so this can stay as VLOG.
  LOG_DETERMINATION() << "Initial State Determination required.";
  return true;
}

void AutoEnrollmentController::OnOwnershipStatusCheckDone(
    DeviceSettingsService::OwnershipStatus status) {
  switch (status) {
    case DeviceSettingsService::OWNERSHIP_NONE:
      switch (auto_enrollment_check_type_) {
        case AutoEnrollmentCheckType::kForcedReEnrollment:
          ++request_state_keys_tries_;
          // For FRE, request state keys first.
          g_browser_process->platform_part()
              ->browser_policy_connector_chromeos()
              ->GetStateKeysBroker()
              ->RequestStateKeys(
                  base::BindOnce(&AutoEnrollmentController::StartClientForFRE,
                                 client_start_weak_factory_.GetWeakPtr()));
          break;
        case AutoEnrollmentCheckType::kInitialStateDetermination:
          StartClientForInitialEnrollment();
          break;
        case AutoEnrollmentCheckType::kNone:
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
    if (fre_requirement_ == FRERequirement::kExplicitlyRequired) {
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
          ->browser_policy_connector_chromeos()
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
    SystemClockSyncState system_clock_sync_state) {
  system_clock_sync_state_ = system_clock_sync_state;
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
      kInitialEnrollmentModulusPowerOutdatedServer);

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
      break;
  }

  // Device disabling mode is relying on device state stored in install
  // attributes. In case that file is corrupted, this should prevent device
  // re-enabling.
  if (state_ == policy::AUTO_ENROLLMENT_STATE_DISABLED) {
    policy::DeviceMode device_mode =
        chromeos::InstallAttributes::Get()->GetMode();
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
    base::Optional<user_data_auth::RemoveFirmwareManagementParametersReply>
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
  // for EXPLICTLY_REQUIRED.
  // TODO(igorcov): Investigate the remaining causes of hitting timeout and
  // potentially either remove the timeout altogether or enforce FRE in the
  // REQUIRED case as well.
  // TODO(mnissler): Add UMA to track results of auto-enrollment checks.
  if (client_start_weak_factory_.HasWeakPtrs() &&
      fre_requirement_ != FRERequirement::kExplicitlyRequired) {
    // If the callbacks to check ownership status or state keys are still
    // pending, there's a bug in the code running on the device. No use in
    // retrying anything, need to fix that bug.
    LOG(ERROR) << "Failed to start auto-enrollment check, fix the code!";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
  } else {
    // This can actually happen in some cases, for example when state key
    // generation is waiting for time sync or the server just doesn't reply and
    // keeps the connection open.
    LOG(ERROR) << "AutoEnrollmentClient didn't complete within time limit.";
    UpdateState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
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

}  // namespace chromeos
