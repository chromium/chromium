// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_SESSION_MANAGER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_SESSION_MANAGER_H_

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>

#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_session_runner.h"
#include "ash/components/arc/session/arc_stop_reason.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/arc/arc_support_host.h"
#include "chrome/browser/ash/arc/policy/arc_android_management_checker.h"
#include "chrome/browser/ash/arc/session/adb_sideloading_availability_delegate_impl.h"
#include "chrome/browser/ash/arc/session/arc_activation_necessity_checker.h"
#include "chrome/browser/ash/arc/session/arc_app_id_provider_impl.h"
#include "chrome/browser/ash/arc/session/arc_requirement_checker.h"
#include "chrome/browser/ash/arc/session/arc_reven_hardware_checker.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ash/arc/session/arc_vm_data_migration_necessity_checker.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/browser/ash/policy/arc/android_management_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class ArcAppLauncher;
class Profile;

namespace arc {

// The file exists only when ARC container is in use.
constexpr const char kGeneratedBuildPropertyFilePath[] =
    "/run/arc/host_generated/build.prop";

// The file exists only when ARCVM is in use.
constexpr const char kGeneratedCombinedPropertyFilePathVm[] =
    "/run/arcvm/host_generated/combined.prop";

// Maximum number of auto-resumes for ARCVM /data migration. When this number of
// auto-resumes have been already attempted but the migration has not finished,
// ARC is blocked and the user needs to manually trigger the resume by clicking
// a notification.
constexpr int kArcVmDataMigrationMaxAutoResumeCount = 3;

class ArcDataRemover;
class ArcDlcInstaller;
class ArcFastAppReinstallStarter;
class ArcPaiStarter;
class ArcProvisioningResult;
class ArcUiAvailabilityReporter;
class ArcRevenHardwareChecker;

enum class ProvisioningStatus;
enum class ArcStopReason;

// This class is responsible for handing stages of ARC life-cycle.
class ArcSessionManager : public ArcSessionRunner::Observer,
                          public ArcSupportHost::ErrorDelegate,
                          public ash::SessionManagerClient::Observer,
                          public ash::ConciergeClient::VmObserver,
                          public ArcRequirementChecker::Observer,
                          public session_manager::SessionManagerObserver {
 public:
  // Represents each State of ARC session.
  // NOT_INITIALIZED: represents the state that the Profile is not yet ready
  //   so that this service is not yet initialized, or Chrome is being shut
  //   down so that this is destroyed.
  // STOPPED: ARC session is not running, or being terminated.
  // CHECKING_REQUIREMENTS: Checking requirements before starting ARC.
  //   First, negotiates Google Play Store "Terms of Service" with a user. There
  //   are several ways for the negotiation, including opt-in flow, which shows
  //   "Terms of Service" page on ARC support app, and OOBE flow, which shows
  //   "Terms of Service" page as a part of Chrome OOBE flow. If user does not
  //   accept the Terms of Service, disables Google Play Store, which triggers
  //   RequestDisable() and the state will be set to STOPPED, then.
  //   Second, checks Android management status. Note that the status is checked
  //   for each ARC session starting, but this is the state only for the first
  //   boot case (= opt-in case). The second time and later the management check
  //   is running in parallel with ARC session starting, and in such a case,
  //   State is ACTIVE, instead.
  // REMOVING_DATA_DIR: When ARC is disabled, the data directory is removed.
  //   While removing is processed, ARC cannot be started. This is the state.
  // CHECKING_DATA_MIGRATION_NECESSITY: When ARC /data migration is enabled but
  //   not started yet, we need to check whether the migration is necessary by
  //   inspecting the content of /data. ARC cannot be started while the check is
  //   being performed, which is indicated by this state.
  // READY: ARC is ready to run, but not running yet. This state is skipped on
  //   the first boot case.
  // ACTIVE: ARC is running.
  // STOPPING: ARC is being shut down.
  //
  // State transition should be as follows:
  //
  // NOT_INITIALIZED -> STOPPED: when the primary Profile gets ready.
  // ...(any)... -> NOT_INITIALIZED: when the Chrome is being shutdown.
  // ...(any)... -> STOPPED: on error.
  //
  // In the first boot case:
  //   STOPPED -> CHECKING_REQUIREMENTS: On request to enable.
  //   CHECKING_REQUIREMENTS -> ACTIVE: when a user accepts "Terms Of Service"
  //     and the auth token is successfully fetched.
  //
  // In the second (or later) boot case:
  //   STOPPED -> READY: when arc.enabled preference is checked that it is true.
  //     Practically, this is when the primary Profile gets ready.
  //   READY -> ACTIVE: when activation is allowed.
  //
  // In the disabling case:
  //   CHECKING_REQUIREMENTS -> STOPPED
  //   READY -> STOPPED
  //   ACTIVE -> STOPPING -> (maybe REMOVING_DATA_DIR ->) STOPPED
  //   STOPPING: Eventually change the state to STOPPED. Do nothing
  //     immediately.
  //   REMOVING_DATA_DIR: Eventually state will become STOPPED. Do nothing
  //     immediately.
  //   CHECKING_DATA_MIGRATION_NECESSITY: Eventually state will become STOPPED.
  //     Do nothing immediately.
  //
  // TODO(hidehiko): Fix the state machine, and update the comment including
  // relationship with |enable_requested_|.
  enum class State {
    NOT_INITIALIZED,
    STOPPED,
    CHECKING_REQUIREMENTS,
    REMOVING_DATA_DIR,
    CHECKING_DATA_MIGRATION_NECESSITY,
    READY,
    ACTIVE,
    STOPPING,
  };

  using ExpansionResult = std::pair<std::string /* salt on disk */,
                                    bool /* expansion successful */>;

  ArcSessionManager(std::unique_ptr<ArcSessionRunner> arc_session_runner,
                    std::unique_ptr<AdbSideloadingAvailabilityDelegateImpl>
                        adb_sideloading_availability_delegate);

  ArcSessionManager(const ArcSessionManager&) = delete;
  ArcSessionManager& operator=(const ArcSessionManager&) = delete;

  ~ArcSessionManager() override;

  static ArcSessionManager* Get();

  static void SetUiEnabledForTesting(bool enabled);
  static void SetArcTermsOfServiceOobeNegotiatorEnabledForTesting(bool enabled);
  static void EnableCheckAndroidManagementForTesting(bool enable);

  // Returns true if ARC is allowed to run for the current session.
  // TODO(hidehiko): The name is very close to IsArcAllowedForProfile(), but
  // has different meaning. Clean this up.
  bool IsAllowed() const;

  // Start expanding the property files. Note that these property files are
  // needed to start the mini instance. This function also tries to read
  // /var/lib/misc/arc_salt when ARCVM is enabled.
  void ExpandPropertyFilesAndReadSalt();

  // Initializes ArcSessionManager. Before this runs, Profile must be set
  // via SetProfile().
  void Initialize();

  // Set the device scale factor used to start the arc. This must be called
  // before staring mini-ARC.
  void SetDefaultDeviceScaleFactor(float default_device_scale_factor);

  void Shutdown();

  // Sets the |profile|, and sets up Profile related fields in this instance.
  // IsArcAllowedForProfile() must return true for the given |profile|.
  void SetProfile(Profile* profile);
  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }

  State state() const { return state_; }

  // Adds or removes observers.
  void AddObserver(ArcSessionManagerObserver* observer);
  void RemoveObserver(ArcSessionManagerObserver* observer);

  // Notifies observers that Google Play Store enabled preference is changed.
  // Note: ArcPlayStoreEnabledPreferenceHandler has the main responsibility to
  // notify the event. However, due to life time, it is difficult for non-ARC
  // services to subscribe the handler instance directly. Instead, they can
  // subscribe to ArcSessionManager, and ArcSessionManager proxies the event.
  void NotifyArcPlayStoreEnabledChanged(bool enabled);

  // Called from ARC support platform app when user cancels signing.
  void CancelAuthCode();

  // Requests to enable ARC session. This starts ARC instance, or maybe starts
  // Terms Of Service negotiation if they haven't been accepted yet.
  // If it is already requested to enable, no-op.
  // Currently, enabled/disabled is tied to whether Google Play Store is
  // enabled or disabled. Please see also TODO of
  // SetArcPlayStoreEnabledForProfile().
  void RequestEnable();

  enum class AllowActivationReason {
    // Activated when ARCVM is ready to be launched.
    kImmediateActivation = 0,

    // User session start up tasks are completed, so deferred ARC activation
    // is done.
    kUserSessionStartUpTaskCompleted = 1,

    // AlwaysStart option is set, so forced to launch ARC.
    kAlwaysStartIsEnabled = 2,

    // Policy enforces to start ARC.
    kForcedByPolicy = 3,

    // User has taken an action to launch ARC app.
    kUserLaunchAction = 4,

    // User flipped the flag to enable ARC in the system.
    kUserEnableAction = 5,

    // ARC app is being restored.
    kRestoreApps = 6,

    kMaxValue = kRestoreApps,
  };

  // Allows changing the state from READY to ACTIVE. If the state is already
  // READY, calling this method changes the state to ACTIVE.
  void AllowActivation(AllowActivationReason reason);

  // Requests to disable ARC session. This stops ARC instance, or quits Terms
  // Of Service negotiation if it is the middle of the process (e.g. closing
  // UI for manual negotiation if it is shown). This does not remove user ARC
  // data.
  // If it is already requested to disable, no-op.
  void RequestDisable();

  // Requests to disable ARC session and remove ARC data.
  // If it is already requested to disable, no-op.
  void RequestDisableWithArcDataRemoval();

  // Requests to remove the ARC data.
  // If ARC is stopped, triggers to remove the data. Otherwise, queues to
  // remove the data after ARC stops.
  // A log statement with the removal reason must be added prior to calling
  // this.
  void RequestArcDataRemoval();

  // Stops ARC instance without removing user ARC data.
  // Unlike RequestDisable(), this doesn't clear user ARC prefs, and ARC is not
  // supposed to restart within the same user session.
  // NOTE: This method should be used only for the purpose of stopping ARC
  //       under low disk space.
  // TODO(b/236325019): Remove this once ArcSessionManager officially supports
  //       a method to stop ARC without clearing user ARC prefs, or when we
  //       remove ArcDiskSpaceMonitor after Storage Balloon is ready.
  void RequestStopOnLowDiskSpace();

  // ArcSupportHost:::ErrorDelegate:
  void OnWindowClosed() override;
  void OnRetryClicked() override;
  void OnSendFeedbackClicked() override;
  void OnRunNetworkTestsClicked() override;
  void OnErrorPageShown(bool network_tests_shown) override;

  // StopArc(), then restart. Between them data clear may happens.
  // This is a special method to support enterprise device lost case.
  // This can be called only when ARC is running.
  void StopAndEnableArc();

  ArcSupportHost* support_host() { return support_host_.get(); }

  // On provisioning completion (regardless of whether successfully done or
  // not), this is called with its status. On success, is_success() of
  // |result| returns true, otherwise ArcSignInResult can be retrieved from
  // get() if sign-in result came from ARC or stop_reason()
  // will indicate that ARC stopped prematurely and provisioning could
  // not finish successfully. is_timedout() indicates that operation timed
  // out.
  void OnProvisioningFinished(const ArcProvisioningResult& result);

  // A helper function that calls ArcSessionRunner's SetUserInfo.
  void SetUserInfo();

  // Trims VM's memory by moving it to zram.
  // When the operation is done |callback| is called.
  // If nonzero, |page_limit| defines the max number of pages to reclaim.
  using TrimVmMemoryCallback = ArcSessionRunner::TrimVmMemoryCallback;
  void TrimVmMemory(TrimVmMemoryCallback callback, int page_limit);

  // Returns the time when ARC was pre-started (mini-ARC start), or a null time
  // if ARC has not been pre-started yet.
  base::TimeTicks pre_start_time() const { return pre_start_time_; }

  // Returns the time when ARC was about to start, or a null time if ARC has
  // not been started yet.
  base::TimeTicks start_time() const { return start_time_; }

  // Returns the time when the sign in process started, or a null time if
  // signing in didn't happen during this session.
  base::TimeTicks sign_in_start_time() const { return sign_in_start_time_; }

  // Returns true if ARC requested to start.
  bool enable_requested() const { return enable_requested_; }

  // Returns PAI starter that is used to start Play Auto Install flow. It is
  // available only on initial start.
  ArcPaiStarter* pai_starter() { return pai_starter_.get(); }

  // Returns Fast App Reinstall starter that is used to start Play Fast App
  // Reinstall flow. It is available only on initial start.
  ArcFastAppReinstallStarter* fast_app_resintall_starter() {
    return fast_app_reinstall_starter_.get();
  }

  // Returns true if the current ARC run has started with skipping user ToS
  // negotiation, because the user had accepted already or policy does not
  // require ToS acceptance. Returns false in other cases, including one when
  // ARC is not currently running.
  bool skipped_terms_of_service_negotiation() const {
    return skipped_terms_of_service_negotiation_;
  }
  void set_skipped_terms_of_service_negotiation_for_testing(
      bool skipped_terms_of_service_negotiation) {
    skipped_terms_of_service_negotiation_ =
        skipped_terms_of_service_negotiation;
  }

  // Injectors for testing.
  void SetArcSessionRunnerForTesting(
      std::unique_ptr<ArcSessionRunner> arc_session_runner);
  ArcSessionRunner* GetArcSessionRunnerForTesting();
  void SetAttemptUserExitCallbackForTesting(
      const base::RepeatingClosure& callback);
  void SetAttemptRestartCallbackForTesting(
      const base::RepeatingClosure& callback);
  void SetAndroidManagementCheckerFactoryForTesting(
      ArcRequirementChecker::AndroidManagementCheckerFactory
          android_management_checker_factory) {
    android_management_checker_factory_ = android_management_checker_factory;
  }

  // Returns whether the Play Store app is requested to be launched by this
  // class. Should be used only for tests.
  bool IsPlaystoreLaunchRequestedForTesting() const;

  // Invoking StartArc() only for testing, e.g., to emulate accepting Terms of
  // Service then passing Android management check successfully.
  void StartArcForTesting();

  // Invokes functions as if requirement checks are completed for testing.
  void EmulateRequirementCheckCompletionForTesting() {
    DCHECK(requirement_checker_);
    requirement_checker_->EmulateRequirementCheckCompletionForTesting();
  }

  // Invokes OnExpandPropertyFilesAndReadSalt as if the expansion is done.
  void OnExpandPropertyFilesAndReadSaltForTesting(bool result) {
    OnExpandPropertyFilesAndReadSalt(ExpansionResult{{}, result});
  }

  void reset_property_files_expansion_result() {
    property_files_expansion_result_.reset();
  }

  // ash::ConciergeClient::VmObserver overrides.
  void OnVmStarted(
      const vm_tools::concierge::VmStartedSignal& vm_signal) override;
  void OnVmStopped(
      const vm_tools::concierge::VmStoppedSignal& vm_signal) override;

  // session_manager::SessionManagerObserver overrides.
  void OnUserSessionStartUpTaskCompleted() override;

  // Getter for |serialno|.
  std::string GetSerialNumber() const;

  // Stops mini-ARC instance. This should only be called before login.
  void StopMiniArcIfNecessary();

  // Returns whether ARC activation is delayed by ARC on Demand
  bool IsActivationDelayed() const {
    return is_activation_delayed_.value_or(false);
  }

  // The unit test will use a mock hardware checker for testing.
  void SetHardwareCheckerForTesting(
      std::unique_ptr<ArcRevenHardwareChecker> hardware_checker) {
    hardware_checker_ = std::move(hardware_checker);
  }

 private:
  // Reports statuses of OptIn flow to UMA.
  class ScopedOptInFlowTracker;

  // Handles the completion of the hardware compatibility check for ARC on a
  // Reven device. If the device is compatible with ARC, it adds a job for
  // installing the Android DLC image.
  void OnEnableArcOnReven(std::deque<JobDesc> jobs, bool is_compatible);

  // Requests to disable ARC session and allows to optionally remove ARC data.
  // If ARC is already disabled, no-op.
  void RequestDisable(bool remove_arc_data);

  // RequestEnable() has a check in order not to trigger starting procedure
  // twice. This method can be called to bypass that check when restarting.
  void RequestEnableImpl();

  // Called when activation necessity check is done.
  void OnActivationNecessityChecked(bool result);

  // Negotiates the terms of service to user, if necessary.
  // Otherwise, move to StartAndroidManagementCheck().
  void MaybeStartTermsOfServiceNegotiation();

  // ArcRequirementChecker::Observer override:
  void OnArcOptInManagementCheckStarted() override;

  // Called when requirement checks are done.
  void OnRequirementChecksDone(
      ArcRequirementChecker::RequirementCheckResult result);

  void ShutdownSession();
  void ResetArcState();
  void OnArcSignInTimeout();

  // Starts requirement checks in background (in parallel with starting
  // ARC). This is for secondary or later ARC enabling.
  // The reason running them in parallel is for performance. The secondary or
  // later ARC enabling is typically on "logging into Chrome" for the user who
  // already opted in to use Google Play Store. In such a case, network is
  // typically not yet ready. Thus, if we block ARC boot, it delays several
  // seconds, which is not very user friendly.
  void StartBackgroundRequirementChecks();

  // Called when the background requirement checks are done.
  void OnBackgroundRequirementChecksDone(
      ArcRequirementChecker::BackgroundCheckResult result);

  // Requests to start ARC instance. Also, updates the internal state to
  // ACTIVE.
  void StartArc();

  // Calls StartArc() and starts background requirement checks.
  void StartArcForRegularBoot();

  // Requests to stop ARC instance. This resets two persistent flags:
  // kArcSignedIn and kArcTermsAccepted, so that, in next enabling,
  // it is started from Terms of Service negotiation.
  // TODO(hidehiko): Introduce STOPPING state, and this function should
  // transition to it.
  void StopArc();

  // ArcSessionRunner::Observer:
  void OnSessionStopped(ArcStopReason reason, bool restarting) override;
  void OnSessionRestarting() override;

  // Starts to remove ARC data, if it is requested via RequestArcDataRemoval().
  // On completion, OnArcDataRemoved() is called.
  // If not requested, just skipping the data removal, and moves to
  // MaybeReenableArc() or CheckArcVmDataMigrationNecessity() directly.
  void MaybeStartArcDataRemoval();
  void OnArcDataRemoved(std::optional<bool> success);

  // Checks whether /data migration is needed for enabling virtio-blk /data.
  // On completion, OnArcVmDataMigrationNecessityChecked() is called.
  // ArcSessionRunner::set_use_virtio_blk_data() should be called after the
  // check is finished but before ARC is enabled in MaybeReenableArc().
  void CheckArcVmDataMigrationNecessity(base::OnceClosure callback);
  void OnArcVmDataMigrationNecessityChecked(base::OnceClosure callback,
                                            std::optional<bool> result);

  // On ARC session stopped and/or data removal completion, this is called
  // so that, if necessary, ARC session is restarted.
  // TODO(hidehiko): This can be removed after the racy state machine
  // is fixed.
  void MaybeReenableArc();

  // Starts a timer to check if provisioning takes too long.
  // The timer will not be set if this device was previously provisioned
  // successfully.
  void MaybeStartTimer();

  // Starts mini-ARC and updates related information.
  void StartMiniArc();

  // Requests the support host (if it exists) to show the error, and notifies
  // the observers.
  void ShowArcSupportHostError(ArcSupportHost::ErrorInfo error_info,
                               bool should_show_send_feedback,
                               bool should_show_run_network_tests);

  // ash::SessionManagerClient::Observer:
  void EmitLoginPromptVisibleCalled() override;

  // Called when the first part of ExpandPropertyFilesAndReadSalt is done.
  void OnExpandPropertyFiles(bool result);

  // Called when ExpandPropertyFilesAndReadSalt is done.
  void OnExpandPropertyFilesAndReadSalt(ExpansionResult result);

  // Records whether the first activation is triggered during
  // the user session start up.
  // Only the first invocation records it, and following calls
  // will be no-op.
  void MaybeRecordFirstActivationDuringUserSessionStartUp(bool value);

  std::unique_ptr<ArcSessionRunner> arc_session_runner_;
  std::unique_ptr<AdbSideloadingAvailabilityDelegateImpl>
      adb_sideloading_availability_delegate_;

  // Unowned pointer. Keeps current profile.
  raw_ptr<Profile> profile_ = nullptr;

  // Whether ArcSessionManager is requested to enable (starting to run ARC
  // instance) or not.
  bool enable_requested_ = false;

  // Internal state machine. See also State enum class.
  State state_ = State::NOT_INITIALIZED;

  base::ObserverList<ArcSessionManagerObserver>::UncheckedAndDanglingUntriaged
      observer_list_;
  std::unique_ptr<ArcAppLauncher> playstore_launcher_;
  bool reenable_arc_ = false;
  bool provisioning_reported_ = false;
  bool skipped_terms_of_service_negotiation_ = false;
  bool activation_is_allowed_ = false;
  // Tri-state of if Activation is delayed. 1) std::nullopt means it is yet
  // unknown, 2) true means Activation is delayed by ARC-on-demand, and 3)
  // false means Activation is not delayed by ARC-on-demand.
  // TODO(hidehiko): Consider to rename to make it more explicit that this is
  // for ARC-On-Demand only.
  std::optional<bool> is_activation_delayed_ = false;
  bool is_first_activation_during_user_session_start_up_recorded_ = false;
  base::OneShotTimer arc_sign_in_timer_;

  std::unique_ptr<ArcSupportHost> support_host_;
  std::unique_ptr<ArcDataRemover> data_remover_;

  std::unique_ptr<ArcVmDataMigrationNecessityChecker>
      arc_vm_data_migration_necessity_checker_;

  ArcRequirementChecker::AndroidManagementCheckerFactory
      android_management_checker_factory_;
  std::unique_ptr<ArcRequirementChecker> requirement_checker_;

  std::unique_ptr<ArcActivationNecessityChecker> activation_necessity_checker_;

  std::unique_ptr<ScopedOptInFlowTracker> scoped_opt_in_tracker_;
  std::unique_ptr<ArcPaiStarter> pai_starter_;
  std::unique_ptr<ArcFastAppReinstallStarter> fast_app_reinstall_starter_;
  std::unique_ptr<ArcUiAvailabilityReporter> arc_ui_availability_reporter_;
  std::unique_ptr<ArcRevenHardwareChecker> hardware_checker_ =
      std::make_unique<arc::ArcRevenHardwareChecker>();

  // The time when the sign in process started.
  base::TimeTicks sign_in_start_time_;
  // The time when ARC was pre-started (mini-ARC start).
  base::TimeTicks pre_start_time_;
  // The time when ARC was about to start.
  base::TimeTicks start_time_;

  // Timer set up when ARC necessity check is completed
  // but user session start up task are not yet completed.
  // Used to measure the elapsed time between it and the user session
  // start up task completion.
  struct UserSessionStartUpTaskTimer {
    base::ElapsedTimer timer;
    bool deferred;
  };
  std::optional<UserSessionStartUpTaskTimer> user_session_start_up_task_timer_;

  base::RepeatingClosure attempt_user_exit_callback_;

  base::RepeatingClosure attempt_restart_callback_;

  ArcAppIdProviderImpl app_id_provider_;

  // The content of /var/lib/misc/arc_salt. Empty if the file doesn't exist.
  std::optional<std::string> arc_salt_on_disk_;

  std::optional<bool> property_files_expansion_result_;

  std::unique_ptr<ArcDlcInstaller> arc_dlc_installer_;

  std::optional<guest_os::GuestOsMountProviderRegistry::Id>
      arcvm_mount_provider_id_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  // Must be the last member.
  base::WeakPtrFactory<ArcSessionManager> weak_ptr_factory_{this};
};

// Outputs the stringified |state| to |os|. This is only for logging purposes.
std::ostream& operator<<(std::ostream& os,
                         const ArcSessionManager::State& state);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_SESSION_MANAGER_H_
