// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_
#define ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/components/arc/enterprise/arc_apps_tracker.h"
#include "ash/components/arc/enterprise/snapshot_hours_policy_service.h"
#include "ash/components/arc/enterprise/snapshot_reboot_controller.h"
#include "ash/components/arc/enterprise/snapshot_session_controller.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefService;

namespace base {

class Value;

}  // namespace base

namespace arc {
namespace data_snapshotd {

class ArcDataRemoveRequestedPrefHandler;
class ArcDataSnapshotdBridge;

// Ozone platform headless command line switch value.
extern const char kHeadless[];
// Environment variable to be passed to UpstartClient::StartArcDataSnapshotd
// if the frecon (freon console from platform/frecon) must be restarted.
// The environment variable is passed in order to restart frecon from a
// configuration script and not grant excessive permissions to
// arc-data-snapshotd.
// The restart of frecon is needed only when system UI is shown (in BlockedUi
// state).
extern const char kRestartFreconEnv[];

// This class manages ARC data/ directory snapshots and controls the lifetime of
// the arc-data-snapshotd daemon.
class ArcDataSnapshotdManager final
    : public SnapshotSessionController::Observer,
      public SnapshotHoursPolicyService::Observer {
 public:
  // State of the flow.
  enum class State {
    kNone,
    // Blocked UI mode is ON.
    kBlockedUi,
    // In process of loading a snapshot.
    kLoading,
    // In blocked UI mode, MGS can be launched.
    kMgsToLaunch,
    // MGS is launched to create a snapshot.
    kMgsLaunched,
    // User session was restored.
    kRestored,
    // Snapshot feature is disabled, in the process of stopping. The browser
    // must be restarted once snapshots are cleared.
    kStopping,
    // Running with a snapshot.
    kRunning,
  };

  // This class is a delegate to perform actions in Chrome.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Requests to stop ARC instance and async invokes |stopped_callback| when
    // ARC is not running with a success result or failure result if ARC is
    // failed to stop.
    // |stopped_callback| should never be null.
    virtual void RequestStopArcInstance(
        base::OnceCallback<void(bool)> stopped_callback) = 0;

    // Returns a current profile pref service. Should be called only when ARC
    // session is up and running.
    virtual PrefService* GetProfilePrefService() = 0;

    // Creates a snapshot reboot notification.
    virtual std::unique_ptr<ArcSnapshotRebootNotification>
    CreateRebootNotification() = 0;

    // Creates an ARC apps tracker.
    virtual std::unique_ptr<ArcAppsTracker> CreateAppsTracker() = 0;

    virtual void RestartChrome(const base::CommandLine& command_line) = 0;
  };

  // This class operates with a snapshot related info either last or
  // backed-up (previous): stores and keeps in sync with an appropriate
  // preference in local state.
  class SnapshotInfo {
   public:
    // Creates new snapshot with current parameters.
    explicit SnapshotInfo(bool is_last);
    SnapshotInfo(const base::Value::Dict& value, bool is_last);
    SnapshotInfo(const SnapshotInfo&) = delete;
    SnapshotInfo& operator=(const SnapshotInfo&) = delete;
    ~SnapshotInfo();

    // Creates from the passed arguments instead of constructing it from
    // dictionary.
    static std::unique_ptr<SnapshotInfo> CreateForTesting(
        const std::string& os_version,
        const base::Time& creation_date,
        bool verified,
        bool updated,
        bool is_last);

    // Syncs stored snapshot info to dictionaty |value|.
    void Sync(base::Value::Dict& value);

    // Returns true if snapshot is expired.
    bool IsExpired() const;

    // Returns true if OS version is updated, since the snapshot has been taken.
    bool IsOsVersionUpdated() const;

    void set_verified(bool verified) { verified_ = true; }
    bool is_verified() const { return verified_; }

    void set_is_last(bool is_last) { is_last_ = is_last; }
    bool is_last() const { return is_last_; }

    void set_updated(bool updated) { updated_ = updated; }
    bool updated() const { return updated_; }

   private:
    SnapshotInfo(const std::string& os_version,
                 const base::Time& creation_date,
                 bool verified,
                 bool updated,
                 bool is_last);

    // Returns dictionary path in arc.snapshot local state preference.
    std::string GetDictPath() const;

    void UpdateCreationDate(const base::Time& creation_date);

    // Called once this snapshot is expired.
    void OnSnapshotExpired();

    // True if the instance is the last snapshot taken.
    bool is_last_;

    // Values should be kept in sync with values stored in arc.snapshot.last or
    // arc.snapshot.previous preferences.
    std::string os_version_;
    base::Time creation_date_;
    bool verified_ = false;
    bool updated_ = false;

    // The snapshots' lifetime timer is fired when this snapshot must be
    // cleared.
    base::OneShotTimer lifetime_timer_;

    base::WeakPtrFactory<SnapshotInfo> weak_ptr_factory_{this};
  };

  // This class operates with a snapshot related info including mode and
  // creation flow time: stores and keeps in sync with arc.snapshot preference
  // in local state.
  class Snapshot {
   public:
    // Snapshot does not own |local_state|, it must be non nullptr and must
    // outlive the instance.
    explicit Snapshot(PrefService* local_state);

    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    ~Snapshot();

    // Creates an instance from the passed arguments instead of reading it from
    // |local_state|.
    static std::unique_ptr<Snapshot> CreateForTesting(
        PrefService* local_state,
        bool blocked_ui_mode,
        bool started,
        std::unique_ptr<SnapshotInfo> last_snapshot,
        std::unique_ptr<SnapshotInfo> previous_snapshot);

    // Parses the snapshot info from arc.snapshot preference.
    void Parse();
    // Syncs stored snapshot info to local state.
    void Sync();

    // Syncs stored snapshot info to local state.
    // |callback| is executed once all changes to the local state have been
    // committed.
    void Sync(base::OnceClosure callback);

    // Clears snapshot related info in arc.snapshot preference either last
    // if |last| is true or previous otherwise.
    void ClearSnapshot(bool last);

    // Moves last snapshot to previous and updates a |start_date| to the current
    // date.
    void StartNewSnapshot();

    // Updates the last snapshot creation date and OS version.
    void OnSnapshotTaken();

    // Returns the info of a snapshot in use.
    SnapshotInfo* GetCurrentSnapshot();

    void set_blocked_ui_mode(bool blocked_ui_mode) {
      blocked_ui_mode_ = blocked_ui_mode;
    }
    bool is_blocked_ui_mode() const { return blocked_ui_mode_; }
    bool started() const { return started_; }
    SnapshotInfo* last_snapshot() { return last_snapshot_.get(); }
    SnapshotInfo* previous_snapshot() { return previous_snapshot_.get(); }

   private:
    Snapshot(PrefService* local_state,
             bool blocked_ui_mode,
             bool started,
             std::unique_ptr<SnapshotInfo> last_snapshot,
             std::unique_ptr<SnapshotInfo> previous_snapshot);

    // Unowned pointer - outlives this instance.
    PrefService* const local_state_;

    // Values should be kept in sync with values stored in arc.snapshot
    // preference.
    bool blocked_ui_mode_ = false;
    bool started_ = false;
    std::unique_ptr<SnapshotInfo> last_snapshot_;
    std::unique_ptr<SnapshotInfo> previous_snapshot_;
  };

  ArcDataSnapshotdManager(PrefService* local_state,
                          std::unique_ptr<Delegate> delegate,
                          base::OnceClosure attempt_user_exit_callback);
  ArcDataSnapshotdManager(const ArcDataSnapshotdManager&) = delete;
  ArcDataSnapshotdManager& operator=(const ArcDataSnapshotdManager&) = delete;
  ~ArcDataSnapshotdManager() override;

  static ArcDataSnapshotdManager* Get();

  static base::TimeDelta snapshot_max_lifetime_for_testing();

  // Starts arc-data-snapshotd.
  void EnsureDaemonStarted(base::OnceClosure callback);
  // Stops arc-data-snapshotd.
  void EnsureDaemonStopped(base::OnceClosure callback);

  // Starts loading a snapshot to android-data directory.
  // |callback| is called once the process is over.
  void StartLoadingSnapshot(base::OnceClosure callback);

  // Returns true if autologin to public account should be performed.
  bool IsAutoLoginConfigured();
  // Returns true if autologin is allowed to be performed and manager is not
  // waiting for the response from arc-data-snapshotd daemon.
  bool IsAutoLoginAllowed();
  // Returns true if blocked UI screen is shown.
  bool IsBlockedUiScreenShown();

  // Returns true if ARC data snapshot update is in progress.
  bool IsSnapshotInProgress();

  // SnapshotSessionController::Observer overrides:
  void OnSnapshotSessionStarted() override;
  void OnSnapshotSessionStopped() override;
  void OnSnapshotSessionFailed() override;
  void OnSnapshotAppInstalled(int percent) override;
  void OnSnapshotSessionPolicyCompliant() override;

  static void set_snapshot_enabled_for_testing(bool enabled) {
    is_snapshot_enabled_for_testing_ = enabled;
  }
  static bool is_snapshot_enabled_for_testing() {
    return is_snapshot_enabled_for_testing_;
  }

  // SnapshotHoursPolicyService::Observer overrides:
  void OnSnapshotsDisabled() override;
  void OnSnapshotUpdateEndTimeChanged() override;

  // Returns true if the feature is enabled.
  bool IsSnapshotEnabled();

  SnapshotHoursPolicyService* policy_service() { return &policy_service_; }

  // Get |bridge_| for testing.
  ArcDataSnapshotdBridge* bridge() { return bridge_.get(); }

  State state() const { return state_; }

  base::OnceClosure& get_reset_autologin_callback_for_testing() {
    return reset_autologin_callback_;
  }
  void set_reset_autologin_callback(base::OnceClosure callback) {
    reset_autologin_callback_ = std::move(callback);
  }
  void set_state_for_testing(State state) { state_ = state; }

  void set_session_controller_for_testing(
      std::unique_ptr<SnapshotSessionController> session_controller) {
    session_controller_ = std::move(session_controller);
  }

  SnapshotRebootController* get_reboot_controller_for_testing() const {
    return reboot_controller_.get();
  }

 private:
  // Local State initialization observer.
  void OnLocalStateInitialized(bool intialized);

  // Attempts to arc-data-snapshotd daemon regardless of state of the class.
  // Runs |callback| once finished.
  void StopDaemon(base::OnceClosure callback);

  // Attempts to clear snapshots.
  void DoClearSnapshots();
  // Attempts to clear the passed snapshot, calls |callback| once finished.
  // |success| indicates a successfully or not the previous operation has been
  // finished.
  void DoClearSnapshot(SnapshotInfo* snapshot,
                       base::OnceCallback<void(bool)> callback,
                       bool success);

  // Delegates operations to |bridge_|
  void GenerateKeyPair();
  void ClearSnapshot(bool last, base::OnceCallback<void(bool)> callback);
  void TakeSnapshot(const std::string& account_id);
  void LoadSnapshot(const std::string& account_id, base::OnceClosure callback);
  void UpdateUi(int percent);

  // Called once a snapshot is expired.
  void OnSnapshotExpired();
  // Called once the outdated snapshots were removed or ensured that there are
  // no outdated snapshots.
  void OnSnapshotsCleared(bool success);
  // Called once GenerateKeyPair is finished with a result |success|.
  void OnKeyPairGenerated(bool success);
  // Called once arc-data-snapshotd starting process is finished with result
  // |success|, runs |callback| afterwards.
  void OnDaemonStarted(base::OnceClosure callback, bool success);
  // Called once arc-data-snapshotd stopping process is finished with result
  // |success|, runs |callback| afterwards.
  void OnDaemonStopped(base::OnceClosure callback, bool success);

  // Callback to be passed to ArcAppsTracker::StartTracking to get notified
  // about a percentage of installed apps.
  void Update(int percent);

  // Called once ARC instance is stopped.
  void OnArcInstanceStopped(bool success);

  // Called once a snapshot is taken.
  void OnSnapshotTaken(bool success);

  // Called once a snapshot is taken.
  void OnSnapshotLoaded(base::OnceClosure callback, bool success, bool last);

  // Called once unexpected ARC data removal is requested,
  void OnUnexpectedArcDataRemoveRequested();

  // Called once a progress bar is updated.
  void OnUiUpdated(bool success);

  // Called once user escapes the blocked UI screen.
  void OnUiClosed();

  // Returns the list of daemon enviromnet variables to be passed to upstart of
  // arc-data-snapshotd daemon.
  // Currently, sets RESTART_FRECON=1 if the UI should be blocked.
  std::vector<std::string> GetStartEnvVars();

  static bool is_snapshot_enabled_for_testing_;

  SnapshotHoursPolicyService policy_service_;

  State state_ = State::kNone;
  Snapshot snapshot_;

  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<ArcDataSnapshotdBridge> bridge_;
  std::unique_ptr<ArcDataRemoveRequestedPrefHandler>
      data_remove_requested_handler_;

  base::OnceClosure attempt_user_exit_callback_;

  // Callback to reset an autologin timer once userless MGS is ready to start.
  base::OnceClosure reset_autologin_callback_;

  // Initialized only when needed to observe and call back on a user session
  // events.
  std::unique_ptr<SnapshotSessionController> session_controller_;

  // Initialized only when the device reboot is requested.
  std::unique_ptr<SnapshotRebootController> reboot_controller_;

  // Used for cancelling previously posted tasks to daemon.
  base::WeakPtrFactory<ArcDataSnapshotdManager> daemon_weak_ptr_factory_{this};
  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcDataSnapshotdManager> weak_ptr_factory_{this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_
