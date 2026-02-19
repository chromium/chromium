// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/swift_shims/user_metrics.h"

#include "base/metrics/user_metrics.h"

namespace base {
namespace swift {

void RecordUserMetricsAction(const std::string& action) {
  // tools/metrics/actions/extract_actions.py doesn't recognize this format. You
  // have to manually add a new action item in actions.xml.
  base::RecordComputedAction(action);
}

}  // namespace swift
}  // namespace base
