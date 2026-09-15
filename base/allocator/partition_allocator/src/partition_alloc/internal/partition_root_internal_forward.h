// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_INTERNAL_FORWARD_H_
#define PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_INTERNAL_FORWARD_H_

#include "partition_alloc/partition_alloc_base/thread_annotations.h"

namespace partition_alloc {

class PartitionRoot;

namespace internal {

class PA_LOCKABLE Lock;

// Declare PartitionRootLock() for thread analysis. Its implementation
// is defined in partition_root_internal.h.
Lock& PartitionRootLock(PartitionRoot*);

}  // namespace internal

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_INTERNAL_FORWARD_H_
