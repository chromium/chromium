// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_DEADLINE_POLICY_H_
#define CC_LAYERS_DEADLINE_POLICY_H_

#include <cstdint>

#include "base/logging.h"
#include "base/optional.h"
#include "cc/cc_export.h"

namespace cc {

class CC_EXPORT DeadlinePolicy {
 public:
  enum Type {
    kUseExistingDeadline,
    kUseDefaultDeadline,
    kUseSpecifiedDeadline,
    kUseInfiniteDeadline
  };

  static DeadlinePolicy UseExistingDeadline();

  static DeadlinePolicy UseDefaultDeadline();

  static DeadlinePolicy UseSpecifiedDeadline(uint32_t deadline_in_frames);

  static DeadlinePolicy UseInfiniteDeadline();

  DeadlinePolicy(const DeadlinePolicy& other);

  DeadlinePolicy& operator=(const DeadlinePolicy& other) = default;

  ~DeadlinePolicy() = default;

  bool use_existing_deadline() const {
    return policy_type_ == DeadlinePolicy::kUseExistingDeadline;
  }

  base::Optional<uint32_t> deadline_in_frames() const {
    DCHECK(policy_type_ == Type::kUseDefaultDeadline ||
           policy_type_ == Type::kUseSpecifiedDeadline ||
           policy_type_ == Type::kUseInfiniteDeadline);
    return deadline_in_frames_;
  }

  Type policy_type() const { return policy_type_; }

  bool operator==(const DeadlinePolicy& other) const {
    return other.policy_type_ == policy_type_ &&
           other.deadline_in_frames_ == deadline_in_frames_;
  }

  bool operator!=(const DeadlinePolicy& other) const {
    return !(*this == other);
  }

 private:
  explicit DeadlinePolicy(
      Type policy_type,
      base::Optional<uint32_t> deadline_in_frames = base::nullopt);

  Type policy_type_;
  base::Optional<uint32_t> deadline_in_frames_;
};

}  // namespace cc

#endif  // CC_LAYERS_DEADLINE_POLICY_H_
