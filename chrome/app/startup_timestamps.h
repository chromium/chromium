// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_STARTUP_TIMESTAMPS_H_
#define CHROME_APP_STARTUP_TIMESTAMPS_H_

#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <variant>

#include "components/startup_metric_utils/common/preread_experiment_group.h"
#endif

struct StartupTimestamps {
#if BUILDFLAG(IS_WIN)
  // Time at which the executable was entered (recorded as early as possible in
  // main()). chrome.exe passes a QPC-derived `base::TimeDelta` (microseconds
  // since the QPC epoch via `base::TimeDelta::FromQPCValue`) across the DLL
  // boundary to be resolved into chrome.dll's `TimeTicks` clock domain;
  // in-process callers that already have a `base::TimeTicks` pass that
  // directly.
  std::variant<base::TimeTicks, base::TimeDelta> exe_entry_point_ticks;

  // Time at which base::PreReadFile(chrome.dll) was called/returned.
  base::TimeTicks preread_begin_ticks;
  base::TimeTicks preread_end_ticks;
  startup_metric_utils::PrereadExperimentGroup preread_group =
      startup_metric_utils::PrereadExperimentGroup::kUnknown;
#else
  // Time at which chrome_exe was entered (recorded as early as possible in
  // main()).
  base::TimeTicks exe_entry_point_ticks;
#endif
};

#endif  // CHROME_APP_STARTUP_TIMESTAMPS_H_
