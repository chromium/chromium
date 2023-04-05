// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_DISPATCHER_STANDARD_HOOKS_H_
#define BASE_ALLOCATOR_DISPATCHER_STANDARD_HOOKS_H_

// This file and its cc file contain the standard allocation hooks and auxiliary
// functions. These are intended to be replaced by the new dispatcher mechanism
// in /base/allocator/dispatcher.

#include "base/allocator/buildflags.h"
#include "base/base_export.h"

namespace base::allocator::dispatcher {

#if !BUILDFLAG(USE_ALLOCATION_EVENT_DISPATCHER)
// Install the standard allocation hooks which forward allocation events to the
// PoissonAllocationSampler.
void BASE_EXPORT InstallStandardAllocatorHooks();
#endif

}  // namespace base::allocator::dispatcher

#endif  // BASE_ALLOCATOR_DISPATCHER_STANDARD_HOOKS_H_
