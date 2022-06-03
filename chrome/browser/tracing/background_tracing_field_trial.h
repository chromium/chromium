// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRACING_BACKGROUND_TRACING_FIELD_TRIAL_H_
#define CHROME_BROWSER_TRACING_BACKGROUND_TRACING_FIELD_TRIAL_H_

namespace tracing {

enum class BackgroundTracingSetupMode {
  // Background tracing config comes from a field trial.
  kFromFieldTrial,

  // Background tracing config comes from a config file passed on the
  // command-line (for local testing).
  kFromConfigFile,

  // Background tracing is disabled due to invalid command-line flags.
  kDisabledInvalidCommandLine,
};

BackgroundTracingSetupMode GetBackgroundTracingSetupMode();

void SetupBackgroundTracingFieldTrial();

}  // namespace tracing

#endif  // CHROME_BROWSER_TRACING_BACKGROUND_TRACING_FIELD_TRIAL_H_
