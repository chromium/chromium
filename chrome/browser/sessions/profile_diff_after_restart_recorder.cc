// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/profile_diff_after_restart_recorder.h"

#include <algorithm>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

// Bounds the samples so that corrupted local state cannot create unbounded
// distinct buckets in the sparse histograms.
constexpr int kMaxProfileDiff = 50;

// Returns `count` as a positive size_t, or std::nullopt if absent or not
// positive. SavePreRestartCounts() never saves zero, so a zero count can only
// come from corrupted local state.
std::optional<size_t> ToProfileCount(std::optional<int> count) {
  if (!count.has_value() || *count <= 0) {
    return std::nullopt;
  }
  return base::checked_cast<size_t>(*count);
}

// Emits `expected - actual`, clamped to [-kMaxProfileDiff, kMaxProfileDiff],
// when `expected` is present. A positive sample means profiles were lost
// across the restart; a negative sample means profiles were gained.
void RecordDiffMetric(std::string_view histogram_name,
                      std::optional<size_t> expected,
                      size_t actual) {
  if (!expected.has_value()) {
    return;
  }
  int sample =
      base::checked_cast<int>(*expected) - base::checked_cast<int>(actual);
  base::UmaHistogramSparse(
      histogram_name, std::clamp(sample, -kMaxProfileDiff, kMaxProfileDiff));
}

}  // namespace

// static
ProfileDiffAfterRestartRecorder&
ProfileDiffAfterRestartRecorder::GetInstance() {
  static base::NoDestructor<ProfileDiffAfterRestartRecorder> instance;
  return *instance;
}

// static
void ProfileDiffAfterRestartRecorder::SavePreRestartCounts(
    PrefService* local_state,
    size_t normal_profiles,
    size_t app_profiles) {
  ClearPreRestartCounts(local_state);

  base::DictValue profile_dict;
  if (normal_profiles > 0) {
    profile_dict.Set(kNormalProfilesKey,
                     base::checked_cast<int>(normal_profiles));
  }
  if (app_profiles > 0) {
    profile_dict.Set(kAppProfilesKey, base::checked_cast<int>(app_profiles));
  }
  if (!profile_dict.empty()) {
    local_state->SetDict(prefs::kPreSmartRestartProfileCounts,
                         std::move(profile_dict));
  }
}

// static
void ProfileDiffAfterRestartRecorder::ClearPreRestartCounts(
    PrefService* local_state) {
  CHECK(local_state);
  local_state->ClearPref(prefs::kPreSmartRestartProfileCounts);
}

ProfileDiffAfterRestartRecorder::ProfileDiffAfterRestartRecorder() = default;

ProfileDiffAfterRestartRecorder::~ProfileDiffAfterRestartRecorder() {
  StopRecording();
}

void ProfileDiffAfterRestartRecorder::MaybeStartRecording(
    PrefService* local_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(local_state);
  if (recording_) {
    return;
  }

  // Nothing to compare against: the previous session had no windows open, or
  // it exited without going through a restart.
  if (!local_state->HasPrefPath(prefs::kPreSmartRestartProfileCounts)) {
    return;
  }

  const base::DictValue& pre_restart_counts =
      local_state->GetDict(prefs::kPreSmartRestartProfileCounts);
  std::optional<size_t> expected_normal =
      ToProfileCount(pre_restart_counts.FindInt(kNormalProfilesKey));
  std::optional<size_t> expected_app =
      ToProfileCount(pre_restart_counts.FindInt(kAppProfilesKey));

  // Consume the pref immediately so that stale counts cannot linger on disk if
  // startup is interrupted, and cannot be clobbered if startup browser creation
  // runs a second wave while asynchronous restores are still in flight.
  ClearPreRestartCounts(local_state);

  // Check counts first so clients with nothing to record do not enter the
  // study.
  if (!expected_normal.has_value() && !expected_app.has_value()) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kRecordTabWindowDiffOnRestart)) {
    return;
  }

  recording_ = true;
  launches_finished_ = false;
  expected_normal_profiles_ = expected_normal;
  expected_app_profiles_ = expected_app;
  SessionRestore::AddObserver(this);
}

void ProfileDiffAfterRestartRecorder::OnStartupLaunchesFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!recording_) {
    return;
  }

  launches_finished_ = true;
  MaybeRecordDiff();
}

void ProfileDiffAfterRestartRecorder::AbandonRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!recording_) {
    return;
  }

  StopRecording();
}

void ProfileDiffAfterRestartRecorder::ResetForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StopRecording();
}

void ProfileDiffAfterRestartRecorder::OnProfileSessionRestored(
    Profile* profile,
    int normal_windows,
    int app_windows) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!recording_) {
    return;
  }
  if (normal_windows > 0) {
    restored_normal_profiles_.insert(profile->GetPath());
  }
  if (app_windows > 0) {
    restored_app_profiles_.insert(profile->GetPath());
  }
  MaybeRecordDiff();
}

void ProfileDiffAfterRestartRecorder::MaybeRecordDiff() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!recording_) {
    return;
  }

  // Profiles are still being launched, or a session restore is still creating
  // windows. Whichever of the two finishes last will call back here.
  // This relies on ~SessionRestoreImpl() (chrome/browser/sessions/
  // session_restore.cc) leaving the active restorer set before notifying
  // observers. Otherwise the final OnProfileSessionRestored() would see
  // IsAnySessionCurrentlyRestoring() return true, nothing would call back
  // afterwards, and SessionRestore.ProfileDiffAfterRestart.{Normal,App}
  // would be dropped.
  if (!launches_finished_ || SessionRestore::IsAnySessionCurrentlyRestoring()) {
    return;
  }

  RecordDiffMetric(kNormalHistogram, expected_normal_profiles_,
                   restored_normal_profiles_.size());
  RecordDiffMetric(kAppHistogram, expected_app_profiles_,
                   restored_app_profiles_.size());

  StopRecording();
}

void ProfileDiffAfterRestartRecorder::StopRecording() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (recording_) {
    SessionRestore::RemoveObserver(this);
  }

  recording_ = false;
  launches_finished_ = false;
  expected_normal_profiles_.reset();
  expected_app_profiles_.reset();
  restored_normal_profiles_.clear();
  restored_app_profiles_.clear();
}
