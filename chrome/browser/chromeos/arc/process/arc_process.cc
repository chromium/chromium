// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/process/arc_process.h"

#include <unordered_set>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace arc {

using mojom::ProcessState;

constexpr char kCloudDpcrocessName[] =
    "com.google.android.apps.work.clouddpc.arc";

const std::unordered_set<ProcessState>& ImportantStates() {
  static const base::NoDestructor<std::unordered_set<ProcessState>>
      kImportantStates({ProcessState::IMPORTANT_FOREGROUND,
                        ProcessState::BOUND_FOREGROUND_SERVICE,
                        ProcessState::FOREGROUND_SERVICE, ProcessState::TOP,
                        ProcessState::PERSISTENT_UI, ProcessState::PERSISTENT});
  return *kImportantStates;
}

const std::unordered_set<ProcessState>& PersistentStates() {
  static const base::NoDestructor<std::unordered_set<ProcessState>>
      kPersistentStates(
          {ProcessState::PERSISTENT_UI, ProcessState::PERSISTENT});
  return *kPersistentStates;
}


const std::unordered_set<ProcessState>& ProtectedBackgroundStates() {
  static const base::NoDestructor<std::unordered_set<ProcessState>>
      kProtectedBackgroundStates({ProcessState::TOP,
                                  ProcessState::FOREGROUND_SERVICE,
                                  ProcessState::BOUND_FOREGROUND_SERVICE,
                                  ProcessState::IMPORTANT_FOREGROUND,
                                  ProcessState::IMPORTANT_BACKGROUND});
  return *kProtectedBackgroundStates;
}

const std::unordered_set<ProcessState>& BackgroundStates() {
  static const base::NoDestructor<std::unordered_set<ProcessState>>
      kBackgroundStates({ProcessState::TRANSIENT_BACKGROUND,
                         ProcessState::BACKUP, ProcessState::SERVICE,
                         ProcessState::RECEIVER, ProcessState::TOP_SLEEPING,
                         ProcessState::HEAVY_WEIGHT, ProcessState::HOME,
                         ProcessState::LAST_ACTIVITY,
                         ProcessState::CACHED_ACTIVITY});
  return *kBackgroundStates;
}

const std::unordered_set<ProcessState>& CachedStates() {
  static const base::NoDestructor<std::unordered_set<ProcessState>>
      kCachedStates({ProcessState::CACHED_ACTIVITY_CLIENT,
                     ProcessState::CACHED_RECENT, ProcessState::CACHED_EMPTY,
                     ProcessState::NONEXISTENT});
  return *kCachedStates;
}

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

// TODO(wvk): Use a simple switch/case instead of std::unordered_set lookup,
// it will likely be faster.
bool ArcProcess::IsImportant() const {
  return ImportantStates().count(process_state()) == 1 || IsArcProtected();
}

bool ArcProcess::IsPersistent() const {
  // Protect PERSISTENT, PERSISTENT_UI, our HOME and custom set of ARC processes
  // since they should have lower priority to be killed.
  return PersistentStates().count(process_state()) == 1 || IsArcProtected();
}

bool ArcProcess::IsCached() const {
  return CachedStates().count(process_state()) == 1;
}

bool ArcProcess::IsBackgroundProtected() const {
  return ProtectedBackgroundStates().count(process_state()) == 1;
}

bool ArcProcess::IsArcProtected() const {
  return process_name() == kCloudDpcrocessName;
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
