// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_id_helper.h"
#include "base/atomic_sequence_num.h"
#include "base/rand_util.h"

namespace base {
namespace trace_event {

uint64_t GetNextGlobalTraceId() {
  static const uint64_t kPerProcessRandomValue = base::RandUint64();
  static base::AtomicSequenceNumber counter;
  return kPerProcessRandomValue ^ static_cast<uint64_t>(counter.GetNext());
}

}  // namespace trace_event
}  // namespace base
