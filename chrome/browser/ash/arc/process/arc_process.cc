// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/process/arc_process.h"

#include <utility>

#include "ash/components/arc/arc_features.h"
#include "ash/components/arc/mojom/process.mojom.h"
#include "base/containers/fixed_flat_set.h"
#include "base/strings/string_util.h"

namespace arc {

using mojom::ProcessState;

namespace {

bool IsImportantState(ProcessState state) {
  switch (state) {
    case ProcessState::IMPORTANT_FOREGROUND:
    case ProcessState::BOUND_FOREGROUND_SERVICE:
    case ProcessState::FOREGROUND_SERVICE:
    case ProcessState::TOP:
    case ProcessState::PERSISTENT_UI:
    case ProcessState::PERSISTENT:
      return true;
    default:
      return false;
  }
}

bool IsPersistentState(ProcessState state) {
  switch (state) {
    case ProcessState::PERSISTENT_UI:
    case ProcessState::PERSISTENT:
      return true;
    default:
      return false;
  }
}

bool IsProtectedBackgroundState(ProcessState state) {
  switch (state) {
    case ProcessState::TOP:
    case ProcessState::FOREGROUND_SERVICE:
    case ProcessState::BOUND_FOREGROUND_SERVICE:
    case ProcessState::IMPORTANT_FOREGROUND:
    case ProcessState::IMPORTANT_BACKGROUND:
      return true;
    default:
      return false;
  }
}

bool IsCachedState(ProcessState state) {
  switch (state) {
    case ProcessState::CACHED_ACTIVITY_CLIENT:
    case ProcessState::CACHED_RECENT:
    case ProcessState::CACHED_EMPTY:
    case ProcessState::NONEXISTENT:
      return true;
    default:
      return false;
  }
}

}  // namespace

ArcProcess::ArcProcess(base::ProcessId nspid,
                       base::ProcessId pid,
                       const std::string& process_name,
                       mojom::ProcessState process_state,
                       bool is_focused,
                       int64_t last_activity_time)
    : nspid_(nspid),
      pid_(pid),
      process_name_(process_name),
      process_state_(process_state),
      is_focused_(is_focused),
      last_activity_time_(last_activity_time) {}

ArcProcess::~ArcProcess() = default;

// Sort by (process_state, last_activity_time) pair.
// Smaller process_state value means higher priority as defined in Android.
// Larger last_activity_time means more recently used.
bool ArcProcess::operator<(const ArcProcess& rhs) const {
  return std::make_pair(process_state(), -last_activity_time()) <
         std::make_pair(rhs.process_state(), -rhs.last_activity_time());
}

ArcProcess::ArcProcess(ArcProcess&& other) = default;
ArcProcess& ArcProcess::operator=(ArcProcess&& other) = default;

bool ArcProcess::IsImportant() const {
  return IsImportantState(process_state()) || IsArcProtected() ||
         IsGmsCoreProtected();
}

bool ArcProcess::IsPersistent() const {
  // Protect PERSISTENT, PERSISTENT_UI, our HOME and custom set of ARC processes
  // since they should have lower priority to be killed.
  return IsPersistentState(process_state()) || IsArcProtected() ||
         IsGmsCoreProtected();
}

bool ArcProcess::IsCached() const {
  return IsCachedState(process_state());
}

bool ArcProcess::IsBackgroundProtected() const {
  return IsProtectedBackgroundState(process_state());
}

bool ArcProcess::IsArcProtected() const {
  constexpr auto kSet = base::MakeFixedFlatSet<std::string_view>(
      {"com.google.android.apps.work.clouddpc.arc"});

  return kSet.contains(process_name());
}

bool ArcProcess::IsGmsCoreProtected() const {
  constexpr auto kSet = base::MakeFixedFlatSet<std::string_view>({
      "com.google.process.gservices",
      "com.google.android.gms",
      "com.google.android.gms.persistent",
      "com.google.android.gms.unstable",
  });

  return kSet.contains(process_name());
}

std::ostream& operator<<(std::ostream& out, const ArcProcess& arc_process) {
  out << "process_name: " << arc_process.process_name()
      << ", pid: " << arc_process.pid()
      << ", process_state: " << arc_process.process_state()
      << ", is_focused: " << arc_process.is_focused()
      << ", last_activity_time: " << arc_process.last_activity_time()
      << ", packages: " << base::JoinString(arc_process.packages(), ",");
  return out;
}

}  // namespace arc
