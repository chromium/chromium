// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_uma.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace {

const char* const kMetricEnrollment = "Enterprise.Enrollment";
const char* const kMetricEnrollmentForced = "Enterprise.EnrollmentForced";
const char* const kMetricEnrollmentForcedInitial =
    "Enterprise.EnrollmentForcedInitial";
const char* const kMetricEnrollmentAttestationBased =
    "Enterprise.EnrollmentAttestationBased";
const char* const kMetricEnrollmentForcedAttestationBased =
    "Enterprise.EnrollmentForcedAttestationBased";
const char* const kMetricEnrollmentForcedInitialAttestationBased =
    "Enterprise.EnrollmentForcedInitialAttestationBased";
const char* const kMetricEnrollmentForcedManualFallback =
    "Enterprise.EnrollmentForcedManualFallback";
const char* const kMetricEnrollmentForcedInitialManualFallback =
    "Enterprise.EnrollmentForcedInitialManualFallback";
const char* const kMetricEnrollmentRecovery = "Enterprise.EnrollmentRecovery";
const char* const kMetricEnrollmentConfiguration =
    "Enterprise.EnrollmentConfiguration";

}  // namespace

namespace chromeos {

void EnrollmentUMA(policy::MetricEnrollment sample,
                   policy::EnrollmentConfig::Mode mode) {
  switch (mode) {
    case policy::EnrollmentConfig::MODE_MANUAL:
    case policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
    case policy::EnrollmentConfig::MODE_LOCAL_ADVERTISED:
    case policy::EnrollmentConfig::MODE_SERVER_ADVERTISED:
    case policy::EnrollmentConfig::MODE_OFFLINE_DEMO:
      base::UmaHistogramSparse(kMetricEnrollment, sample);
      break;
    case policy::EnrollmentConfig::MODE_ATTESTATION:
      base::UmaHistogramSparse(kMetricEnrollmentAttestationBased, sample);
      break;
    case policy::EnrollmentConfig::MODE_LOCAL_FORCED:
    case policy::EnrollmentConfig::MODE_SERVER_FORCED:
      base::UmaHistogramSparse(kMetricEnrollmentForced, sample);
      break;
    case policy::EnrollmentConfig::MODE_INITIAL_SERVER_FORCED:
      base::UmaHistogramEnumeration(kMetricEnrollmentForcedInitial, sample);
      break;
    case policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED:
    case policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED:
      base::UmaHistogramSparse(kMetricEnrollmentForcedAttestationBased, sample);
      break;
    case policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED:
      base::UmaHistogramEnumeration(
          kMetricEnrollmentForcedInitialAttestationBased, sample);
      break;
    case policy::EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK:
      base::UmaHistogramSparse(kMetricEnrollmentForcedManualFallback, sample);
      break;
    case policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK:
      base::UmaHistogramEnumeration(
          kMetricEnrollmentForcedInitialManualFallback, sample);
      break;
    case policy::EnrollmentConfig::MODE_RECOVERY:
    case policy::EnrollmentConfig::MODE_ENROLLED_ROLLBACK:
      base::UmaHistogramSparse(kMetricEnrollmentRecovery, sample);
      break;
    case policy::EnrollmentConfig::MODE_ATTESTATION_ENROLLMENT_TOKEN:
      base::UmaHistogramSparse(kMetricEnrollmentConfiguration, sample);
      break;
    case policy::EnrollmentConfig::MODE_NONE:
      NOTREACHED();
      break;
  }
}

}  // namespace chromeos
