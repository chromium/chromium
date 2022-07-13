// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_DISPATCHER_DISPATCHER_H_
#define BASE_ALLOCATOR_DISPATCHER_DISPATCHER_H_

#include "base/base_export.h"

namespace base::allocator::dispatcher {

void BASE_EXPORT InstallStandardAllocatorHooks();
void BASE_EXPORT RemoveStandardAllocatorHooksForTesting();

}  // namespace base::allocator::dispatcher

#endif  // BASE_ALLOCATOR_DISPATCHER_DISPATCHER_H_