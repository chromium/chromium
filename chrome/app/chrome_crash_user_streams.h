// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_CRASH_USER_STREAMS_H_
#define CHROME_APP_CHROME_CRASH_USER_STREAMS_H_

#include <stdint.h>

#include <vector>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/crash/core/common/shared_memory_user_stream_writer.h"

// Owns the shared memory minidump user streams that Chrome exposes to the
// Crashpad handler (currently only the SystemProfile stream). Owned by
// `ChromeCrashReporterClient`, so it lives in the module that starts Crashpad:
// `chrome_elf.dll` on Windows, the browser binary elsewhere.
//
// Not thread-safe: all calls must happen on the main thread.
class ChromeCrashUserStreams {
 public:
  ChromeCrashUserStreams();
  ~ChromeCrashUserStreams();

  ChromeCrashUserStreams(const ChromeCrashUserStreams&) = delete;
  ChromeCrashUserStreams& operator=(const ChromeCrashUserStreams&) = delete;

  // Returns read-only handles to the streams.
  std::vector<base::ReadOnlySharedMemoryRegion> GetSharedMemoryRegions() const;

  // Writes `data` into the stream with `user_stream_type`. `data` must not
  // exceed the stream's capacity (`CHECK`ed). Returns true if the write
  // succeeded, or false if there is no stream of that type.
  bool Write(uint32_t user_stream_type, base::span<const uint8_t> data);

 private:
  crash_reporter::SharedMemoryUserStreamWriter system_profile_stream_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_APP_CHROME_CRASH_USER_STREAMS_H_
