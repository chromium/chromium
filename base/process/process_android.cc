// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include "base/notreached.h"
#include "base/process/internal_linux.h"

namespace base {

// static
bool Process::CanBackgroundProcesses() {
  return false;
}

bool Process::IsProcessBackgrounded() const {
  // See SetProcessBackgrounded().
  DCHECK(IsValid());
  return false;
}

bool Process::SetProcessBackgrounded(bool value) {
  // Not implemented for POSIX systems other than Linux and Mac. With POSIX, if
  // we were to lower the process priority we wouldn't be able to raise it back
  // to its initial priority.
  NOTIMPLEMENTED();
  return false;
}

Time Process::CreationTime() const {
  // On Android, /proc is mounted (on recent-enough versions) with hidepid=2,
  // which hides other PIDs in /proc. This means that only /proc/self is
  // accessible. Instead of trying (and failing) to read the file, don't attempt
  // to read it. This also provides consistency across releases.
  int64_t start_ticks = is_current()
                            ? internal::ReadProcSelfStatsAndGetFieldAsInt64(
                                  internal::VM_STARTTIME)
                            : 0;

  if (!start_ticks)
    return Time();

  TimeDelta start_offset = internal::ClockTicksToTimeDelta(start_ticks);
  Time boot_time = internal::GetBootTime();
  if (boot_time.is_null())
    return Time();
  return Time(boot_time + start_offset);
}

}  // namespace base
