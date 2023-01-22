// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"

#include <dlfcn.h>

static void* gRecordReplayValueFn;

static uintptr_t RecordReplayValue(const char* why, uintptr_t v) {
  if (!gRecordReplayValueFn) {
    void* fnptr = dlsym(RTLD_DEFAULT, "RecordReplayValue");
    if (!fnptr) {
      gRecordReplayValueFn = reinterpret_cast<void*>(1);
      return v;
    }
    gRecordReplayValueFn = fnptr;
  }

  if (gRecordReplayValueFn != reinterpret_cast<void*>(1)) {
    return reinterpret_cast<uintptr_t(*)(const char*, uintptr_t)>(gRecordReplayValueFn)(why, v);
  }
  return v;
}

namespace base {

ScopedClosureRunner::ScopedClosureRunner() = default;

ScopedClosureRunner::ScopedClosureRunner(OnceClosure closure)
    : closure_(std::move(closure)) {}

ScopedClosureRunner::ScopedClosureRunner(ScopedClosureRunner&& other)
    : closure_(other.Release()) {}

ScopedClosureRunner& ScopedClosureRunner::operator=(
    ScopedClosureRunner&& other) {
  if (this != &other) {
    RunAndReset();
    ReplaceClosure(other.Release());
  }
  return *this;
}

ScopedClosureRunner::~ScopedClosureRunner() {
  RunAndReset();
}

void ScopedClosureRunner::RunAndReset() {
  if (closure_)
    std::move(closure_).Run();
}

void ScopedClosureRunner::ReplaceClosure(OnceClosure closure) {
  closure_ = std::move(closure);
}

OnceClosure ScopedClosureRunner::Release() {
  return std::move(closure_);
}

uintptr_t CallbackRecordReplayValue(const char* why, uintptr_t value) {
  return RecordReplayValue(why, value);
}

}  // namespace base
