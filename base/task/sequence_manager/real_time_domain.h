// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_REAL_TIME_DOMAIN_H_
#define BASE_TASK_SEQUENCE_MANAGER_REAL_TIME_DOMAIN_H_

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequence_manager/time_domain.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {
namespace internal {

class BASE_EXPORT RealTimeDomain : public TimeDomain {
 public:
  explicit RealTimeDomain(const base::TickClock* clock);
  RealTimeDomain(const RealTimeDomain&) = delete;
  RealTimeDomain& operator=(const RealTimeDomain&) = delete;
  ~RealTimeDomain() override = default;

  // TickClock implementation:
  TimeTicks NowTicks() const override;

  // TimeDomain implementation:
  base::TimeTicks GetNextDelayedTaskTime(
      WakeUp next_wake_up,
      sequence_manager::LazyNow* lazy_now) const override;
  bool MaybeFastForwardToWakeUp(absl::optional<WakeUp> next_wake_up,
                                bool quit_when_idle_requested) override;

 protected:
  const char* GetName() const override;

 private:
  raw_ptr<const TickClock> tick_clock_ = nullptr;
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_REAL_TIME_DOMAIN_H_
