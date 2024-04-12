// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_CPU_FREQUENCY_UTILS_H_
#define BASE_POWER_MONITOR_CPU_FREQUENCY_UTILS_H_

#include "base/base_export.h"

namespace base {

// Returns the estimated CPU frequency by executing a tight loop of predictable
// assembly instructions. The estimated frequency should be proportional and
// about the same magnitude than the real CPU frequency. The measurement should
// be long enough to avoid Turbo Boost effect (~3ms) and be low enough to stay
// within the operating system scheduler quantum (~100ms).
// The return value is the estimated CPU frequency, in Hz.
BASE_EXPORT double EstimateCpuFrequency();

// These functions map to fields in the win32 PROCESSOR_POWER_INFORMATION struct
// They are currently only implemented on Windows since that's the platform
// being investigated, but they could be replaced with something more
// generic/cross-platform as needed.
// Return the maximum frequency or the frequency limit of the fastest logical
// CPU. This means that on a big/little system, the little cores will never be
// captured by these functions.
BASE_EXPORT unsigned long GetCpuMaxMhz();
BASE_EXPORT unsigned long GetCpuMhzLimit();

}  // namespace base

#endif  // BASE_POWER_MONITOR_CPU_FREQUENCY_UTILS_H_
