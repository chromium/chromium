// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_stats.h"

#include <cstring>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"

namespace partition_alloc {

SimplePartitionStatsDumper::SimplePartitionStatsDumper() {
  PA_UNSAFE_TODO(memset(&stats_, 0, sizeof(stats_)));
}

void SimplePartitionStatsDumper::PartitionDumpTotals(
    const char* partition_name,
    const PartitionMemoryStats* memory_stats) {
  stats_ = *memory_stats;
}

}  // namespace partition_alloc
