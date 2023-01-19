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

void LogKeyRotationCommandError(KeyRotationCommandError error) {
  static constexpr char kErrorHistogram[] =
      "Enterprise.DeviceTrust.KeyRotationCommand.Error";
  base::UmaHistogramEnumeration(kErrorHistogram, error);
}

void LogKeyRotationExitCode(int exit_code) {
  static constexpr char kExitCodeHistogram[] =
      "Enterprise.DeviceTrust.KeyRotationCommand.ExitCode";
  base::UmaHistogramSparse(kExitCodeHistogram, exit_code);
}

void LogManagementServiceExitCode(int exit_code) {
  if (exit_code < 0) {
    base::UmaHistogramExactLinear(kNegativeExitCodeHistogramName, -exit_code,
                                  kMaxExitCode);
  } else {
    base::UmaHistogramExactLinear(kPositiveExitCodeHistogramName, exit_code,
                                  kMaxExitCode);
  }
}

#if BUILDFLAG(IS_WIN)
void LogUnexpectedHresult(HRESULT result) {
  static constexpr char kHresultHistogram[] =
      "Enterprise.DeviceTrust.KeyRotationCommand.Error.Hresult";
  base::UmaHistogramSparse(kHresultHistogram, result);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace enterprise_connectors
