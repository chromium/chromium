// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/record_replay.h"

#include <stdarg.h>

namespace recordreplay {

// V8 isn't linked to when building for nacl.
#ifdef NACL_TC_REV

static void V8RecordReplayAssertVA(const char* format, va_list args) {}

#else // !NACL_TC_REV

extern "C" void V8RecordReplayAssertVA(const char* format, va_list args);

#endif // !NACL_TC_REV

void Assert(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  V8RecordReplayAssertVA(format, ap);
  va_end(ap);
}

} // namespace recordreplay
