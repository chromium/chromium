// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/time_domain.h"

#include <optional>

#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/threading/thread_checker.h"

namespace base {
namespace sequence_manager {

void TimeDomain::NotifyPolicyChanged() {
  sequence_manager_->ScheduleWork();
}

void TimeDomain::OnAssignedToSequenceManager(
    internal::SequenceManagerImpl* sequence_manager) {
  DCHECK(sequence_manager);
  sequence_manager_ = sequence_manager;
}

Value::Dict TimeDomain::AsValue() const {
  Value::Dict state;
  state.Set("name", GetName());
  return state;
}

}  // namespace sequence_manager
}  // namespace base
