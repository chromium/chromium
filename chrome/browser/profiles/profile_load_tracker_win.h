// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_LOAD_TRACKER_WIN_H_
#define CHROME_BROWSER_PROFILES_PROFILE_LOAD_TRACKER_WIN_H_

#include <string_view>

#include "base/feature.h"
#include "base/files/file.h"
#include "base/files/file_path.h"

class Profile;

namespace features {
BASE_DECLARE_FEATURE(kProfileLoadTracker);
}  // namespace features

// On Windows, in addition to the normal prefs-based exit type recording, a
// sideband tracking mechanism in `ExitTypeService` is implemented using three
// files in the profile directory for purposes of more thorough understanding of
// the current failure modes:
//
// Profile.lock: Created on profile load to ensure exclusive access.
// Automatically deleted by the OS on close or a Chrome crash due to
// FLAG_DELETE_ON_CLOSE.
//
// Profile.dirty: Only ever created after `Profile.lock` is exclusively
// acquired. Deleted during tracker destruction during profile teardown (clean
// exit). Its presence on profile load indicates a previous unclean shutdown.
//
// Profile.waiting_for_crash_ack: Locked behind Profile.lock. Created on profile
// load only if Profile.dirty was already present. Deleted when the user
// acknowledges the crash bubble.
//
// Profile.lock guards the other two files, and its acquiry status is recorded
// with `ProfileLockStatus`.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ProfileLockStatus)
enum class ProfileLockStatus {
  // The file was acquired successfully.
  kAcquired = 0,
  // The previous instance of Chrome died in a system crash.
  kAcquiredSystemCrash = 1,
  // There is another writeable handle to the file open.
  kFailedInUse = 2,
  // Filesystem error.
  kFailedUnknown = 3,
  kMaxValue = kFailedUnknown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/profile/enums.xml:ProfileLockStatus)

// When loading a profile, the previous exit status of that profile can be
// determined:
// - If only `Profile.dirty` exists, record `kDirty` on profile load ince the
//   crash markers are still around from last launch, create
//   Profile.waiting_for_crash_ack on disk.
//   - Then, if crash bubble is acked:
//     - Profile.waiting_for_crash_ack is cleared. Things proceed back to normal
//       and next relaunch will record `kNoCrash` if exit is clean or kDirty
//       if the profile is not unloaded correctly.
//   - Else if the profile is unloaded normally without acking the bubble:
//     - Profile.dirty is cleared on exit. Profile.waiting_for_crash_ack
//       remains on disk. On next profile load, Profile.Windows.LoadStatus
//       records kNoCrashDidNotAck, since Profile.waiting_for_crash_ack was
//       present without Profile.dirty.
//   - Else if the profile is closed uncleanly without acking the bubble:
//     - Crash bubble is shown again. Both Profile.dirty and
//       Profile.waiting_for_crash_ack are on disk. Profile.Windows.LoadStatus
//       records kDirtyWaitingForAck, since both files are on disk.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ProfilePreviousExitStatus)
enum class ProfilePreviousExitStatus {
  // Clean exit, and no previously un-acked crash bubble.
  kNoCrash = 0,
  // Clean exit, but user did not ack a crash bubble.
  kNoCrashDidNotAck = 1,
  // Previous session dirty.
  kDirty = 2,
  // Dirty, and there was an un-acked bubble.
  kDirtyDidNotAck = 3,
  kMaxValue = kDirtyDidNotAck,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/profile/enums.xml:ProfilePreviousExitStatus)

// The emission of `ProfilePreviousExitStatus` depends on the acquisition of
// Profile.lock, and so, if `ProfileLockStatus` is either `kFailedLocked` or
// `kFailedUnknown`, `ProfilePreviousExitStatus` will not be recorded.

class ProfileLoadTracker {
 public:
  explicit ProfileLoadTracker(Profile& profile);
  ~ProfileLoadTracker();

  // Created on profile load to ensure exclusive access. Automatically deleted
  // by the OS on close/crash due to FLAG_DELETE_ON_CLOSE.
  static constexpr base::FilePath::CharType kLockFileName[] =
      FILE_PATH_LITERAL("Profile.lock");
  // Created on profile load. Deleted during destructor (clean exit). If it
  // already exists on profile load, it indicates an unclean shutdown.
  static constexpr base::FilePath::CharType kDirtyFileName[] =
      FILE_PATH_LITERAL("Profile.dirty");
  // Created on profile load if Profile.dirty was already present. Deleted
  // when the user acknowledges the crash bubble.
  static constexpr base::FilePath::CharType kWaitingForCrashAckFileName[] =
      FILE_PATH_LITERAL("Profile.waiting_for_crash_ack");

  // Remove `Profile.waiting_for_crash_ack`, only if a writeable handle to
  // `Profile.lock` is already held by this process.
  void AckCrashForTracking();

 private:
  const std::string_view histogram_suffix_;
  const base::FilePath profile_dir_;

  // Held exclusively for the lifetime of the profile loading session. Closing
  // the handle will automatically delete the `lock_file_` due to
  // FLAG_DELETE_ON_CLOSE.
  base::File lock_file_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_LOAD_TRACKER_WIN_H_
