// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/record_replay.h"

#include <stdarg.h>

namespace base {
namespace recordreplay {

extern "C" void V8RecordReplayAssertVA(const char* format, va_list args);

void Assert(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  V8RecordReplayAssertVA(format, ap);
  va_end(ap);
}

} // namespace recordreplay
} // namespace base
