// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_load_tracker_win.h"

#include <string_view>

#include "base/feature.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace features {

// When enabled, Profile loading/unloading and exit type recording will be
// tracked via a `ProfileLoadTracker`, and stats regarding load/unload success
// rates will be recorded to UMA.
BASE_FEATURE(kProfileLoadTracker, base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features

namespace {

std::string_view GetHistogramSuffix(
    profile_metrics::BrowserProfileType profile_type) {
  // LINT.IfChange(BrowserProfileType)
  switch (profile_type) {
    case profile_metrics::BrowserProfileType::kRegular:
      return "Regular";
    case profile_metrics::BrowserProfileType::kIncognito:
      return "Incognito";
    case profile_metrics::BrowserProfileType::kGuest:
      return "Guest";
    case profile_metrics::BrowserProfileType::kSystem:
      return "System";
    case profile_metrics::BrowserProfileType::kOtherOffTheRecordProfile:
      return "OtherOffTheRecordProfile";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/profile/histograms.xml:BrowserProfileType)
}

}  // namespace

ProfileLoadTracker::ProfileLoadTracker(Profile& profile)
    : histogram_suffix_(
          GetHistogramSuffix(profile_metrics::GetBrowserProfileType(&profile))),
      profile_dir_(profile.GetPath()) {
  // Allow path creation for profile locking.
  base::ScopedAllowBlocking allow_blocking;

  // The combination of FLAG_WRITE with FLAG_WIN_EXCLUSIVE_WRITE guarantees that
  // this initialization will fail if the file is already open by this or
  // another browser process.
  static constexpr int kLockFlags =
      base::File::FLAG_WIN_EXCLUSIVE_WRITE | base::File::FLAG_WRITE |
      base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_DELETE_ON_CLOSE |
      base::File::FLAG_WIN_SHARE_DELETE;

  lock_file_.Initialize(profile_dir_.Append(kLockFileName), kLockFlags);
  DWORD error = lock_file_.IsValid() ? ERROR_SUCCESS : ::GetLastError();
  base::UmaHistogramSparse(
      base::StrCat({"Profile.Windows.CreateLockResult.", histogram_suffix_}),
      error);

  if (!lock_file_.IsValid()) {
    // IN_USE indicates that the EXCLUSIVE_WRITE open failed, meaning there is
    // currently a FLAG_WRITE handle open to the file.
    ProfileLockStatus failure_reason =
        lock_file_.error_details() == base::File::FILE_ERROR_IN_USE
            ? ProfileLockStatus::kFailedInUse
            // File open failed, but not because the file was not present. The
            // reason is thus unknown. Antivirus/driver failures/weird
            // filesystem errors?
            : ProfileLockStatus::kFailedUnknown;
    base::UmaHistogramEnumeration(
        base::StrCat({"Profile.Windows.ProfileLockStatus.", histogram_suffix_}),
        failure_reason);
    return;
  }
  // If the file was just opened rather than created, then it was not deleted by
  // Windows upon unload or process termination. There must have been a system
  // crash that prevented its deletion. This is fine, however, and execution
  // will continue.
  ProfileLockStatus acquired_reason =
      !lock_file_.created() ? ProfileLockStatus::kAcquiredSystemCrash
                            : ProfileLockStatus::kAcquired;
  base::UmaHistogramEnumeration(
      base::StrCat({"Profile.Windows.ProfileLockStatus.", histogram_suffix_}),
      acquired_reason);

  // The lock file was successfully created/opened, such that this process
  // exclusively owns write access to it, and thus, ProfilePreviousExitStatus
  // may be recorded, assuming all goes well henceforth. Create/open locked
  // files with the most permissive sharing settings to maximize the chances of
  // success.

  // Determine whether or not the previous session terminated cleanly.
  base::File dirty_file(profile_dir_.Append(kDirtyFileName),
                        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE |
                            base::File::FLAG_WIN_SHARE_DELETE);
  error = dirty_file.IsValid() ? ERROR_SUCCESS : ::GetLastError();
  base::UmaHistogramSparse(
      base::StrCat({"Profile.Windows.CreateDirtyResult.", histogram_suffix_}),
      error);
  if (!dirty_file.IsValid()) {
    // Do not record `Profile.Windows.ProfilePreviousExitStatus` when this
    // filesystem call fails.
    return;
  }

  const bool previous_session_dirty = !dirty_file.created();
  // The "dirty" file is left on-disk for the lifetime of this session. It
  // will be deleted if/when this session exits cleanly to indicate such to
  // the next session.
  ProfilePreviousExitStatus previous_exit_status =
      ProfilePreviousExitStatus::kNoCrash;
  base::FilePath waiting_for_crash_ack_path =
      profile_dir_.Append(kWaitingForCrashAckFileName);
  if (previous_session_dirty) {
    // Unconditionally create the file since the "dirty" file would not
    // exist if the crash had been acknowledged.
    base::File waiting_for_crash_ack_file(
        waiting_for_crash_ack_path, base::File::FLAG_OPEN_ALWAYS |
                                        base::File::FLAG_WRITE |
                                        base::File::FLAG_WIN_SHARE_DELETE);
    error =
        waiting_for_crash_ack_file.IsValid() ? ERROR_SUCCESS : ::GetLastError();
    base::UmaHistogramSparse(
        base::StrCat(
            {"Profile.Windows.CreateCrashAckResult.", histogram_suffix_}),
        error);

    if (!waiting_for_crash_ack_file.IsValid()) {
      // Failed to create/open the file.
      // Do not record `Profile.Windows.ProfilePreviousExitStatus` when this
      // filesystem call fails.
      return;
    }
    // If the crash ack file was present, the previous session did not ack the
    // crash.
    previous_exit_status = (!waiting_for_crash_ack_file.created())
                               ? ProfilePreviousExitStatus::kDirtyDidNotAck
                               : ProfilePreviousExitStatus::kDirty;

  } else if (base::PathExists(waiting_for_crash_ack_path)) {
    // The previous session did not crash, but there is a lingering need for
    // the user to acknowledge a preceding session's crash.
    previous_exit_status = ProfilePreviousExitStatus::kNoCrashDidNotAck;
  } else {
    // The previous session did not crash, and the user does not need to
    // acknowledge a preceding session's crash.
    CHECK_EQ(previous_exit_status, ProfilePreviousExitStatus::kNoCrash);
  }
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Profile.Windows.ProfilePreviousExitStatus.", histogram_suffix_}),
      previous_exit_status);
}

ProfileLoadTracker::~ProfileLoadTracker() {
  if (lock_file_.IsValid()) {
    base::ScopedAllowBlocking allow_blocking;
    base::FilePath dirty_path = profile_dir_.Append(kDirtyFileName);

    // Delete the dirty file to confirm the profile was unloaded without any
    // crash. Must happen under the profile lock to avoid race condition.
    base::UmaHistogramSparse(
        base::StrCat({"Profile.Windows.DeleteDirtyResult.", histogram_suffix_}),
        base::DeleteFile(dirty_path) ? ERROR_SUCCESS : ::GetLastError());

    // Release the lock on the profile. Closing the handle will automatically
    // delete Profile.lock due to FLAG_DELETE_ON_CLOSE. This file must be
    // closed explicitly here because it is a blocking operation.
    lock_file_.Close();
  }
}

void ProfileLoadTracker::AckCrashForTracking() {
  // Delete the crash_ack file to confirm the crash bubble was acked.
  // Must happen under the profile lock to avoid race condition.
  if (lock_file_.IsValid()) {
    // Allow file deletion for crash ack.
    base::ScopedAllowBlocking allow_blocking;
    base::FilePath waiting_for_crash_ack_path =
        profile_dir_.Append(kWaitingForCrashAckFileName);
    base::UmaHistogramSparse(
        base::StrCat(
            {"Profile.Windows.DeleteCrashAckResult.", histogram_suffix_}),
        base::DeleteFile(waiting_for_crash_ack_path) ? ERROR_SUCCESS
                                                     : ::GetLastError());
  }
}
