// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_FRAME_H_
#define BASE_PROFILER_FRAME_H_

#include "base/base_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/profiler/module_cache.h"

namespace base {

// Frame represents an individual sampled stack frame with full module
// information.
struct BASE_EXPORT Frame {
  Frame(uintptr_t instruction_pointer, const ModuleCache::Module* module);

  // TODO(crbug.com/40241229): For prototype use by Android arm browser main
  // thread profiling for tracing only. Update once we have a full design
  // for function name upload.
  Frame(uintptr_t instruction_pointer,
        const ModuleCache::Module* module,
        std::string function_name);
  ~Frame();

  // The sampled instruction pointer within the function.
  uintptr_t instruction_pointer;

  // The module information.
  // `module` is not a raw_ptr<...> because it is used with gmock Field() that
  // expects a raw pointer in V8UnwinderTest.UnwindThroughV8Frames.
  RAW_PTR_EXCLUSION const ModuleCache::Module* module;

  // This serves as a temporary way to pass function names from libunwindstack
  // unwinder to tracing profiler. Not used by any other unwinder.
  // TODO(crbug.com/40241229): For prototype use by Android arm browser main
  // thread profiling for tracing only. Update once we have a full design
  // for function name upload.
  std::string function_name;
};

}  // namespace base

#endif  // BASE_PROFILER_FRAME_H_
