// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/call_with_eh_frame.h"

#include <stdint.h>
#include <unwind.h>

namespace base::apple {

#if defined(__x86_64__) || defined(__aarch64__)
extern "C" _Unwind_Reason_Code __gxx_personality_v0(int,
                                                    _Unwind_Action,
                                                    uint64_t,
                                                    struct _Unwind_Exception*,
                                                    struct _Unwind_Context*);

_Unwind_Reason_Code CxxPersonalityRoutine(
    int version,
    _Unwind_Action actions,
    uint64_t exception_class,
    struct _Unwind_Exception* exception_object,
    struct _Unwind_Context* context) {
  // Unwinding is a two-phase process: phase one searches for an exception
  // handler, and phase two performs cleanup. For phase one, this custom
  // personality will terminate the search. For phase two, this should delegate
  // back to the standard personality routine.

  if ((actions & _UA_SEARCH_PHASE) != 0) {
    // Tell libunwind that this is the end of the stack. When it encounters the
    // CallWithEHFrame, it will stop searching for an exception handler. The
    // result is that no exception handler has been found higher on the stack,
    // and any that are lower on the stack (e.g. in CFRunLoopRunSpecific), will
    // now be skipped. Since this is reporting the end of the stack, and no
    // exception handler will have been found, std::terminate() will be called.
    return _URC_END_OF_STACK;
  }

  return __gxx_personality_v0(version, actions, exception_class,
                              exception_object, context);
}
#else   // !defined(__x86_64__) && !defined(__aarch64__)
// No implementation exists, so just call the block directly.
void CallWithEHFrame(void (^block)(void)) {
  block();
}
#endif  // defined(__x86_64__) || defined(__aarch64__)
}  // namespace base::apple
