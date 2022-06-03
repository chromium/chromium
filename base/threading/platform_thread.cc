// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <atomic>
#include <memory>
#include <ostream>

#include "base/feature_list.h"
#include "base/time/time.h"

namespace base {

namespace {

// Whether thread priorities should be used. When disabled,
// PlatformThread::SetCurrentThreadPriority() no-ops.
const Feature kThreadPrioritiesFeature{"ThreadPriorities",
                                       FEATURE_ENABLED_BY_DEFAULT};

// Whether thread priorities should be used.
//
// PlatformThread::SetCurrentThreadPriority() doesn't query the state of the
// feature directly because FeatureList initialization is not always
// synchronized with PlatformThread::SetCurrentThreadPriority().
std::atomic<bool> g_use_thread_priorities(true);

}  // namespace

std::ostream& operator<<(std::ostream& os, const PlatformThreadRef& ref) {
  os << ref.id_;
  return os;
}

// static
void PlatformThread::SetCurrentThreadPriority(ThreadPriority priority) {
  if (g_use_thread_priorities.load())
    SetCurrentThreadPriorityImpl(priority);
}

TimeDelta PlatformThread::GetRealtimePeriod(Delegate* delegate) {
  if (g_use_thread_priorities.load())
    return delegate->GetRealtimePeriod();
  return TimeDelta();
}

TimeDelta PlatformThread::Delegate::GetRealtimePeriod() {
  return TimeDelta();
}

namespace internal {

void InitializeThreadPrioritiesFeature() {
  // A DCHECK is triggered on FeatureList initialization if the state of a
  // feature has been checked before. To avoid triggering this DCHECK in unit
  // tests that call this before initializing the FeatureList, only check the
  // state of the feature if the FeatureList is initialized.
  if (FeatureList::GetInstance() &&
      !FeatureList::IsEnabled(kThreadPrioritiesFeature)) {
    g_use_thread_priorities.store(false);
  }

#if defined(OS_APPLE)
  PlatformThread::InitializeOptimizedRealtimeThreadingFeature();
#endif
}

}  // namespace internal

}  // namespace base
