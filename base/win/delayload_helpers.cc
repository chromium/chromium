// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/delayload_helpers.h"

namespace base::win {

namespace {

thread_local bool g_suppress_delay_load_failure = false;

}  // namespace

bool IsDelayLoadFailureSuppressed() {
  return g_suppress_delay_load_failure;
}

namespace delayload_internal {

ScopedSuppressDelayLoadFailure::ScopedSuppressDelayLoadFailure()
    : previous_value_(g_suppress_delay_load_failure) {
  g_suppress_delay_load_failure = true;
}

ScopedSuppressDelayLoadFailure::~ScopedSuppressDelayLoadFailure() {
  g_suppress_delay_load_failure = previous_value_;
}

}  // namespace delayload_internal

}  // namespace base::win
