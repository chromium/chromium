// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_PROFILE_DIFF_AFTER_RESTART_RECORDER_H_
#define CHROME_BROWSER_SESSIONS_PROFILE_DIFF_AFTER_RESTART_RECORDER_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "chrome/browser/sessions/session_restore_observer.h"

class PrefService;
class Profile;

// Records SessionRestore.ProfileDiffAfterRestart.{Normal,App}: the difference
// between the number of profiles that had windows open immediately before a
// browser restart and the number of profiles whose windows were restored
// afterwards.
//
// The pre-restart counts are written to the `kPreSmartRestartProfileCounts`
// local state pref by SavePreRestartTabWindowCounts() while the browser is
// shutting down. This class consumes them during the next startup:
//
//   1. StartupBrowserCreator calls MaybeStartRecording() before launching
//      profiles, which consumes the saved pre-restart counts from local state
//      and enables the recorder if the browser was restarted.
//   2. While enabled, the recorder observes session restore and remembers
//      which profiles restored at least one normal or app window.
//   3. The metrics are emitted once startup launches have finished *and* no
//      session restore is still in flight, whichever happens last. Session
//      restore at startup is usually synchronous, but it is asynchronous for
//      profiles whose session data is read after the launch loop completes,
//      so both orderings must be handled.
//
// Startup skips the profile picker entirely after a restart, so that the
// session can be restored. The picker is still shown if every profile that
// was open is locked, in which case nothing is recorded because no session
// restore is attempted (see AbandonRecording()).
//
// Built and recorded on Windows, Mac, and Linux only. ChromeOS restarts end the
// user session rather than relaunching into it, and startup ignores
// `WasRestarted()` after the next login, so this class is excluded from
// ChromeOS builds.
//
// All methods must be called on the UI thread.
class ProfileDiffAfterRestartRecorder : public SessionRestoreObserver {
 public:
  // Keys of the `kPreSmartRestartProfileCounts` local state dictionary pref.
  // Note that these are distinct from the SessionRestore::k*Key constants,
  // which key into the per-profile `kPreSmartRestartSessionState` pref.
  static constexpr char kNormalProfilesKey[] = "normal_profiles";
  static constexpr char kAppProfilesKey[] = "app_profiles";

  static constexpr char kNormalHistogram[] =
      "SessionRestore.ProfileDiffAfterRestart.Normal";
  static constexpr char kAppHistogram[] =
      "SessionRestore.ProfileDiffAfterRestart.App";

  // Returns the process-wide recorder. The instance is never destroyed; it
  // stops observing session restore as soon as it is done recording.
  static ProfileDiffAfterRestartRecorder& GetInstance();

  // Saves the number of profiles that had normal and app windows open before a
  // restart. Clears the pref when both counts are zero so counts from an
  // earlier restart cannot carry over.
  static void SavePreRestartCounts(PrefService* local_state,
                                   size_t normal_profiles,
                                   size_t app_profiles);

  // Discards pre-restart counts saved by an earlier session. Called on startups
  // that are not restarts, where the saved counts can never be consumed.
  static void ClearPreRestartCounts(PrefService* local_state);

  ProfileDiffAfterRestartRecorder(const ProfileDiffAfterRestartRecorder&) =
      delete;
  ProfileDiffAfterRestartRecorder& operator=(
      const ProfileDiffAfterRestartRecorder&) = delete;

  // Consumes any saved pre-restart counts from `local_state` and starts
  // observing session restore if the feature is enabled. Must only be called
  // when the browser was restarted.
  void MaybeStartRecording(PrefService* local_state);

  // Called once StartupBrowserCreator has finished launching every profile it
  // intends to launch. Records the metrics if no session restore is still in
  // flight.
  void OnStartupLaunchesFinished();

  // Stops recording without emitting anything. Called when startup shows the
  // profile picker instead of launching profiles.
  void AbandonRecording();

  bool is_recording_for_testing() const { return recording_; }
  void ResetForTesting();

  // SessionRestoreObserver:
  void OnProfileSessionRestored(Profile* profile,
                                int normal_windows,
                                int app_windows) override;

 private:
  friend class base::NoDestructor<ProfileDiffAfterRestartRecorder>;

  ProfileDiffAfterRestartRecorder();
  // Never called: the instance is a NoDestructor singleton. Defined so that
  // the class can hold members with non-trivial destructors.
  ~ProfileDiffAfterRestartRecorder();

  // Emits the metrics and stops recording, but only once startup launches have
  // finished and every in-flight session restore has completed.
  void MaybeRecordDiff();

  // Stops observing session restore and drops all accumulated state.
  void StopRecording();

  bool recording_ = false;
  bool launches_finished_ = false;
  std::optional<size_t> expected_normal_profiles_;
  std::optional<size_t> expected_app_profiles_;

  // Paths of the profiles that restored at least one normal / app window.
  // Keyed by path rather than by Profile* so that the sets stay valid even if
  // a profile is destroyed while recording.
  base::flat_set<base::FilePath> restored_normal_profiles_;
  base::flat_set<base::FilePath> restored_app_profiles_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_SESSIONS_PROFILE_DIFF_AFTER_RESTART_RECORDER_H_
