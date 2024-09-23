// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_STARTUP_TIMESTAMPS_H_
#define CHROME_APP_STARTUP_TIMESTAMPS_H_

#include "base/time/time.h"
#include "build/build_config.h"

struct StartupTimestamps {
  // Time at which chrome_exe was entered (recorded as early as possible in
  // main()).
  base::TimeTicks exe_entry_point_ticks;
#if BUILDFLAG(IS_WIN)
  // Time at which base::PreReadFile(chrome.dll) was called/returned.
  base::TimeTicks preread_begin_ticks;
  base::TimeTicks preread_end_ticks;
#endif
};

#endif  // CHROME_APP_STARTUP_TIMESTAMPS_H_
