// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/log.h>
#include <unistd.h>
#include <cstddef>
#include <cstdlib>

// Custom implementation of new and delete, this prevents dragging
// the libc++ implementation, which drags exception-related machine
// code that is not needed here. This helps reduce the size of the
// final binary considerably.

// These symbols are not exported, thus this does not affect the libraries that
// it will load, only the linker binary itself.
void* operator new(size_t size) {
  void* ptr = ::malloc(size);
  if (ptr != nullptr)
    return ptr;

  // Don't assume it is possible to call any C library function like
  // snprintf() here, since it might allocate heap memory and crash at
  // runtime. Hence our fatal message does not contain the number of
  // bytes requested by the allocation.
  static const char kFatalMessage[] = "Out of memory!";
#ifdef __ANDROID__
  __android_log_write(ANDROID_LOG_FATAL, "linker", kFatalMessage);
#else
  ::write(STDERR_FILENO, kFatalMessage, sizeof(kFatalMessage) - 1);
#endif
  _exit(1);
#if defined(__GNUC__)
  __builtin_unreachable();
#endif

  // Adding a 'return nullptr' here will make the compiler error with a message
  // stating that 'operator new(size_t)' is not allowed to return nullptr.
  //
  // Indeed, an new expression like 'new T' shall never return nullptr,
  // according to the C++ specification, and an optimizing compiler will gladly
  // remove any null-checks after them (something the Fuschsia team had to
  // learn the hard way when writing their kernel in C++). What is meant here
  // is something like:
  //
  //   Foo* foo = new Foo(10);
  //   if (!foo) {                             <-- entire check and branch
  //      ... Handle out-of-memory condition.  <-- removed by an optimizing
  //   }                                       <-- compiler.
  //
  // Note that some C++ library implementations (e.g. recent libc++) recognize
  // when they are compiled with -fno-exceptions and provide a simpler version
  // of operator new that can return nullptr. However, it is very hard to
  // guarantee at build time that this code is linked against such a version
  // of the runtime. Moreoever, technically disabling exceptions is completely
  // out-of-spec regarding the C++ language, and what the compiler is allowed
  // to do in this case is mostly implementation-defined, so better be safe
  // than sorry here.
  //
  // C++ provides a non-throwing new expression that can return a nullptr
  // value, but it must be written as 'new (std::nothrow) T' instead of
  // 'new T', and thus nobody uses this. This ends up calling
  // 'operator new(size_t, const std::nothrow_t&)' which is not implemented
  // here.
}

void* operator new[](size_t size) {
  return operator new(size);
}

void operator delete(void* ptr) {
  // The compiler-generated code already checked that |ptr != nullptr|
  // so don't to it a second time.
  ::free(ptr);
}

void operator delete[](void* ptr) {
  ::free(ptr);
}
