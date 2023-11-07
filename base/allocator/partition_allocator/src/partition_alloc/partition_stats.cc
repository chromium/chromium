// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_stats.h"

#include <cstring>

namespace partition_alloc {

SimplePartitionStatsDumper::SimplePartitionStatsDumper() {
  memset(&stats_, 0, sizeof(stats_));
}

void SimplePartitionStatsDumper::PartitionDumpTotals(
    const char* partition_name,
    const PartitionMemoryStats* memory_stats) {
  stats_ = *memory_stats;
}

}  // namespace partition_alloc
