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
  kBlocked = 2,
  kUserInstalledEnabled = 3,
  kUserInstalledDisabled = 4,
  kMaxValue = kUserInstalledDisabled,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/extensions/enums.xml:PolicyExtensionStatus)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LowTrustPolicyBlockAction)
enum class LowTrustPolicyBlockAction {
  kBlockedOnInstall = 0,
  kSkippedDownload = 1,
  kBlockedPolicyTakeoverOfUserInstall = 2,
  // Logged when an existing policy-installed extension is uninstalled because
  // low-trust blocking is enforced (either on initial enforcement rollout on a
  // low-trust device or after device trust loss).
  kUninstalledOnTrustLoss = 3,
  kCacheClearedOnPolicyRemoval = 4,
  kCacheClearedOnTtlExpiration = 5,
  kMaxValue = kCacheClearedOnTtlExpiration,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/extensions/enums.xml:LowTrustPolicyBlockAction)

class PolicyDseNtpOverrideMetricsReporter {
 public:
  static void ReportMetrics(Profile* profile);
  static void LogBlockAction(LowTrustPolicyBlockAction action);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_POLICY_DSE_NTP_OVERRIDE_METRICS_REPORTER_H_
