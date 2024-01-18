// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/debug/stack_trace.h"

#include <unistd.h>
#include <unwind.h>

#include <cstring>

#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/strings/safe_sprintf.h"

namespace partition_alloc::internal::base::debug {

namespace {

struct StackCrawlState {
  StackCrawlState(uintptr_t* frames, size_t max_depth)
      : frames(frames),
        frame_count(0),
        max_depth(max_depth),
        have_skipped_self(false) {}

  uintptr_t* frames;
  size_t frame_count;
  size_t max_depth;
  bool have_skipped_self;
};

_Unwind_Reason_Code TraceStackFrame(_Unwind_Context* context, void* arg) {
  StackCrawlState* state = static_cast<StackCrawlState*>(arg);
  uintptr_t ip = _Unwind_GetIP(context);

  // The first stack frame is this function itself.  Skip it.
  if (ip != 0 && !state->have_skipped_self) {
    state->have_skipped_self = true;
    return _URC_NO_REASON;
  }

  state->frames[state->frame_count++] = ip;
  if (state->frame_count >= state->max_depth) {
    return _URC_END_OF_STACK;
  }
  return _URC_NO_REASON;
}

}  // namespace

size_t CollectStackTrace(const void** trace, size_t count) {
  StackCrawlState state(reinterpret_cast<uintptr_t*>(trace), count);
  _Unwind_Backtrace(&TraceStackFrame, &state);
  return state.frame_count;
}

void OutputStackTrace(unsigned index,
                      uintptr_t address,
                      uintptr_t base_address,
                      const char* module_name,
                      uintptr_t offset) {
  size_t module_name_len = strlen(module_name);

  char buffer[256];
  if (module_name_len > 4 &&
      !strcmp(module_name + module_name_len - 4, ".apk")) {
    strings::SafeSPrintf(buffer, "#%02d pc 0x%0x %s (offset 0x%0x)\n", index,
                         address - base_address, module_name, offset);
  } else {
    strings::SafeSPrintf(buffer, "#%02d pc 0x%0x %s\n", index,
                         address - base_address, module_name);
  }
  PA_RAW_LOG(INFO, buffer);
}

}  // namespace partition_alloc::internal::base::debug
