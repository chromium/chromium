// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

namespace {
constexpr char kLatencyHistogramFormat[] =
    "Enterprise.DeviceTrust.Attestation.ResponseLatency.%s";

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Enrollment status of the device where the Device Trust connector attestation
// is happening. These values are persisted to logs and should not be
// renumbered. Please update the DTEnrollmentStatus enum in enums.xml when
// adding a new step here.
enum class DTEnrollmentStatus {
  kManaged = 0,
  kUnmanaged = 1,
  kMaxValue = kUnmanaged,
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}  // namespace

void LogAttestationFunnelStep(DTAttestationFunnelStep step) {
  base::UmaHistogramEnumeration("Enterprise.DeviceTrust.Attestation.Funnel",
                                step);
}

void LogAttestationResult(DTAttestationResult result) {
  base::UmaHistogramEnumeration("Enterprise.DeviceTrust.Attestation.Result",
                                result);
}

void LogAttestationResponseLatency(base::TimeTicks start_time, bool success) {
  base::UmaHistogramTimes(base::StringPrintf(kLatencyHistogramFormat,
                                             success ? "Success" : "Failure"),
                          base::TimeTicks::Now() - start_time);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void LogOrigin(DTOrigin origin) {
  base::UmaHistogramEnumeration("Enterprise.DeviceTrust.Origin", origin);
}

void LogEnrollmentStatus() {
  base::UmaHistogramEnumeration(
      "Enterprise.DeviceTrust.EnrollmentStatus",
      ash::InstallAttributes::Get()->IsEnterpriseManaged()
          ? DTEnrollmentStatus::kManaged
          : DTEnrollmentStatus::kUnmanaged);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_connectors
