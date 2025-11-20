// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/allocator_shim_functions.h"
#include "partition_alloc/shim/checked_multiply_win.h"

// Cross-checks.
#if !defined(COMPONENT_BUILD) || !PA_BUILDFLAG(IS_WIN)
#error This code is only for Windows component build.
#endif
