// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TEST_MOCK_TIME_DOMAIN_H_
#define BASE_TASK_SEQUENCE_MANAGER_TEST_MOCK_TIME_DOMAIN_H_

#include "base/task/sequence_manager/time_domain.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {

// TimeDomain with a mock clock and not invoking SequenceManager.
// NOTE: All methods are main thread only.
class MockTimeDomain : public TimeDomain {
 public:
  explicit MockTimeDomain(TimeTicks initial_now_ticks);
  MockTimeDomain(const MockTimeDomain&) = delete;
  MockTimeDomain& operator=(const MockTimeDomain&) = delete;
  ~MockTimeDomain() override;

  void SetNowTicks(TimeTicks now_ticks);

  // TimeDomain implementation:
  LazyNow CreateLazyNow() const override;
  TimeTicks Now() const override;
  absl::optional<TimeDelta> DelayTillNextTask(LazyNow* lazy_now) override;
  void SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time) override;
  bool MaybeFastForwardToNextTask(bool quit_when_idle_requested) override;
  const char* GetName() const override;

 private:
  TimeTicks now_ticks_;
};

}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TEST_MOCK_TIME_DOMAIN_H_
