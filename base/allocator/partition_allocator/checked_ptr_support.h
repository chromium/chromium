// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_

#include "base/partition_alloc_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(USE_BACKUP_REF_PTR) && !defined(OS_NACL)
#define ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR 1
#define DISABLE_REF_COUNT_IN_RENDERER 1
#else
#define ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR 0
#define DISABLE_REF_COUNT_IN_RENDERER 0
#endif

#if DISABLE_REF_COUNT_IN_RENDERER
static_assert(ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR,
              "DISABLE_REF_COUNT_IN_RENDERER can only by used if "
              "PartitionRefCount is enabled");
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_
