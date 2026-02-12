// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_type.h"

#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "base/threading/platform_thread.h"

namespace base {

std::string_view ThreadTypeToString(ThreadType type) {
  switch (type) {
    case ThreadType::kBackground:
      return "kBackground";
    case ThreadType::kUtility:
      return "kUtility";
    case ThreadType::kDefault:
      return "kDefault";
    case ThreadType::kPresentation:
      return "kPresentation";
    case ThreadType::kAudioProcessing:
      return "kAudioProcessing";
    case ThreadType::kRealtimeAudio:
      return "kRealtimeAudio";
  }
  NOTREACHED();
}

namespace internal {

namespace {

constinit thread_local std::optional<ThreadType>
    current_task_importance_override = std::nullopt;

}  // namespace

CurrentTaskImportanceOverride::CurrentTaskImportanceOverride(
    ThreadType override) {
  previous_override_ = std::exchange(
      current_task_importance_override,
      std::min(override, PlatformThreadBase::GetCurrentThreadType()));
}

CurrentTaskImportanceOverride::~CurrentTaskImportanceOverride() {
  current_task_importance_override = previous_override_;
}

ThreadType GetCurrentTaskImportance() {
  if (current_task_importance_override) {
    return *current_task_importance_override;
  }
  return PlatformThread::PlatformThreadBase::GetCurrentThreadType();
}

}  // namespace internal
}  // namespace base
