// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/deadline_policy.h"

#include <limits>

namespace cc {

// static
DeadlinePolicy DeadlinePolicy::UseExistingDeadline() {
  return DeadlinePolicy(Type::kUseExistingDeadline);
}

// static
DeadlinePolicy DeadlinePolicy::UseDefaultDeadline() {
  return DeadlinePolicy(Type::kUseDefaultDeadline);
}

// static
DeadlinePolicy DeadlinePolicy::UseSpecifiedDeadline(
    uint32_t deadline_in_frames) {
  return DeadlinePolicy(Type::kUseSpecifiedDeadline, deadline_in_frames);
}

// static
DeadlinePolicy DeadlinePolicy::UseInfiniteDeadline() {
  return DeadlinePolicy(Type::kUseInfiniteDeadline,
                        std::numeric_limits<uint32_t>::max());
}

DeadlinePolicy::DeadlinePolicy(Type policy_type,
                               absl::optional<uint32_t> deadline_in_frames)
    : policy_type_(policy_type), deadline_in_frames_(deadline_in_frames) {}

DeadlinePolicy::DeadlinePolicy(const DeadlinePolicy& other) = default;

}  // namespace cc
