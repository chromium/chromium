// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/session/arc_session_manager.h"

#include <string>
#include <utility>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_data_remover.h"
#include "ash/components/arc/session/arc_dlc_installer.h"
#include "ash/components/arc/session/arc_instance_mode.h"
#include "ash/components/arc/session/arc_management_transition.h"
#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/session/serial_number_util.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/arc_fast_app_reinstall_starter.h"
#include "chrome/browser/ash/app_list/arc/arc_pai_starter.h"
#include "chrome/browser/ash/app_list/arc/intent.h"
#include "chrome/browser/ash/arc/arc_demo_mode_delegate_impl.h"
#include "chrome/browser/ash/arc/arc_migration_guide_notification.h"
#include "chrome/browser/ash/arc/arc_mount_provider.h"
#include "chrome/browser/ash/arc/arc_optin_uma.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/arc_ui_availability_reporter.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/auth/arc_auth_service.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_default_negotiator.h"
#include "chrome/browser/ash/arc/optin/arc_terms_of_service_oobe_negotiator.h"
#include "chrome/browser/ash/arc/policy/arc_policy_util.h"
#include "chrome/browser/ash/arc/session/arc_provisioning_result.h"
#include "chrome/browser/ash/arc/session/arc_reven_hardware_checker.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/ash/diagnostics_dialog/diagnostics_dialog.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/memory/swap_configuration.h"
#include "components/account_id/account_id.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "ui/display/types/display_constants.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {

// Weak pointer.  This class is owned by ArcServiceLauncher.
ArcSessionManager* g_arc_session_manager = nullptr;

// Allows the session manager to skip creating UI in unit tests.
bool g_ui_enabled = true;

constexpr const char kArcSaltPath[] = "/var/lib/misc/arc_salt";

constexpr const char kArcPrepareHostGeneratedDirJobName[] =
    "arc_2dprepare_2dhost_2dgenerated_2ddir";

constexpr const char kArcvmInstallAndroidImageDlc[] =
    "arcvm_2dinstall_2dandroid_2dimage_2ddlc";

// Maximum amount of time we'll wait for ARC to finish booting up. Once this
// timeout expires, keep ARC running in case the user wants to file feedback,
// but present the UI to try again.
constexpr base::TimeDelta kArcSignInTimeout = base::Minutes(5);

// Updates UMA with user cancel only if error is not currently shown.
void MaybeUpdateOptInCancelUMA(const ArcSupportHost* support_host) {
  if (!support_host ||
      support_host->ui_page() == ArcSupportHost::UIPage::NO_PAGE ||
      support_host->ui_page() == ArcSupportHost::UIPage::ERROR) {
    return;
  }

  UpdateOptInCancelUMA(OptInCancelReason::USER_CANCEL);
}

// Returns true if launching the Play Store on OptIn succeeded is needed.
// Launch Play Store app, except for the following cases:
// * When Opt-in verification is disabled (for tests);
// * In case ARC is enabled from OOBE.
// * In Public Session mode, because Play Store will be hidden from users
//   and only apps configured by policy should be installed.
// * When ARC is managed, and user does not go through OOBE opt-in,
//   because the whole OptIn flow should happen as seamless as possible for
//   the user.
// Some tests require the Play Store to be shown and forces this using chromeos
// switch kArcForceShowPlayStoreApp.
bool ShouldLaunchPlayStoreApp(Profile* profile,
                              bool oobe_or_assistant_wizard_start) {
  if (!IsPlayStoreAvailable()) {
    return false;
  }

  if (oobe_or_assistant_wizard_start) {
    return false;
  }

  if (ShouldShowOptInForTesting()) {
    return true;
  }

  if (IsRobotOrOfflineDemoAccountMode()) {
    return false;
  }

  if (IsArcOptInVerificationDisabled()) {
    return false;
  }

  if (ShouldStartArcSilentlyForManagedProfile(profile)) {
    return false;
  }

  return true;
}

// Defines the conditions that require UI to present eventual error conditions
// to the end user.
//
// Don't show UI for MGS sessions in demo mode because the only one UI must be
// the demo app. In case of error the UI will be useless as well, because
// in typical use case there will be no one nearby the demo device, who can
// do some action to solve the problem be means of UI.
// All other managed sessions will be attended by a user and require an error
// UI.
bool ShouldUseErrorDialog() {
  if (!g_ui_enabled) {
    return false;
  }

  if (IsArcOptInVerificationDisabled()) {
    return false;
  }

  if (ash::DemoSession::IsDeviceInDemoMode()) {
    return false;
  }

  return true;
}

void ResetStabilityMetrics() {
  // TODO(shaochuan): Make this an event observable by StabilityMetricsManager
  // and eliminate this null check.
  auto* stability_metrics_manager = StabilityMetricsManager::Get();
  if (!stability_metrics_manager) {
    return;
  }
  stability_metrics_manager->ResetMetrics();
}

void SetArcEnabledStateMetric(bool enabled) {
  // TODO(shaochuan): Make this an event observable by StabilityMetricsManager
  // and eliminate this null check.
  auto* stability_metrics_manager = StabilityMetricsManager::Get();
  if (!stability_metrics_manager) {
    return;
  }
  stability_metrics_manager->SetArcEnabledState(enabled);
}

int GetSignInErrorCode(const arc::mojom::ArcSignInError* sign_in_error) {
  if (!sign_in_error) {
    return 0;
  }

#define IF_ERROR_RETURN_CODE(name, type)                          \
  if (sign_in_error->is_##name()) {                               \
    return static_cast<std::underlying_type_t<arc::mojom::type>>( \
        sign_in_error->get_##name());                             \
  }

  IF_ERROR_RETURN_CODE(cloud_provision_flow_error, CloudProvisionFlowError)
  IF_ERROR_RETURN_CODE(general_error, GeneralSignInError)
  IF_ERROR_RETURN_CODE(check_in_error, GMSCheckInError)
  IF_ERROR_RETURN_CODE(sign_in_error, GMSSignInError)
#undef IF_ERROR_RETURN_CODE

  LOG(ERROR) << "Unknown sign-in error "
             << std::underlying_type_t<arc::mojom::ArcSignInError::Tag>(
                    sign_in_error->which())
             << ".";

  return -1;
}

ArcSupportHost::Error GetCloudProvisionFlowError(
    mojom::CloudProvisionFlowError cloud_provision_flow_error) {
  switch (cloud_provision_flow_error) {
    case mojom::CloudProvisionFlowError::ERROR_ENROLLMENT_TOKEN_INVALID:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_ENROLLMENT_TOKEN_INVALID;

    case mojom::CloudProvisionFlowError::ERROR_DEVICE_QUOTA_EXCEEDED:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_DOMAIN_JOIN_FAIL_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_NETWORK_UNAVAILABLE:
      return ArcSupportHost::Error::SIGN_IN_CLOUD_PROVISION_FLOW_NETWORK_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_USER_CANCEL:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_INTERRUPTED_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_NO_ACCOUNT_IN_WORK_PROFILE:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_ACCOUNT_MISSING_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_NOT_READY:
    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_NOT_ALLOWLISTED:
    case mojom::CloudProvisionFlowError::ERROR_DPC_SUPPORT:
    case mojom::CloudProvisionFlowError::ERROR_ENTERPRISE_INVALID:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_PERMANENT_ERROR;

    case mojom::CloudProvisionFlowError::ERROR_ACCOUNT_OTHER:
    case mojom::CloudProvisionFlowError::ERROR_ADD_ACCOUNT_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_CHECKIN_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_INVALID_POLICY_STATE:
    case mojom::CloudProvisionFlowError::ERROR_INVALID_SETUP_ACTION:
    case mojom::CloudProvisionFlowError::ERROR_JSON:
    case mojom::CloudProvisionFlowError::ERROR_MANAGED_PROVISIONING_FAILED:
    case mojom::CloudProvisionFlowError::
        ERROR_OAUTH_TOKEN_AUTHENTICATOR_EXCEPTION:
    case mojom::CloudProvisionFlowError::ERROR_OAUTH_TOKEN_IO_EXCEPTION:
    case mojom::CloudProvisionFlowError::
        ERROR_OAUTH_TOKEN_OPERATION_CANCELED_EXCEPTION:
    case mojom::CloudProvisionFlowError::ERROR_OAUTH_TOKEN:
    case mojom::CloudProvisionFlowError::ERROR_OTHER:
    case mojom::CloudProvisionFlowError::ERROR_QUARANTINE:
    case mojom::CloudProvisionFlowError::ERROR_REMOVE_ACCOUNT_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_REQUEST_ANDROID_ID_FAILED:
    case mojom::CloudProvisionFlowError::ERROR_SERVER_TRANSIENT_ERROR:
    case mojom::CloudProvisionFlowError::ERROR_SERVER:
    case mojom::CloudProvisionFlowError::ERROR_TIMEOUT:
    default:
      return ArcSupportHost::Error::
          SIGN_IN_CLOUD_PROVISION_FLOW_TRANSIENT_ERROR;
  }
}

ArcSupportHost::Error GetSupportHostError(const ArcProvisioningResult& result) {
  if (result.gms_sign_in_error() ==
      mojom::GMSSignInError::GMS_SIGN_IN_NETWORK_ERROR) {
    return ArcSupportHost::Error::SIGN_IN_NETWORK_ERROR;
  }

  if (result.gms_sign_in_error() ==
      mojom::GMSSignInError::GMS_SIGN_IN_BAD_AUTHENTICATION) {
    return ArcSupportHost::Error::SIGN_IN_BAD_AUTHENTICATION_ERROR;
  }

  if (result.gms_sign_in_error()) {
    return ArcSupportHost::Error::SIGN_IN_GMS_SIGNIN_ERROR;
  }

  if (result.gms_check_in_error()) {
    return ArcSupportHost::Error::SIGN_IN_GMS_CHECKIN_ERROR;
  }

  if (result.cloud_provision_flow_error()) {
    return GetCloudProvisionFlowError(
        result.cloud_provision_flow_error().value());
  }

  if (result.general_error() ==
      mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR) {
    return ArcSupportHost::Error::SERVER_COMMUNICATION_ERROR;
  }

  if (result.general_error() ==
      mojom::GeneralSignInError::NO_NETWORK_CONNECTION) {
    return ArcSupportHost::Error::NETWORK_UNAVAILABLE_ERROR;
  }

  if (result.general_error() == mojom::GeneralSignInError::ARC_DISABLED) {
    return ArcSupportHost::Error::ANDROID_MANAGEMENT_REQUIRED_ERROR;
  }

  if (result.stop_reason() == ArcStopReason::LOW_DISK_SPACE) {
    return ArcSupportHost::Error::LOW_DISK_SPACE_ERROR;
  }

  return ArcSupportHost::Error::SIGN_IN_UNKNOWN_ERROR;
}

bool ShouldShowNetworkTests(const ArcProvisioningResult& result) {
  if (result.gms_sign_in_error() ==
          mojom::GMSSignInError::GMS_SIGN_IN_TIMEOUT ||
      result.gms_sign_in_error() ==
          mojom::GMSSignInError::GMS_SIGN_IN_SERVICE_UNAVAILABLE ||
      result.gms_sign_in_error() ==
          mojom::GMSSignInError::GMS_SIGN_IN_NETWORK_ERROR) {
    return true;
  }

  if (result.gms_check_in_error() ==
          mojom::GMSCheckInError::GMS_CHECK_IN_FAILED ||
      result.gms_check_in_error() ==
          mojom::GMSCheckInError::GMS_CHECK_IN_TIMEOUT) {
    return true;
  }

  if (result.cloud_provision_flow_error() ==
          mojom::CloudProvisionFlowError::ERROR_SERVER_TRANSIENT_ERROR ||
      result.cloud_provision_flow_error() ==
          mojom::CloudProvisionFlowError::ERROR_TIMEOUT ||
      result.cloud_provision_flow_error() ==
          mojom::CloudProvisionFlowError::ERROR_NETWORK_UNAVAILABLE ||
      result.cloud_provision_flow_error() ==
          mojom::CloudProvisionFlowError::ERROR_SERVER) {
    return true;
  }

  if (result.general_error() ==
          mojom::GeneralSignInError::GENERIC_PROVISIONING_TIMEOUT ||
      result.general_error() ==
          mojom::GeneralSignInError::NO_NETWORK_CONNECTION ||
      result.general_error() ==
          mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR) {
    return true;
  }
  return false;
}

ArcSessionManager::ExpansionResult ReadSaltInternal() {
  DCHECK(arc::IsArcVmEnabled());

  // For ARCVM, read |kArcSaltPath| if that exists.
  std::optional<std::string> salt =
      ReadSaltOnDisk(base::FilePath(kArcSaltPath));
  if (!salt) {
    return ArcSessionManager::ExpansionResult{{}, false};
  }
  return ArcSessionManager::ExpansionResult{std::move(*salt), true};
}

// Checks whether ARC DLCs needs to be installed/uninstalled. Currently,
// "houdini-rvc-dlc" is the only enabled DLC, so we only need to check
// for the presence of kEnableHoudiniDlc flag in the command line.
bool IsDlcRequired() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kEnableHoudiniDlc);
}

// Inform ArcMetricsServices about the starting time of ARC provisioning.
void ReportProvisioningStartTime(const base::TimeTicks& start_time,
                                 Profile* profile) {
  ArcMetricsService* metrics_service =
      ArcMetricsService::GetForBrowserContext(profile);
  // metrics_service might be null in unit tests.
  if (metrics_service) {
    auto account_type_suffix = GetHistogramNameByUserType("", profile);
    metrics_service->ReportProvisioningStartTime(start_time,
                                                 account_type_suffix);
  }
}

// Returns whether ARCVM /data migration is in progress and should be resumed.
bool ArcVmDataMigrationIsInProgress(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(kEnableArcVmDataMigration)) {
    return false;
  }
  return GetArcVmDataMigrationStatus(prefs) ==
         ArcVmDataMigrationStatus::kStarted;
}

// The result status of deferring ARC activation until user session start up
// task completion, used for UMA.
enum class DeferArcActivationResult {
  // Decided to defer, and the prediction succeeded, i.e. no activation
  // happens during user session start up.
  kDeferSucceeded = 0,

  // Decided to defer, but the prediction failed, i.e. an activation happens
  // during user session start up.
  kDeferFailed = 1,

  // Decided not to defer, and the prediction succeeded, i.e. an activation
  // happens during user session start up.
  kNotDeferSucceeded = 2,

  // Decided not to defer, and the prediction failed, i.e. no activation
  // happens during user session start up.
  kNotDeferFailed = 3,

  kMaxValue = kNotDeferFailed,
};

enum class DeferArcActivationCategory {
  // ARC activation is deferred until the user session start up task completion.
  kDeferred = 0,

  // ARC activation is not deferred, because the user is suspected to activate
  // ARC very soon.
  kNotDeferred = 1,

  // ARC is already activated, or the user session start up tasks are already
  // completed. Thus, it was out of scope to decide deferring.
  kNotTarget = 2,

  kMaxValue = kNotTarget,
};

// Using 1ms as minimum for common practice.
// The delay will be up to 20 seconds, because of the timer in the tracker.
// Using 25 secs just in case for additional buffer. The number of buckets are
// linearly extrapolated from the common one.
void UmaHistogramDeferActivationTimes(const std::string& name,
                                      base::TimeDelta elapsed) {
  base::UmaHistogramCustomTimes(name, elapsed, base::Milliseconds(1),
                                base::Seconds(25), 125);
}

}  // namespace

// This class is used to track statuses on OptIn flow. It is created in case ARC
// is activated, and it needs to OptIn. Once started OptInFlowResult::STARTED is
// recorded via UMA. If it finishes successfully OptInFlowResult::SUCCEEDED is
// recorded. Optional OptInFlowResult::SUCCEEDED_AFTER_RETRY is recorded in this
// case if an error occurred during OptIn flow, and user pressed Retry. In case
// the user cancels OptIn flow before it was completed then
// OptInFlowResult::CANCELED is recorded and if an error occurred optional
// OptInFlowResult::CANCELED_AFTER_ERROR. If a shutdown happens during the OptIn
// nothing is recorded, except initial OptInFlowResult::STARTED.
// OptInFlowResult::STARTED = OptInFlowResult::SUCCEEDED +
// OptInFlowResult::CANCELED + cases happened during the shutdown.
class ArcSessionManager::ScopedOptInFlowTracker {
 public:
  ScopedOptInFlowTracker() {
    UpdateOptInFlowResultUMA(OptInFlowResult::STARTED);
  }

  ScopedOptInFlowTracker(const ScopedOptInFlowTracker&) = delete;
  ScopedOptInFlowTracker& operator=(const ScopedOptInFlowTracker&) = delete;

  ~ScopedOptInFlowTracker() {
    if (shutdown_) {
      return;
    }

    UpdateOptInFlowResultUMA(success_ ? OptInFlowResult::SUCCEEDED
                                      : OptInFlowResult::CANCELED);
    if (error_) {
      UpdateOptInFlowResultUMA(success_
                                   ? OptInFlowResult::SUCCEEDED_AFTER_RETRY
                                   : OptInFlowResult::CANCELED_AFTER_ERROR);
    }
  }

  // Tracks error occurred during the OptIn flow.
  void TrackError() {
    DCHECK(!success_ && !shutdown_);
    error_ = true;
  }

  // Tracks that OptIn finished successfully.
  void TrackSuccess() {
    DCHECK(!success_ && !shutdown_);
    success_ = true;
  }

  // Tracks that OptIn was not completed before shutdown.
  void TrackShutdown() {
    DCHECK(!success_ && !shutdown_);
    shutdown_ = true;
  }

 private:
  bool error_ = false;
  bool success_ = false;
  bool shutdown_ = false;
};

ArcSessionManager::ArcSessionManager(
    std::unique_ptr<ArcSessionRunner> arc_session_runner,
    std::unique_ptr<AdbSideloadingAvailabilityDelegateImpl>
        adb_sideloading_availability_delegate)
    : arc_session_runner_(std::move(arc_session_runner)),
      adb_sideloading_availability_delegate_(
          std::move(adb_sideloading_availability_delegate)),
      android_management_checker_factory_(
          ArcRequirementChecker::GetDefaultAndroidManagementCheckerFactory()),
      attempt_user_exit_callback_(base::BindRepeating(chrome::AttemptUserExit)),
      attempt_restart_callback_(base::BindRepeating(chrome::AttemptRestart)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!g_arc_session_manager);
  g_arc_session_manager = this;
  arc_session_runner_->AddObserver(this);
  arc_session_runner_->SetDemoModeDelegate(
      std::make_unique<ArcDemoModeDelegateImpl>());
  if (ash::SessionManagerClient::Get()) {
    ash::SessionManagerClient::Get()->AddObserver(this);
  }
  ResetStabilityMetrics();
  ash::ConciergeClient::Get()->AddVmObserver(this);
  arc_dlc_installer_ = std::make_unique<ArcDlcInstaller>();
}

ArcSessionManager::~ArcSessionManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc_dlc_installer_.reset();

  ash::ConciergeClient::Get()->RemoveVmObserver(this);

  if (ash::SessionManagerClient::Get()) {
    ash::SessionManagerClient::Get()->RemoveObserver(this);
  }

  Shutdown();
  DCHECK(arc_session_runner_);
  arc_session_runner_->RemoveObserver(this);

  DCHECK_EQ(this, g_arc_session_manager);
  g_arc_session_manager = nullptr;
}

// static
ArcSessionManager* ArcSessionManager::Get() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return g_arc_session_manager;
}

// static
void ArcSessionManager::SetUiEnabledForTesting(bool enable) {
  g_ui_enabled = enable;
  ArcRequirementChecker::SetUiEnabledForTesting(enable);
}

// static
void ArcSessionManager::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
    bool enable) {
  ArcRequirementChecker::SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(
      enable);
}

// static
void ArcSessionManager::EnableCheckAndroidManagementForTesting(bool enable) {
  ArcRequirementChecker::EnableCheckAndroidManagementForTesting(enable);
}

void ArcSessionManager::OnSessionStopped(ArcStopReason reason,
                                         bool restarting) {
  if (restarting) {
    DCHECK_EQ(state_, State::ACTIVE);
    // If ARC is being restarted, here do nothing, and just wait for its
    // next run.
    return;
  }

  DCHECK(state_ == State::ACTIVE || state_ == State::STOPPING) << state_;
  state_ = State::STOPPED;

  if (arc_sign_in_timer_.IsRunning()) {
    OnProvisioningFinished(ArcProvisioningResult(reason));
  }

  for (auto& observer : observer_list_) {
    observer.OnArcSessionStopped(reason);
  }

  MaybeStartArcDataRemoval();

  if (!enable_requested_ && IsDlcRequired()) {
    arc_dlc_installer_->RequestDisable();
  }
}

void ArcSessionManager::OnSessionRestarting() {
  for (auto& observer : observer_list_) {
    observer.OnArcSessionRestarting();
  }
}

void ArcSessionManager::OnProvisioningFinished(
    const ArcProvisioningResult& result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the Mojo message to notify finishing the provisioning is already sent
  // from the container, it will be processed even after requesting to stop the
  // container. Ignore all |result|s arriving while ARC is disabled, in order to
  // avoid popping up an error message triggered below. This code intentionally
  // does not support the case of re-enabling.
  if (!enable_requested_) {
    LOG(WARNING) << "Provisioning result received after ARC was disabled. "
                 << "Ignoring result " << result << ".";
    return;
  }

  // Due asynchronous nature of stopping the ARC instance,
  // OnProvisioningFinished may arrive after setting the |State::STOPPED| state
  // and |State::Active| is not guaranteed to be set here.
  // prefs::kArcDataRemoveRequested also can be active for now.

  const bool provisioning_successful = result.is_success();
  if (provisioning_reported_) {
    // We don't expect success ArcProvisioningResult to be reported twice
    // or reported after an error.
    DCHECK(!provisioning_successful);
    // TODO(khmel): Consider changing LOG to NOTREACHED once we guaranty that
    // no double message can happen in production.
    LOG(WARNING) << "Provisioning result was already reported. Ignoring "
                 << "additional result " << result << ".";
    return;
  }
  provisioning_reported_ = true;
  if (scoped_opt_in_tracker_ && !provisioning_successful) {
    scoped_opt_in_tracker_->TrackError();
  }

  if (result.general_error() ==
      mojom::GeneralSignInError::CHROME_SERVER_COMMUNICATION_ERROR) {
    // TODO(poromov): Consider ARC PublicSession offline mode.
    // Currently ARC session will be exited below, while the main user session
    // will be kept alive without Android apps.
    if (IsRobotOrOfflineDemoAccountMode()) {
      VLOG(1) << "Robot account auth code fetching error";
    }

    // For backwards compatibility, use NETWORK_ERROR for
    // CHROME_SERVER_COMMUNICATION_ERROR case.
    UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
  } else if (!sign_in_start_time_.is_null()) {
    DCHECK(profile_);
    arc_sign_in_timer_.Stop();

    UpdateProvisioningTiming(base::TimeTicks::Now() - sign_in_start_time_,
                             provisioning_successful, profile_);
    UpdateProvisioningStatusUMA(GetProvisioningStatus(result), profile_);

    if (result.gms_sign_in_error()) {
      UpdateProvisioningSigninResultUMA(
          GetSigninErrorResult(result.gms_sign_in_error().value()), profile_);
    } else if (result.gms_check_in_error()) {
      UpdateProvisioningCheckinResultUMA(
          GetCheckinErrorResult(result.gms_check_in_error().value()), profile_);
    } else if (result.cloud_provision_flow_error()) {
      UpdateProvisioningDpcResultUMA(
          GetDpcErrorResult(result.cloud_provision_flow_error().value()),
          profile_);
    }

    if (!provisioning_successful) {
      UpdateOptInCancelUMA(OptInCancelReason::PROVISIONING_FAILED);
    }
  }

  PrefService* const prefs = profile_->GetPrefs();
  CHECK(prefs);
  if (provisioning_successful) {
    if (support_host_) {
      support_host_->Close();
    }

    if (scoped_opt_in_tracker_) {
      scoped_opt_in_tracker_->TrackSuccess();
      scoped_opt_in_tracker_.reset();
    }

    bool managed = policy_util::IsAccountManaged(profile_);
    if (managed) {
      UpdateProvisioningDpcResultUMA(ArcProvisioningDpcResult::kSuccess,
                                     profile_);
    } else {
      UpdateProvisioningSigninResultUMA(ArcProvisioningSigninResult::kSuccess,
                                        profile_);
    }
    UpdateProvisioningCheckinResultUMA(ArcProvisioningCheckinResult::kSuccess,
                                       profile_);

    prefs->SetBoolean(prefs::kArcIsManaged, managed);

    if (prefs->HasPrefPath(prefs::kArcSignedIn) &&
        prefs->GetBoolean(prefs::kArcSignedIn)) {
      return;
    }

    prefs->SetBoolean(prefs::kArcSignedIn, true);

    if (ShouldLaunchPlayStoreApp(
            profile_,
            prefs->GetBoolean(prefs::kArcProvisioningInitiatedFromOobe))) {
      playstore_launcher_ = std::make_unique<ArcAppLauncher>(
          profile_, kPlayStoreAppId,
          apps_util::MakeIntentForActivity(
              kPlayStoreActivity, kInitialStartParam, kCategoryLauncher),
          false /* deferred_launch_allowed */, display::kInvalidDisplayId,
          apps::LaunchSource::kFromChromeInternal);
    }

    prefs->ClearPref(prefs::kArcProvisioningInitiatedFromOobe);

    for (auto& observer : observer_list_) {
      observer.OnArcInitialStart();
    }
    return;
  }

  VLOG(1) << "ARC provisioning failed: " << result << ".";

  if (result.stop_reason()) {
    if (prefs->HasPrefPath(prefs::kArcSignedIn)) {
      prefs->SetBoolean(prefs::kArcSignedIn, false);
    }
    VLOG(1) << "ARC stopped unexpectedly";
    ShutdownSession();
  }

  if (result.cloud_provision_flow_error() ||
      // OVERALL_SIGN_IN_TIMEOUT might be an indication that ARC believes it is
      // fully setup, but Chrome does not.
      result.is_timedout() ||
      // Just to be safe, remove data if we don't know the cause.
      result.general_error() == mojom::GeneralSignInError::UNKNOWN_ERROR) {
    VLOG(1) << "ARC provisioning failed permanently. Removing user data";
    RequestArcDataRemoval();
  }

  std::optional<int> error_code;
  ArcSupportHost::Error support_error = GetSupportHostError(result);
  if (support_error == ArcSupportHost::Error::SIGN_IN_UNKNOWN_ERROR) {
    error_code = static_cast<std::underlying_type_t<ProvisioningStatus>>(
        GetProvisioningStatus(result));
  } else if (result.sign_in_error()) {
    error_code = GetSignInErrorCode(result.sign_in_error());
  }

  ShowArcSupportHostError({support_error, error_code} /* error_info */,
                          true /* should_show_send_feedback */,
                          ShouldShowNetworkTests(result));
}

bool ArcSessionManager::IsAllowed() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return profile_ != nullptr;
}

void ArcSessionManager::SetProfile(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!profile_);
  DCHECK(IsArcAllowedForProfile(profile));
  DCHECK(adb_sideloading_availability_delegate_);
  adb_sideloading_availability_delegate_->SetProfile(profile);
  profile_ = profile;
  // RequestEnable() requires |profile_| set, therefore shouldn't have been
  // called at this point.
  SetArcEnabledStateMetric(false);
  session_manager_observation_.Observe(session_manager::SessionManager::Get());
}

void ArcSessionManager::SetUserInfo() {
  DCHECK(profile_);
  DCHECK(arc_session_runner_);

  const AccountId account(multi_user_util::GetAccountIdFromProfile(profile_));
  const cryptohome::Identification cryptohome_id(account);
  const std::string user_id_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);

  std::string serialno = GetSerialNumber();
  arc_session_runner_->SetUserInfo(cryptohome_id, user_id_hash, serialno);
}

void ArcSessionManager::TrimVmMemory(TrimVmMemoryCallback callback,
                                     int page_limit) {
  arc_session_runner_->TrimVmMemory(std::move(callback), page_limit);
}

std::string ArcSessionManager::GetSerialNumber() const {
  DCHECK(profile_);
  DCHECK(arc_salt_on_disk_);

  const AccountId account(multi_user_util::GetAccountIdFromProfile(profile_));
  const std::string user_id_hash =
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_);

  std::string serialno;
  // ARC container doesn't need the serial number.
  if (arc::IsArcVmEnabled()) {
    const std::string chromeos_user =
        cryptohome::CreateAccountIdentifierFromAccountId(account).account_id();
    serialno = GetOrCreateSerialNumber(g_browser_process->local_state(),
                                       chromeos_user, *arc_salt_on_disk_);
  }
  return serialno;
}

void ArcSessionManager::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  DCHECK_EQ(state_, State::NOT_INITIALIZED);
  state_ = State::STOPPED;

  // If ExpandPropertyFilesAndReadSaltInternal() takes time to finish,
  // Initialize() may be called before it finishes. In that case,
  // SetUserInfo() is called in OnExpandPropertyFilesAndReadSalt().
  if (arc_salt_on_disk_) {
    VLOG(1) << "Calling SetUserInfo() in ArcSessionManager::Initialize";
    SetUserInfo();
  }

  // Create the support host at initialization. Note that, practically,
  // ARC support Chrome app is rarely used (only opt-in and re-auth flow).
  // So, it may be better to initialize it lazily.
  // TODO(hidehiko): Revisit to think about lazy initialization.
  if (ShouldUseErrorDialog()) {
    DCHECK(!support_host_);
    support_host_ = std::make_unique<ArcSupportHost>(profile_);
    support_host_->SetErrorDelegate(this);
  }
  auto* prefs = profile_->GetPrefs();
  const cryptohome::Identification cryptohome_id(
      multi_user_util::GetAccountIdFromProfile(profile_));
  data_remover_ = std::make_unique<ArcDataRemover>(prefs, cryptohome_id);

  if (ArcVmDataMigrationIsInProgress(prefs)) {
    const int auto_resume_count =
        prefs->GetInteger(prefs::kArcVmDataMigrationAutoResumeCount);
    if (auto_resume_count <= kArcVmDataMigrationMaxAutoResumeCount) {
      // |auto_resume_count| == kArcVmDataMigrationMaxAutoResumeCount means that
      // this is the first ARC session in which auto-resume is disabled.
      // Report to UMA and increment the pref value so that we can track the
      // number of users who hit the maximum number of auto-resumes.
      base::UmaHistogramExactLinear("Arc.VmDataMigration.AutoResumeCount",
                                    auto_resume_count,
                                    kArcVmDataMigrationMaxAutoResumeCount);
      prefs->SetInteger(prefs::kArcVmDataMigrationAutoResumeCount,
                        auto_resume_count + 1);
      if (auto_resume_count < kArcVmDataMigrationMaxAutoResumeCount) {
        VLOG(1) << "ARCVM /data migration is in progress. Restarting Chrome "
                   "session to resume the migration. Auto-resume count: "
                << auto_resume_count;
        attempt_restart_callback_.Run();
        return;
      }
    }
    LOG(WARNING) << "Skipping auto-resume of ARCVM /data migration, because it "
                    "has reached the maximum number of retries";
  }

  // Chrome may be shut down before completing ARC data removal.
  // For such a case, start removing the data now, if necessary.
  MaybeStartArcDataRemoval();
}

void ArcSessionManager::Shutdown() {
  VLOG(1) << "Shutting down session manager";
  enable_requested_ = false;
  ResetArcState();
  session_manager_observation_.Reset();
  arc_session_runner_->OnShutdown();
  data_remover_.reset();
  if (support_host_) {
    support_host_->SetErrorDelegate(nullptr);
    support_host_->Close();
    support_host_.reset();
  }
  pai_starter_.reset();
  fast_app_reinstall_starter_.reset();
  arc_ui_availability_reporter_.reset();
  profile_ = nullptr;
  state_ = State::NOT_INITIALIZED;
  if (scoped_opt_in_tracker_) {
    scoped_opt_in_tracker_->TrackShutdown();
    scoped_opt_in_tracker_.reset();
  }
  for (auto& observer : observer_list_) {
    observer.OnShutdown();
  }
}

void ArcSessionManager::ShutdownSession() {
  ResetArcState();
  switch (state_) {
    case State::NOT_INITIALIZED:
      // Ignore in NOT_INITIALIZED case. This is called in initial SetProfile
      // invocation.
      // TODO(hidehiko): Remove this along with the clean up.
    case State::STOPPED:
      // Currently, ARC is stopped. Do nothing.
    case State::REMOVING_DATA_DIR:
      // When data removing is done, |state_| will be set to STOPPED.
      // Do nothing here.
    case State::CHECKING_DATA_MIGRATION_NECESSITY:
      // Checking whether /data migration is necessary. |state_| will be set to
      // STOPPED when the check is done. Do nothing.
    case State::STOPPING:
      // Now ARC is stopping. Do nothing here.
      VLOG(1) << "Skipping session shutdown because state is: " << state_;
      break;
    case State::CHECKING_REQUIREMENTS:
    case State::READY:
      // We need to kill the mini-container that might be running here.
      arc_session_runner_->RequestStop();
      // While RequestStop is asynchronous, ArcSessionManager is agnostic to the
      // state of the mini-container, so we can set it's state_ to STOPPED
      // immediately.
      state_ = State::STOPPED;
      break;
    case State::ACTIVE:
      // Request to stop the ARC. |state_| will be set to STOPPED eventually.
      // Set state before requesting the runner to stop in order to prevent the
      // case when |OnSessionStopped| can be called inline and as result
      // |state_| might be changed.
      state_ = State::STOPPING;
      arc_session_runner_->RequestStop();
      break;
  }
}

void ArcSessionManager::ResetArcState() {
  pre_start_time_ = base::TimeTicks();
  start_time_ = base::TimeTicks();
  arc_sign_in_timer_.Stop();
  playstore_launcher_.reset();
  requirement_checker_.reset();
}

void ArcSessionManager::AddObserver(ArcSessionManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.AddObserver(observer);
  if (property_files_expansion_result_) {
    observer->OnPropertyFilesExpanded(*property_files_expansion_result_);
  }
}

void ArcSessionManager::RemoveObserver(ArcSessionManagerObserver* observer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  observer_list_.RemoveObserver(observer);
}

void ArcSessionManager::NotifyArcPlayStoreEnabledChanged(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  for (auto& observer : observer_list_) {
    observer.OnArcPlayStoreEnabledChanged(enabled);
  }
}

// This is the special method to support enterprise mojo API.
// TODO(hidehiko): Remove this.
void ArcSessionManager::StopAndEnableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  reenable_arc_ = true;
  StopArc();
}

void ArcSessionManager::OnArcSignInTimeout() {
  LOG(ERROR) << "Timed out waiting for first sign in.";
  OnProvisioningFinished(ArcProvisioningResult(ChromeProvisioningTimeout()));
}

void ArcSessionManager::CancelAuthCode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (state_ == State::NOT_INITIALIZED) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  // If ARC failed to boot normally, stop ARC. Otherwise, ARC is booting
  // normally and the instance should not be stopped.
  if (state_ != State::CHECKING_REQUIREMENTS &&
      (!support_host_ ||
       support_host_->ui_page() != ArcSupportHost::UIPage::ERROR)) {
    return;
  }

  MaybeUpdateOptInCancelUMA(support_host_.get());
  VLOG(1) << "Auth cancelled. Stopping ARC. state: " << state_;
  StopArc();
  SetArcPlayStoreEnabledForProfile(profile_, false);
}

void ArcSessionManager::RequestEnable() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  if (enable_requested_) {
    VLOG(1) << "ARC is already enabled. Do nothing.";
    return;
  }
  enable_requested_ = true;
  ash::ConfigureSwap(true);
  SetArcEnabledStateMetric(true);

  VLOG(1) << "ARC opt-in. Starting ARC session.";

  if (IsDlcRequired()) {
    arc_dlc_installer_->RequestEnable();
  }

  // |skipped_terms_of_service_negotiation_| is reset only in case terms are shown.
  // In all other cases it is conidered as skipped.
  skipped_terms_of_service_negotiation_ = true;
  RequestEnableImpl();
}

void ArcSessionManager::OnUserSessionStartUpTaskCompleted() {
  MaybeRecordFirstActivationDuringUserSessionStartUp(false);

  // Allow activation only when it already turns out ARC-On-Demand does not
  // delay the activation.
  if (is_activation_delayed_.has_value() && !is_activation_delayed_.value()) {
    AllowActivation(AllowActivationReason::kUserSessionStartUpTaskCompleted);
  }
}

void ArcSessionManager::AllowActivation(AllowActivationReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (user_session_start_up_task_timer_.has_value() &&
      reason != AllowActivationReason::kImmediateActivation) {
    base::TimeDelta elapsed =
        user_session_start_up_task_timer_->timer.Elapsed();
    if (user_session_start_up_task_timer_->deferred) {
      if (reason == AllowActivationReason::kUserSessionStartUpTaskCompleted) {
        base::UmaHistogramEnumeration(
            "Arc.DeferActivation.Result",
            DeferArcActivationResult::kDeferSucceeded);
        UmaHistogramDeferActivationTimes(
            "Arc.DeferActivation.Deferred.Success.ElapsedTime", elapsed);
      } else {
        base::UmaHistogramEnumeration("Arc.DeferActivation.Result",
                                      DeferArcActivationResult::kDeferFailed);
        base::UmaHistogramEnumeration(
            "Arc.DeferActivation.Deferred.Failure.Reason", reason);
        UmaHistogramDeferActivationTimes(
            "Arc.DeferActivation.Deferred.Failure.ElapsedTime", elapsed);
      }
    } else {
      if (reason == AllowActivationReason::kUserSessionStartUpTaskCompleted) {
        base::UmaHistogramEnumeration(
            "Arc.DeferActivation.Result",
            DeferArcActivationResult::kNotDeferFailed);
        UmaHistogramDeferActivationTimes(
            "Arc.DeferActivation.NotDeferred.Failure.ElapsedTime", elapsed);
      } else {
        base::UmaHistogramEnumeration(
            "Arc.DeferActivation.Result",
            DeferArcActivationResult::kNotDeferSucceeded);
        base::UmaHistogramEnumeration(
            "Arc.DeferActivation.NotDeferred.Success.Reason", reason);
        UmaHistogramDeferActivationTimes(
            "Arc.DeferActivation.NotDeferred.Success.ElapsedTime", elapsed);
      }
    }
    user_session_start_up_task_timer_.reset();
  }

  // Record the first activation is happening during the user session start up
  // to be referred whether or not to defer ARC for user session start up in
  // following user sessions.
  // ImmediateAction is ignored here. That happens when ARC gets READY and
  // it is decided not to defer ARC, and it should not be considered on deciding
  // whether or not to defer ARC in the following user sessions. Instead,
  // a following activation is recorded, e.g. user's explicit action to launch
  // an ARC app.
  // TODO(hidehiko): Consider excluding non user initiated actions, such as
  // forced by policy.
  if (reason != AllowActivationReason::kImmediateActivation) {
    MaybeRecordFirstActivationDuringUserSessionStartUp(
        reason != AllowActivationReason::kUserSessionStartUpTaskCompleted);
  }

  // First time that ARCVM is allowed in this user session.
  if (!activation_is_allowed_) {
    VLOG(1) << "ARCVM activation is allowed: " << static_cast<int>(reason);
  }

  activation_is_allowed_ = true;
  if (state_ == State::READY) {
    StartArcForRegularBoot();
  }
}

bool ArcSessionManager::IsPlaystoreLaunchRequestedForTesting() const {
  return playstore_launcher_.get();
}

void ArcSessionManager::OnVmStarted(
    const vm_tools::concierge::VmStartedSignal& vm_signal) {
  // When ARCVM starts, register GuestOsMountProvider for Play files.
  if (vm_signal.name() == kArcVmName) {
    if (arcvm_mount_provider_id_.has_value()) {
      // An old instance of ArcMountProvider can remain registered if the
      // previous ARC session did not finish normally and OnVmStopped() was not
      // called (due to concierge crash etc.). Unregister the old instance
      // before registering a new one to prevent multiple registration like
      // b/279378611.
      guest_os::GuestOsServiceFactory::GetForProfile(profile())
          ->MountProviderRegistry()
          ->Unregister(*arcvm_mount_provider_id_);
    }
    arcvm_mount_provider_id_ =
        std::optional<guest_os::GuestOsMountProviderRegistry::Id>(
            guest_os::GuestOsServiceFactory::GetForProfile(profile())
                ->MountProviderRegistry()
                ->Register(std::make_unique<ArcMountProvider>(
                    profile(), vm_signal.vm_info().cid())));
  }
}

void ArcSessionManager::OnVmStopped(
    const vm_tools::concierge::VmStoppedSignal& vm_signal) {
  // When ARCVM stops, unregister GuestOsMountProvider for Play files.
  if (vm_signal.name() == kArcVmName) {
    if (arcvm_mount_provider_id_.has_value()) {
      guest_os::GuestOsServiceFactory::GetForProfile(profile())
          ->MountProviderRegistry()
          ->Unregister(*arcvm_mount_provider_id_);
      arcvm_mount_provider_id_.reset();
    }
  }
}

void ArcSessionManager::RequestEnableImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(enable_requested_);
  DCHECK(state_ == State::STOPPED || state_ == State::STOPPING ||
         state_ == State::REMOVING_DATA_DIR ||
         state_ == State::CHECKING_DATA_MIGRATION_NECESSITY)
      << state_;

  if (state_ != State::STOPPED) {
    // If the previous invocation of ARC is still running (but currently being
    // stopped) or ARC data removal is in progress, postpone the enabling
    // procedure.
    reenable_arc_ = true;
    return;
  }

  PrefService* const prefs = profile_->GetPrefs();

  // |prefs::kArcProvisioningInitiatedFromOobe| is used to remember
  // |IsArcOobeOptInActive| or |IsArcOptInWizardForAssistantActive| state when
  // ARC start request was made initially. |IsArcOobeOptInActive| or
  // |IsArcOptInWizardForAssistantActive| will be changed by the time when
  // decision to auto-launch the Play Store would be made.
  // |IsArcOobeOptInActive| and |IsArcOptInWizardForAssistantActive| are not
  // preserved on Chrome restart also and in last case
  // |prefs::kArcProvisioningInitiatedFromOobe| is used to remember the state of
  // the initial request.
  // |prefs::kArcProvisioningInitiatedFromOobe| is reset when provisioning is
  // done or ARC is opted out.
  const bool opt_in_start = IsArcOobeOptInActive();
  const bool signed_in = IsArcProvisioned(profile_);
  if (opt_in_start) {
    prefs->SetBoolean(prefs::kArcProvisioningInitiatedFromOobe, true);
  }

  // If it is marked that sign in has been successfully done or if Play Store is
  // not available, then directly start ARC with skipping Play Store ToS.
  // For Kiosk mode, skip ToS because it is very likely that near the device
  // there will be no one who is eligible to accept them.
  // In Public Session mode ARC should be started silently without user
  // interaction. If opt-in verification is disabled, skip negotiation, too.
  // This is for testing purpose.
  const bool should_start_arc_without_user_interaction =
      ShouldArcAlwaysStart() || IsRobotOrOfflineDemoAccountMode() ||
      IsArcOptInVerificationDisabled();
  const bool skip_terms_of_service_negotiation =
      signed_in || should_start_arc_without_user_interaction;
  // When ARC is blocked because of filesystem compatibility, do not proceed
  // to starting ARC nor follow further state transitions.
  if (IsArcBlockedDueToIncompatibleFileSystem(profile_)) {
    // If the next step was the ToS negotiation, show a notification instead.
    // Otherwise, be silent now. Users are notified when clicking ARC app icons.
    if (!skip_terms_of_service_negotiation && g_ui_enabled) {
      arc::ShowArcMigrationGuideNotification(profile_);
    }
    return;
  }

  if (ArcVmDataMigrationIsInProgress(prefs)) {
    VLOG(1) << "Skipping request to enable ARC because ARCVM /data migration "
               "is in progress";
    // Auto-resume should be disabled only when |auto_resume_enabled| is larger
    // than kArcVmDataMigrationMaxAutoResumeCount. This is because the value is
    // incremented in Initialize() when it is smaller than or equal to
    // kArcVmDataMigrationMaxAutoResumeCount. See Initialize() for detail.
    const bool auto_resume_enabled =
        prefs->GetInteger(prefs::kArcVmDataMigrationAutoResumeCount) <=
        kArcVmDataMigrationMaxAutoResumeCount;
    for (auto& observer : observer_list_) {
      observer.OnArcSessionBlockedByArcVmDataMigration(auto_resume_enabled);
    }
    return;
  }

  // ARC might be re-enabled and in this case |arc_ui_availability_reporter_| is
  // already set.
  if (!arc_ui_availability_reporter_) {
    arc_ui_availability_reporter_ = std::make_unique<ArcUiAvailabilityReporter>(
        profile_,
        opt_in_start ? ArcUiAvailabilityReporter::Mode::kOobeProvisioning
        : signed_in  ? ArcUiAvailabilityReporter::Mode::kAlreadyProvisioned
                     : ArcUiAvailabilityReporter::Mode::kInSessionProvisioning);
  }

  if (!pai_starter_ && IsPlayStoreAvailable()) {
    pai_starter_ = ArcPaiStarter::CreateIfNeeded(profile_);
  }

  if (!fast_app_reinstall_starter_ && IsPlayStoreAvailable()) {
    fast_app_reinstall_starter_ = ArcFastAppReinstallStarter::CreateIfNeeded(
        profile_, profile_->GetPrefs());
  }

  if (should_start_arc_without_user_interaction) {
    AllowActivation(AllowActivationReason::kAlwaysStartIsEnabled);
  }

  if (skip_terms_of_service_negotiation) {
    state_ = State::READY;
    if (activation_is_allowed_) {
      StartArcForRegularBoot();
    } else {
      DCHECK(!activation_necessity_checker_);
      activation_necessity_checker_ =
          std::make_unique<ArcActivationNecessityChecker>(
              profile_, adb_sideloading_availability_delegate_.get());
      activation_necessity_checker_->Check(
          base::BindOnce(&ArcSessionManager::OnActivationNecessityChecked,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  MaybeStartTermsOfServiceNegotiation();
}

void ArcSessionManager::OnActivationNecessityChecked(bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(activation_necessity_checker_);

  base::UmaHistogramBoolean("Arc.ArcOnDemand.ActivationIsDelayed", !result);

  activation_necessity_checker_.reset();

  is_activation_delayed_ = !result;
  if (result) {
    bool should_defer =
        !activation_is_allowed_ && !session_manager::SessionManager::Get()
                                        ->IsUserSessionStartUpTaskCompleted();
    if (base::FeatureList::IsEnabled(
            kDeferArcActivationUntilUserSessionStartUpTaskCompletion)) {
      if (should_defer) {
        should_defer =
            ShouldDeferArcActivationUntilUserSessionStartUpTaskCompletion(
                profile_->GetPrefs());
        if (should_defer) {
          base::UmaHistogramEnumeration("Arc.DeferActivation.Category",
                                        DeferArcActivationCategory::kDeferred);
        } else {
          base::UmaHistogramEnumeration(
              "Arc.DeferActivation.Category",
              DeferArcActivationCategory::kNotDeferred);
        }
        user_session_start_up_task_timer_.emplace(
            UserSessionStartUpTaskTimer{base::ElapsedTimer(), should_defer});
      } else {
        base::UmaHistogramEnumeration("Arc.DeferActivation.Category",
                                      DeferArcActivationCategory::kNotTarget);
      }
    }
    if (should_defer) {
      // Wait for the user session start up task completion to prioritize
      // resources for them.
      VLOG(1) << "ARC activation is deferred until user sesssion start up "
              << "tasks are completed";
    } else {
      // In AllowActivation, actual ARC instance is going to be launched,
      // so call it here even if `activation_is_allowed_` checked above is
      // true, intentionally.
      AllowActivation(AllowActivationReason::kImmediateActivation);
    }
  } else {
    VLOG(1) << "Activation is not allowed yet. Not starting ARC for now.";
    for (auto& observer : observer_list_) {
      observer.OnArcStartDelayed();
    }
  }
}

void ArcSessionManager::RequestDisable(bool remove_arc_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  if (!enable_requested_) {
    VLOG(1) << "ARC is already disabled. "
            << "Killing an instance for login screen (if any).";
    arc_session_runner_->RequestStop();
    return;
  }

  VLOG(1) << "Disabling ARC.";

  skipped_terms_of_service_negotiation_ = false;
  enable_requested_ = false;
  SetArcEnabledStateMetric(false);
  scoped_opt_in_tracker_.reset();
  pai_starter_.reset();
  fast_app_reinstall_starter_.reset();
  arc_ui_availability_reporter_.reset();

  // Reset any pending request to re-enable ARC.
  reenable_arc_ = false;
  StopArc();

  if (remove_arc_data) {
    RequestArcDataRemoval();
  }

  ash::ConfigureSwap(false);
}

void ArcSessionManager::RequestDisable() {
  RequestDisable(false);
}

void ArcSessionManager::RequestDisableWithArcDataRemoval() {
  RequestDisable(true);
}

void ArcSessionManager::RequestArcDataRemoval() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(data_remover_);
  VLOG(1) << "Scheduling ARC data removal.";

  // TODO(hidehiko): DCHECK the previous state. This is called for four cases;
  // 1) Supporting managed user initial disabled case (Please see also
  //    ArcPlayStoreEnabledPreferenceHandler::Start() for details).
  // 2) Supporting enterprise triggered data removal.
  // 3) One called in OnProvisioningFinished().
  // 4) On request disabling.
  // After the state machine is fixed, 2) should be replaced by
  // RequestDisable() immediately followed by RequestEnable().
  // 3) and 4) are internal state transition. So, as for public interface, 1)
  // should be the only use case, and the |state_| should be limited to
  // STOPPED, then.
  // TODO(hidehiko): Think a way to get rid of 1), too.

  data_remover_->Schedule();
  auto* prefs = profile_->GetPrefs();
  prefs->SetInteger(prefs::kArcManagementTransition,
                    static_cast<int>(ArcManagementTransition::NO_TRANSITION));

  if (ArcVmDataMigrationIsInProgress(prefs)) {
    VLOG(1) << "Skipping ARC /data removal because ARCVM /data migration is "
               "in progress";
    return;
  }

  // To support 1) case above, maybe start data removal.
  if (state_ == State::STOPPED) {
    MaybeStartArcDataRemoval();
  }
}

void ArcSessionManager::MaybeStartTermsOfServiceNegotiation() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(!requirement_checker_);
  // In Public Session mode, Terms of Service negotiation should be skipped.
  // See also RequestEnableImpl().
  DCHECK(!IsRobotOrOfflineDemoAccountMode());
  // If opt-in verification is disabled, Terms of Service negotiation should
  // be skipped, too. See also RequestEnableImpl().
  DCHECK(!IsArcOptInVerificationDisabled());

  DCHECK_EQ(state_, State::STOPPED);
  state_ = State::CHECKING_REQUIREMENTS;

  // TODO(hidehiko): In kArcSignedIn = true case, this method should never
  // be called. Remove the check.
  // Conceptually, this is starting ToS negotiation, rather than opt-in flow.
  // Move to RequestEnabledImpl.
  if (!scoped_opt_in_tracker_ &&
      !profile_->GetPrefs()->GetBoolean(prefs::kArcSignedIn)) {
    scoped_opt_in_tracker_ = std::make_unique<ScopedOptInFlowTracker>();
  }

  bool is_terms_of_service_negotiation_needed = true;
  if (!IsArcTermsOfServiceNegotiationNeeded(profile_)) {
    if (IsArcStatsReportingEnabled() &&
        !profile_->GetPrefs()->GetBoolean(prefs::kArcTermsAccepted)) {
      // Don't enable stats reporting for users who are not shown the reporting
      // notice during ARC setup.
      profile_->GetPrefs()->SetBoolean(prefs::kArcSkippedReportingNotice, true);
    }
    is_terms_of_service_negotiation_needed = false;
  } else {
    DCHECK(arc_session_runner_);
    // Only set ARC signed in status here before calling StartMiniArc() since
    // we have valid profile available with cryptohome mounted.
    arc_session_runner_->set_arc_signed_in(IsArcProvisioned(profile_));
    // Start the mini-container (or mini-VM) here to save time starting the OS
    // if the user decides to opt-in. Unlike calling StartMiniArc() for ARCVM on
    // login screen, doing so on ToS screen is safe and desirable. The user has
    // already shown the intent to opt-in (or, if this is during OOBE, accepting
    // the ToS is mandatory), and the user's cryptohome has already been
    // mounted. vm_concierge is already running too. For those reasons, calling
    // StartMiniArc() for ARCVM here will actually make its perceived boot time
    // faster.
    StartMiniArc();
  }

  skipped_terms_of_service_negotiation_ =
      !is_terms_of_service_negotiation_needed;
  requirement_checker_ = std::make_unique<ArcRequirementChecker>(
      profile_, support_host_.get(), android_management_checker_factory_);
  requirement_checker_->AddObserver(this);
  requirement_checker_->StartRequirementChecks(
      is_terms_of_service_negotiation_needed,
      base::BindOnce(&ArcSessionManager::OnRequirementChecksDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::StartArcForTesting() {
  enable_requested_ = true;
  StartArc();
}

void ArcSessionManager::OnArcOptInManagementCheckStarted() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // State::STOPPED appears here in following scenario.
  // Initial provisioning finished with state
  // ProvisioningStatus::ArcStop or
  // ProvisioningStatus::CHROME_SERVER_COMMUNICATION_ERROR.
  // At this moment |prefs::kArcTermsAccepted| is set to true, once user
  // confirmed ToS prior to provisioning flow. Once user presses "Try Again"
  // button, OnRetryClicked calls this immediately.
  DCHECK(state_ == State::CHECKING_REQUIREMENTS || state_ == State::STOPPED)
      << state_;

  for (auto& observer : observer_list_) {
    observer.OnArcOptInManagementCheckStarted();
  }
}

void ArcSessionManager::OnRequirementChecksDone(
    ArcRequirementChecker::RequirementCheckResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::CHECKING_REQUIREMENTS);
  DCHECK(requirement_checker_);
  requirement_checker_.reset();

  switch (result) {
    case ArcRequirementChecker::RequirementCheckResult::kOk:
      VLOG(1) << "Starting ARC for first sign in.";
      for (auto& observer : observer_list_) {
        observer.OnArcOptInUserAction();
      }

      StartArc();
      break;
    case ArcRequirementChecker::RequirementCheckResult::
        kTermsOfServicesDeclined:
      // User does not accept the Terms of Service. Disable Google Play Store.
      MaybeUpdateOptInCancelUMA(support_host_.get());
      SetArcPlayStoreEnabledForProfile(profile_, false);
      break;
    case ArcRequirementChecker::RequirementCheckResult::
        kDisallowedByAndroidManagement:
      ShowArcSupportHostError(
          ArcSupportHost::ErrorInfo(
              ArcSupportHost::Error::ANDROID_MANAGEMENT_REQUIRED_ERROR),
          false /* should_show_send_feedback */,
          false /* should_show_run_network_tests */);
      UpdateOptInCancelUMA(OptInCancelReason::ANDROID_MANAGEMENT_REQUIRED);
      break;
    case ArcRequirementChecker::RequirementCheckResult::
        kAndroidManagementCheckError:
      ShowArcSupportHostError(
          ArcSupportHost::ErrorInfo(
              ArcSupportHost::Error::SERVER_COMMUNICATION_ERROR),
          true /* should_show_send_feedback */,
          true /* should_show_run_network_tests */);
      UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
      break;
  }
}

void ArcSessionManager::StartBackgroundRequirementChecks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::ACTIVE);
  DCHECK(!requirement_checker_);

  // We skip Android management check for Public Session mode, because they
  // don't use real google accounts.
  if (IsArcOptInVerificationDisabled() || IsRobotOrOfflineDemoAccountMode()) {
    return;
  }

  requirement_checker_ = std::make_unique<ArcRequirementChecker>(
      profile_, support_host_.get(), android_management_checker_factory_);
  requirement_checker_->StartBackgroundChecks(
      base::BindOnce(&ArcSessionManager::OnBackgroundRequirementChecksDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnBackgroundRequirementChecksDone(
    ArcRequirementChecker::BackgroundCheckResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(requirement_checker_);

  requirement_checker_.reset();

  switch (result) {
    case ArcRequirementChecker::BackgroundCheckResult::kNoActionRequired:
      break;
    case ArcRequirementChecker::BackgroundCheckResult::kArcShouldBeDisabled:
      SetArcPlayStoreEnabledForProfile(profile_, false);
      break;
    case ArcRequirementChecker::BackgroundCheckResult::kArcShouldBeRestarted:
      StopAndEnableArc();
      break;
  }
}

void ArcSessionManager::StartArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(state_ == State::STOPPED || state_ == State::CHECKING_REQUIREMENTS ||
         state_ == State::READY)
      << state_;
  state_ = State::ACTIVE;

  MaybeStartTimer();

  // ARC must be started only if no pending data removal request exists.
  DCHECK(profile_);
  DCHECK(!profile_->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  for (auto& observer : observer_list_) {
    observer.OnArcStarted();
  }

  start_time_ = base::TimeTicks::Now();
  // In case ARC started without mini-ARC |pre_start_time_| is not set.
  if (pre_start_time_.is_null()) {
    pre_start_time_ = start_time_;
  }
  provisioning_reported_ = false;

  std::string locale;
  std::string preferred_languages;
  if (IsArcLocaleSyncDisabled()) {
    // Use fixed locale and preferred languages for auto-tests.
    locale = "en-US";
    preferred_languages = "en-US,en";
    VLOG(1) << "Locale and preferred languages are fixed to " << locale << ","
            << preferred_languages << ".";
  } else {
    GetLocaleAndPreferredLanguages(profile_, &locale, &preferred_languages);
  }

  DCHECK(arc_session_runner_);
  arc_session_runner_->set_default_device_scale_factor(
      exo::GetDefaultDeviceScaleFactor());

  UpgradeParams params;

  const auto* demo_session = ash::DemoSession::Get();
  params.is_demo_session = demo_session && demo_session->started();
  if (params.is_demo_session) {
    DCHECK(demo_session->components()->resources_component_loaded());
    params.demo_session_apps_path =
        demo_session->components()->GetDemoAndroidAppsPath();
  }

  params.management_transition = GetManagementTransition(profile_);
  params.locale = locale;
  // Empty |preferred_languages| is converted to empty array.
  params.preferred_languages = base::SplitString(
      preferred_languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  DCHECK(user_manager->GetPrimaryUser());
  params.account_id =
      cryptohome::Identification(user_manager->GetPrimaryUser()->GetAccountId())
          .id();

  params.is_account_managed =
      profile_->GetProfilePolicyConnector()->IsManaged();

  arc_session_runner_->set_arc_signed_in(IsArcProvisioned(profile_));
  arc_session_runner_->RequestUpgrade(std::move(params));
}

void ArcSessionManager::StartArcForRegularBoot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::READY);
  DCHECK(activation_is_allowed_);

  VLOG(1) << "Starting ARC for a regular boot.";
  StartArc();
  // Check Android management in parallel.
  // Note: StartBackgroundRequirementManagementChecks() may call
  // OnBackgroundRequirementChecksDone() synchronously (or asynchronously). In
  // the callback, Google Play Store enabled preference can be set to false if
  // Android management is enabled, and it triggers RequestDisable() via
  // ArcPlayStoreEnabledPreferenceHandler.
  // Thus, StartArc() should be called so that disabling should work even
  // if synchronous call case.
  StartBackgroundRequirementChecks();
}

void ArcSessionManager::RequestStopOnLowDiskSpace() {
  arc_session_runner_->RequestStop();
}

void ArcSessionManager::StopArc() {
  // TODO(hidehiko): This STOPPED guard should be unnecessary. Remove it later.
  // |reenable_arc_| may be set in |StopAndEnableArc| in case enterprise
  // management state is lost.
  if (!reenable_arc_ && state_ != State::STOPPED) {
    profile_->GetPrefs()->SetBoolean(prefs::kArcSignedIn, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcPaiStarted, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcFastAppReinstallStarted, false);
    profile_->GetPrefs()->SetBoolean(prefs::kArcProvisioningInitiatedFromOobe,
                                     false);
    profile_->GetPrefs()->ClearPref(prefs::kArcIsManaged);
  }

  ShutdownSession();
  if (support_host_) {
    support_host_->Close();
  }
}

void ArcSessionManager::MaybeStartArcDataRemoval() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);

  // Data removal cannot run in parallel with ARC session.
  // LoginScreen instance does not use data directory, so removing should work.
  DCHECK_EQ(state_, State::STOPPED);

  state_ = State::REMOVING_DATA_DIR;
  data_remover_->Run(base::BindOnce(&ArcSessionManager::OnArcDataRemoved,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnArcDataRemoved(std::optional<bool> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::REMOVING_DATA_DIR);
  DCHECK(profile_);

  state_ = State::STOPPED;

  if (result.has_value()) {
    // Regardless of whether it is successfully done or not, notify observers.
    for (auto& observer : observer_list_) {
      observer.OnArcDataRemoved();
    }

    // Note: Currently, we may re-enable ARC even if data removal fails.
    // We may have to avoid it.
  }

  if (!base::FeatureList::IsEnabled(kEnableArcVmDataMigration) ||
      GetArcVmDataMigrationStatus(profile_->GetPrefs()) ==
          ArcVmDataMigrationStatus::kFinished) {
    // No need to check the necessity of ARCVM /data migration.
    MaybeReenableArc();
    return;
  }

  CheckArcVmDataMigrationNecessity(base::BindOnce(
      &ArcSessionManager::MaybeReenableArc, weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::CheckArcVmDataMigrationNecessity(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK_EQ(state_, State::STOPPED);
  state_ = State::CHECKING_DATA_MIGRATION_NECESSITY;

  DCHECK(profile_);
  DCHECK(!arc_vm_data_migration_necessity_checker_);
  arc_vm_data_migration_necessity_checker_ =
      std::make_unique<ArcVmDataMigrationNecessityChecker>(profile_);
  arc_vm_data_migration_necessity_checker_->Check(
      base::BindOnce(&ArcSessionManager::OnArcVmDataMigrationNecessityChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcSessionManager::OnArcVmDataMigrationNecessityChecked(
    base::OnceClosure callback,
    std::optional<bool> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK_EQ(state_, State::CHECKING_DATA_MIGRATION_NECESSITY);
  state_ = State::STOPPED;

  DCHECK(profile_);
  DCHECK(arc_vm_data_migration_necessity_checker_);
  arc_vm_data_migration_necessity_checker_.reset();

  // We assume that the migration is needed when |result| has no value, i.e.,
  // when ArcVmDataMigrationNecessityChecker could not determine the necessity.
  if (!result.value_or(true)) {
    VLOG(1) << "No need to perform ARCVM /data migration. Marking the migration"
            << " as finished";
    base::UmaHistogramEnumeration(
        GetHistogramNameByUserType(kArcVmDataMigrationFinishReasonHistogramName,
                                   profile_),
        ArcVmDataMigrationFinishReason::kNoDataToMigrate);
    SetArcVmDataMigrationStatus(profile_->GetPrefs(),
                                ArcVmDataMigrationStatus::kFinished);
  }
  std::move(callback).Run();
}

void ArcSessionManager::MaybeReenableArc() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(state_, State::STOPPED);
  DCHECK(arc_session_runner_);
  DCHECK(profile_);

  // Whether to use virtio-blk for /data depends on the status of ARCVM /data
  // migration, which can be updated between Initialize() and MaybeReenableArc()
  // by CheckArcVmDataMigrationNecessity(). Hence it should be set here.
  arc_session_runner_->set_use_virtio_blk_data(
      ShouldUseVirtioBlkData(profile_->GetPrefs()));

  if (!reenable_arc_) {
    // Re-enabling is not triggered. Do nothing.
    return;
  }
  DCHECK(enable_requested_);

  // Restart ARC anyway. Let the enterprise reporting instance decide whether
  // the ARC user data wipe is still required or not.
  reenable_arc_ = false;
  VLOG(1) << "Reenable ARC";
  RequestEnableImpl();
}

// Starts a timer to check if provisioning takes too long. The timer will not be
// set if this device was previously provisioned successfully.
void ArcSessionManager::MaybeStartTimer() {
  if (IsArcProvisioned(profile_)) {
    return;
  }

  VLOG(1) << "Setup provisioning timer";
  sign_in_start_time_ = base::TimeTicks::Now();
  ReportProvisioningStartTime(sign_in_start_time_, profile_);
  arc_sign_in_timer_.Start(
      FROM_HERE, kArcSignInTimeout,
      base::BindOnce(&ArcSessionManager::OnArcSignInTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::StartMiniArc() {
  DCHECK(arc_session_runner_);
  pre_start_time_ = base::TimeTicks::Now();
  arc_session_runner_->set_default_device_scale_factor(
      exo::GetDefaultDeviceScaleFactor());
  arc_session_runner_->RequestStartMiniInstance();
}

void ArcSessionManager::OnWindowClosed() {
  CancelAuthCode();

  // If network-related error occurred, collect UMA stats on user action.
  if (support_host_ && support_host_->GetShouldShowRunNetworkTests()) {
    UpdateOptInNetworkErrorActionUMA(
        arc::OptInNetworkErrorActionType::WINDOW_CLOSED);
  }
}

void ArcSessionManager::OnRetryClicked() {
  DCHECK(!g_ui_enabled || support_host_);
  DCHECK(!g_ui_enabled ||
         support_host_->ui_page() == ArcSupportHost::UIPage::ERROR);
  DCHECK(!requirement_checker_);

  UpdateOptInActionUMA(OptInActionType::RETRY);

  VLOG(1) << "Retry button clicked";

  if (state_ == State::ACTIVE) {
    // ERROR_WITH_FEEDBACK is set in OnSignInFailed(). In the case, stopping
    // ARC was postponed to contain its internal state into the report.
    // Here, on retry, stop it, then restart.
    if (support_host_) {
      support_host_->ShowArcLoading();
    }
    // In unit tests ShutdownSession may be executed inline and OnSessionStopped
    // is called before |reenable_arc_| is set.
    reenable_arc_ = true;
    ShutdownSession();
  } else {
    // Otherwise, we start ARC once it is stopped now. Usually ARC container is
    // left active after provisioning failure but in case
    // ProvisioningStatus::ARC_STOPPED and
    // ProvisioningStatus::CHROME_SERVER_COMMUNICATION_ERROR failures
    // container is stopped. At this point ToS is already accepted and
    // IsArcTermsOfServiceNegotiationNeeded returns true or ToS needs not to be
    // shown at all. However there is an exception when this does not happen in
    // case an error page is shown when re-opt-in right after opt-out (this is a
    // bug as it should not show an error). When the user click the retry
    // button on this error page, we may start ToS negotiation instead of
    // recreating the instance.
    // TODO(hidehiko): consider removing this case after fixing the bug.
    MaybeStartTermsOfServiceNegotiation();
  }

  // If network-related error occurred, collect UMA stats on user action.
  if (support_host_ && support_host_->GetShouldShowRunNetworkTests()) {
    UpdateOptInNetworkErrorActionUMA(arc::OptInNetworkErrorActionType::RETRY);
  }
}

void ArcSessionManager::OnErrorPageShown(bool network_tests_shown) {
  if (network_tests_shown) {
    UpdateOptInNetworkErrorActionUMA(
        arc::OptInNetworkErrorActionType::ERROR_SHOWN);
  }
}

void ArcSessionManager::OnSendFeedbackClicked() {
  DCHECK(support_host_);
  chrome::OpenFeedbackDialog(nullptr, feedback::kFeedbackSourceArcApp);

  // If network-related error occurred, collect UMA stats on user action.
  if (support_host_->GetShouldShowRunNetworkTests()) {
    UpdateOptInNetworkErrorActionUMA(
        arc::OptInNetworkErrorActionType::SEND_FEEDBACK);
  }
}

void ArcSessionManager::OnRunNetworkTestsClicked() {
  DCHECK(support_host_);
  ash::DiagnosticsDialog::ShowDialog(
      ash::DiagnosticsDialog::DiagnosticsPage::kConnectivity,
      support_host_->GetNativeWindow());

  // Network-related error occurred so collect UMA stats on user action.
  UpdateOptInNetworkErrorActionUMA(
      arc::OptInNetworkErrorActionType::CHECK_NETWORK);
}

void ArcSessionManager::SetArcSessionRunnerForTesting(
    std::unique_ptr<ArcSessionRunner> arc_session_runner) {
  DCHECK(arc_session_runner);
  DCHECK(arc_session_runner_);
  arc_session_runner_->RemoveObserver(this);
  arc_session_runner_ = std::move(arc_session_runner);
  arc_session_runner_->AddObserver(this);
}

ArcSessionRunner* ArcSessionManager::GetArcSessionRunnerForTesting() {
  return arc_session_runner_.get();
}

void ArcSessionManager::SetAttemptUserExitCallbackForTesting(
    const base::RepeatingClosure& callback) {
  DCHECK(!callback.is_null());
  attempt_user_exit_callback_ = callback;
}

void ArcSessionManager::SetAttemptRestartCallbackForTesting(
    const base::RepeatingClosure& callback) {
  DCHECK(!callback.is_null());
  attempt_restart_callback_ = callback;
}

void ArcSessionManager::ShowArcSupportHostError(
    ArcSupportHost::ErrorInfo error_info,
    bool should_show_send_feedback,
    bool should_show_run_network_tests) {
  if (support_host_) {
    support_host_->ShowError(error_info, should_show_send_feedback,
                             should_show_run_network_tests);
  }
  for (auto& observer : observer_list_) {
    observer.OnArcErrorShowRequested(error_info);
  }
}

void ArcSessionManager::EmitLoginPromptVisibleCalled() {
  // Since 'login-prompt-visible' Upstart signal starts all Upstart jobs the
  // instance may depend on such as cras, EmitLoginPromptVisibleCalled() is the
  // safe place to start a mini instance.
  if (!IsArcAvailable()) {
    return;
  }

  if (IsArcVmEnabled()) {
    // For ARCVM, don't try to start ARCVM on login screen.
    // Calling StartMiniArc() on login screen for ARCVM does more harm than
    // good. First, the ARCVM boot sequence started by StartMiniArc() stops
    // relatively early in ArcVmClientAdapter which waits for vm_concierge to
    // start (note that vm_concierge does not run on login screen these days.)
    // Because of this, crosvm for ARCVM won't start until the user signs into
    // their user session. Second, after the sign-in, the rest of the mini-ARCVM
    // startup sequence is executed regardless of whether the user has opted
    // into ARC. For opt-out users(*), ARCVM will eventually be stopped, but the
    // stop request may be issued after mini-VM is started. This is a complete
    // waste of resources and may also cause page caches evictions making Chrome
    // UI less responsive.
    // (*) This includes Kiosk mode. See b/197510998 for more info.
    VLOG(1) << "Starting ARCVM on login screen is not supported.";
    return;
  }
  if (!ShouldArcStartManually()) {
    StartMiniArc();
  }
}

void ArcSessionManager::ExpandPropertyFilesAndReadSalt() {
  VLOG(1) << "Started expanding *.prop files";

  // For ARCVM, generate <dest_path>/{combined.prop,fstab}. For ARC, generate
  // <dest_path>/{default,build,vendor_build}.prop.
  const bool is_arcvm = arc::IsArcVmEnabled();

  std::deque<JobDesc> jobs = {
      JobDesc{kArcPrepareHostGeneratedDirJobName,
              UpstartOperation::JOB_STOP_AND_START,
              {std::string("IS_ARCVM=") + (is_arcvm ? "1" : "0")}},
  };

  if (arc::IsArcVmDlcEnabled()) {
    // Check if the Reven device is compatible for ARC.
    hardware_checker_->IsRevenDeviceCompatibleForArc(
        base::BindOnce(&ArcSessionManager::OnEnableArcOnReven,
                       weak_ptr_factory_.GetWeakPtr(), std::move(jobs)));
  } else {
    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(&ArcSessionManager::OnExpandPropertyFiles,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ArcSessionManager::OnEnableArcOnReven(std::deque<JobDesc> jobs,
                                           bool is_compatible) {
  if (is_compatible) {
    VLOG(1) << "Reven device is compatible for ARC. Adding and starting the "
               "Android DLC install job.";
    jobs.emplace_front(JobDesc{kArcvmInstallAndroidImageDlc,
                               UpstartOperation::JOB_STOP_AND_START,
                               {}});
    ConfigureUpstartJobs(
        std::move(jobs),
        base::BindOnce(&ArcSessionManager::OnExpandPropertyFiles,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    VLOG(1) << "Reven device is not compatible for ARC.";
    OnExpandPropertyFilesAndReadSalt(
        ArcSessionManager::ExpansionResult{{}, false});
  }
}

void ArcSessionManager::OnExpandPropertyFiles(bool result) {
  if (!result) {
    LOG(ERROR) << "Failed to expand property files";
    OnExpandPropertyFilesAndReadSalt(
        ArcSessionManager::ExpansionResult{{}, false});
    return;
  }

  if (!arc::IsArcVmEnabled()) {
    OnExpandPropertyFilesAndReadSalt(
        ArcSessionManager::ExpansionResult{{}, true});
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadSaltInternal),
      base::BindOnce(&ArcSessionManager::OnExpandPropertyFilesAndReadSalt,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcSessionManager::OnExpandPropertyFilesAndReadSalt(
    ExpansionResult result) {
  // ExpandPropertyFilesAndReadSalt() should be called only once.
  DCHECK(!property_files_expansion_result_);

  arc_salt_on_disk_ = result.first;
  property_files_expansion_result_ = result.second;

  // See the comment in Initialize().
  if (profile_) {
    VLOG(1) << "Calling SetUserInfo() in "
            << "ArcSessionManager::OnExpandPropertyFilesAndReadSalt";
    SetUserInfo();
  }

  if (result.second) {
    DCHECK(arc_session_runner_);
    arc_session_runner_->set_arc_signed_in(IsArcProvisioned(profile_));
    arc_session_runner_->ResumeRunner();
  }
  for (auto& observer : observer_list_) {
    observer.OnPropertyFilesExpanded(*property_files_expansion_result_);
  }
}

void ArcSessionManager::StopMiniArcIfNecessary() {
  // This method should only be called before login.
  DCHECK(!profile_);
  DCHECK(arc_session_runner_);
  pre_start_time_ = base::TimeTicks();
  VLOG(1) << "Stopping mini-ARC instance (if any)";
  arc_session_runner_->RequestStop();
}

void ArcSessionManager::MaybeRecordFirstActivationDuringUserSessionStartUp(
    bool value) {
  if (is_first_activation_during_user_session_start_up_recorded_) {
    return;
  }
  is_first_activation_during_user_session_start_up_recorded_ = true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kLoginUser)) {
    // On browser restart, we don't record the user session start up,
    // because the start up process is different.
    // Theoretically, this is not a pure user login start up, so out of
    // the scope.
    // Practically, start up tasks are considered to be completed
    // quickly as a workaround of the current architecture (b/328339021),
    // so the recording is not reliable.
    return;
  }

  CHECK(profile_);
  RecordFirstActivationDuringUserSessionStartUp(profile_->GetPrefs(), value);
}

std::ostream& operator<<(std::ostream& os,
                         const ArcSessionManager::State& state) {
#define MAP_STATE(name)                \
  case ArcSessionManager::State::name: \
    return os << #name

  switch (state) {
    MAP_STATE(NOT_INITIALIZED);
    MAP_STATE(STOPPED);
    MAP_STATE(CHECKING_REQUIREMENTS);
    MAP_STATE(CHECKING_DATA_MIGRATION_NECESSITY);
    MAP_STATE(REMOVING_DATA_DIR);
    MAP_STATE(READY);
    MAP_STATE(ACTIVE);
    MAP_STATE(STOPPING);
  }

#undef MAP_STATE

  // Some compilers report an error even if all values of an enum-class are
  // covered exhaustively in a switch statement.
  NOTREACHED_IN_MIGRATION() << "Invalid value " << static_cast<int>(state);
  return os;
}

}  // namespace arc
