// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/deadline_policy.h"

#include <limits>

#include "base/notreached.h"
#include "base/strings/stringprintf.h"

namespace cc {
namespace {

const char* PolicyToString(DeadlinePolicy::Type type) {
  switch (type) {
    case DeadlinePolicy::Type::kUseExistingDeadline:
      return "UseExistingDeadline";
    case DeadlinePolicy::Type::kUseDefaultDeadline:
      return "UseDefaultDeadline";
    case DeadlinePolicy::Type::kUseSpecifiedDeadline:
      return "UseSpecifiedDeadline";
    case DeadlinePolicy::Type::kUseInfiniteDeadline:
      return "UseInfiniteDeadline";
  }
  NOTREACHED();
}

}  // namespace

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
                               std::optional<uint32_t> deadline_in_frames)
    : policy_type_(policy_type), deadline_in_frames_(deadline_in_frames) {}

DeadlinePolicy::DeadlinePolicy(const DeadlinePolicy& other) = default;

std::string DeadlinePolicy::ToString() const {
  return base::StringPrintf("DeadlinePolicy(%s, %d)",
                            PolicyToString(policy_type_),
                            deadline_in_frames_.value_or(-1));
}

}  // namespace cc
