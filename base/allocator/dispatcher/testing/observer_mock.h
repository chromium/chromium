// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_DISPATCHER_TESTING_OBSERVER_MOCK_H_
#define BASE_ALLOCATOR_DISPATCHER_TESTING_OBSERVER_MOCK_H_

#include "base/allocator/dispatcher/subsystem.h"
#include "testing/gmock/include/gmock/gmock.h"

#include <cstddef>

namespace base::allocator::dispatcher::testing {

// ObserverMock is a small mock class based on GoogleMock.
// It complies to the interface enforced by the dispatcher. The template
// parameter serves only to create distinct types of observers if required.
template <typename T = void>
struct ObserverMock {
  MOCK_METHOD(void,
              OnAllocation,
              (void* address,
               size_t size,
               AllocationSubsystem sub_system,
               const char* type_name),
              ());
  MOCK_METHOD(void, OnFree, (void* address), ());
};

}  // namespace base::allocator::dispatcher::testing

#endif  // BASE_ALLOCATOR_DISPATCHER_TESTING_OBSERVER_MOCK_H_