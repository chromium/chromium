// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_POLICY_DSE_NTP_OVERRIDE_METRICS_REPORTER_H_
#define CHROME_BROWSER_EXTENSIONS_POLICY_DSE_NTP_OVERRIDE_METRICS_REPORTER_H_
#include "build/build_config.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC));

class Profile;

namespace extensions {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PolicyExtensionStatus)
enum class PolicyExtensionStatus {
  kDisabled = 0,
  kEnabled = 1,
  kMaxValue = kEnabled,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:PolicyExtensionStatus)

class PolicyDseNtpOverrideMetricsReporter {
 public:
  static void ReportMetrics(Profile* profile);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_POLICY_DSE_NTP_OVERRIDE_METRICS_REPORTER_H_
