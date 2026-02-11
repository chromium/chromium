// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_type.h"

#include <string_view>

#include "base/notreached.h"

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

}  // namespace base
