// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <__cxxabi_config.h>

extern "C"
// LLVM's demangler is large, and we have no need of it.  Overriding it with
// our own stub version here stops a lot of code being pulled in from libc++.
// More here:
//   https://llvm.org/svn/llvm-project/libcxxabi/trunk/src/cxa_demangle.cpp
_LIBCXXABI_FUNC_VIS
// This is a weak symbol to let android_crazy_linker override it in
// //base/android/linker:chromium_android_linker.
_LIBCXXABI_WEAK char* __cxa_demangle(const char* mangled_name,
                                     char* buf,
                                     size_t* n,
                                     int* status) {
  static const int kMemoryAllocFailure = -1;  // LLVM's memory_alloc_failure.
  if (status)
    *status = kMemoryAllocFailure;
  return nullptr;
}
