// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_

#define ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR 0
#define DISABLE_REF_COUNT_IN_RENDERER 0

#if DISABLE_REF_COUNT_IN_RENDERER
static_assert(ENABLE_REF_COUNT_FOR_BACKUP_REF_PTR,
              "DISABLE_REF_COUNT_IN_RENDERER can only by used if "
              "PartitionRefCount is enabled");
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_CHECKED_PTR_SUPPORT_H_
