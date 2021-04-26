// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_TAGGING_H_
#define BASE_MEMORY_TAGGING_H_

// This file contains method definitions to support Armv8.5-A's memory tagging
// extension.
#include "base/base_export.h"

namespace base {
namespace memory {
// Enum configures Arm's MTE extension to operate in different modes
enum class TagViolationReportingMode {
  // Default settings
  kUndefined,
  // Precise tag violation reports, higher overhead. Good for unittests
  // and security critical threads.
  kSynchronous,
  // Imprecise tag violation reports (async mode). Lower overhead.
  kAsynchronous,
};

// Changes the memory tagging mode for the calling thread.
BASE_EXPORT void ChangeMemoryTaggingModeForCurrentThread(
    TagViolationReportingMode);

// Gets the memory tagging mode for the calling thread.
BASE_EXPORT TagViolationReportingMode GetMemoryTaggingModeForCurrentThread();
}  // namespace memory
}  // namespace base

#endif  // BASE_MEMORY_TAGGING_H_
