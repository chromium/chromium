// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"

#ifndef _WIN32
#include <dlfcn.h>
#else
#include <windows.h>
#endif

static void* LookupRecordReplaySymbol(const char* name) {
#ifndef _WIN32
  void* fnptr = dlsym(RTLD_DEFAULT, name);
#else
  HMODULE module = GetModuleHandleA("windows-recordreplay.dll");
  void* fnptr = module ? (void*)GetProcAddress(module, name) : nullptr;
#endif
  return fnptr ? fnptr : reinterpret_cast<void*>(1);
}

static uintptr_t RecordReplayValue(const char* why, uintptr_t v) {
  static void* fnptr;
  if (!fnptr) {
    fnptr = LookupRecordReplaySymbol("RecordReplayValue");
  }
  if (fnptr != reinterpret_cast<void*>(1)) {
    return reinterpret_cast<uintptr_t(*)(const char*, uintptr_t)>(fnptr)(why, v);
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
