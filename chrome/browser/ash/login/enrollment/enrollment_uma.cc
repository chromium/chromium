// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_uma.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"

namespace ash {
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
const char* const kMetricEnrollmentRollbackAttestation =
    "Enterprise.EnrollmentRollbackAttestation";
const char* const kMetricEnrollmentRollbackManualFallback =
    "Enterprise.EnrollmentRollbackManualFallback";
const char* const kMetricEnrollmentTokenBased =
    "Enterprise.EnrollmentTokenBased";
const char* const kMetricEnrollmentTokenBasedManualFallback =
    "Enterprise.EnrollmentTokenBasedManualFallback";

}  // namespace

void EnrollmentUMA(policy::MetricEnrollment sample,
                   policy::EnrollmentConfig::Mode mode) {
  switch (mode) {
    case policy::EnrollmentConfig::MODE_MANUAL:
    case policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
    case policy::EnrollmentConfig::MODE_LOCAL_ADVERTISED:
    case policy::EnrollmentConfig::MODE_SERVER_ADVERTISED:
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
      base::UmaHistogramSparse(kMetricEnrollmentRecovery, sample);
      break;
    case policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED:
      base::UmaHistogramSparse(kMetricEnrollmentRollbackAttestation, sample);
      break;
    case policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK:
      base::UmaHistogramSparse(kMetricEnrollmentRollbackManualFallback, sample);
      break;
    case policy::EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED:
      base::UmaHistogramSparse(kMetricEnrollmentTokenBased, sample);
      break;
    case policy::EnrollmentConfig::
        MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK:
      base::UmaHistogramSparse(kMetricEnrollmentTokenBasedManualFallback,
                               sample);
      break;
    case policy::EnrollmentConfig::MODE_NONE:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

std::string GetOOBEConfigSourceVariantString(
    policy::OOBEConfigSource oobe_config_source) {
  switch (oobe_config_source) {
    case policy::OOBEConfigSource::kNone:
      return ".None";
    case policy::OOBEConfigSource::kUnknown:
      return ".Unknown";
    case policy::OOBEConfigSource::kRemoteDeployment:
      return ".RemoteDeployment";
    case policy::OOBEConfigSource::kPackagingTool:
      return ".PackagingTool";
  }
}

void TokenBasedEnrollmentOOBEConfigUMA(
    policy::EnrollmentStatus status,
    policy::OOBEConfigSource oobe_config_source) {
  const bool success =
      status.enrollment_code() == policy::EnrollmentStatus::Code::kSuccess;
  std::string metric_name = base::StrCat(
      {policy::kUMAPrefixEnrollmentTokenBasedOOBEConfig,
       GetOOBEConfigSourceVariantString(oobe_config_source), ".Success"});

  base::UmaHistogramBoolean(metric_name, success);
}

}  // namespace ash
