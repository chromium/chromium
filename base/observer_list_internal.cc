// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list_internal.h"

#include <memory>

namespace base::internal {

#if DCHECK_IS_ON()
CheckedObserverAdapter::CheckedObserverAdapter(const CheckedObserver* observer,
                                               bool collect_stack)
    : weak_ptr_(observer->factory_.GetWeakPtr()) {
  if (collect_stack) {
    stack_ = std::make_unique<base::debug::StackTrace>();
  }
}
#else
CheckedObserverAdapter::CheckedObserverAdapter(const CheckedObserver* observer)
    : weak_ptr_(observer->factory_.GetWeakPtr()) {}
#endif  // DCHECK_IS_ON()

CheckedObserverAdapter::CheckedObserverAdapter(CheckedObserverAdapter&& other) =
    default;
CheckedObserverAdapter& CheckedObserverAdapter::operator=(
    CheckedObserverAdapter&& other) = default;
CheckedObserverAdapter::~CheckedObserverAdapter() = default;

}  // namespace base::internal
