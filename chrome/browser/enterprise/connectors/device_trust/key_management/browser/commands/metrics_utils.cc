// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/commands/metrics_utils.h"

#include "base/metrics/histogram_functions.h"

namespace enterprise_connectors {

namespace {

// Max supported value for the UMA histogram exclusive max.
constexpr int kMaxExitCode = 101;
constexpr char kPositiveExitCodeHistogramName[] =
    "Enterprise.DeviceTrust.ManagementService.ExitCode.Positive";
constexpr char kNegativeExitCodeHistogramName[] =
    "Enterprise.DeviceTrust.ManagementService.ExitCode.Negative";

}  // namespace

void LogManagementServiceExitCode(int exit_code) {
  if (exit_code < 0) {
    base::UmaHistogramExactLinear(kNegativeExitCodeHistogramName, -exit_code,
                                  kMaxExitCode);
  } else {
    base::UmaHistogramExactLinear(kPositiveExitCodeHistogramName, exit_code,
                                  kMaxExitCode);
  }
}

}  // namespace enterprise_connectors
