// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/oidc_metrics_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"

const char kOidcEnrollmentHistogramName[] = "Enterprise.OidcEnrollment";

const char kOidcInterceptionSuffix[] = ".Interception";
const char kOidcProfileCreationSuffix[] = ".ProfileCreation";

const char kOidcFunnelSuffix[] = ".Funnel";
const char kOidcResultSuffix[] = ".Result";

std::string GetIdentityTypeSuffix(bool is_dasher_based) {
  return is_dasher_based ? ".Dasher-based" : ".Dasherless";
}

std::string GetSuccessSuffix(bool success) {
  return success ? ".Success" : ".Failure";
}

void RecordOidcInterceptionFunnelStep(OidcInterceptionFunnelStep step) {
  base::UmaHistogramEnumeration(
      base::StrCat({kOidcEnrollmentHistogramName, kOidcInterceptionSuffix,
                    kOidcFunnelSuffix}),
      step);
}

void RecordOidcInterceptionResult(OidcInterceptionResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({kOidcEnrollmentHistogramName, kOidcInterceptionSuffix,
                    kOidcResultSuffix}),
      result);
}

void RecordOidcProfileCreationFunnelStep(OidcProfileCreationFunnelStep step,
                                         bool is_dasher_based) {
  base::UmaHistogramEnumeration(
      base::StrCat({kOidcEnrollmentHistogramName, kOidcProfileCreationSuffix,
                    kOidcFunnelSuffix, GetIdentityTypeSuffix(is_dasher_based)}),
      step);
}

void RecordOidcProfileCreationResult(OidcProfileCreationResult result,
                                     bool is_dasher_based) {
  base::UmaHistogramEnumeration(
      base::StrCat({kOidcEnrollmentHistogramName, kOidcProfileCreationSuffix,
                    kOidcResultSuffix, GetIdentityTypeSuffix(is_dasher_based)}),
      result);
}

void RecordOidcEnrollmentRegistrationLatency(
    std::optional<bool> is_dasher_based,
    bool success,
    const base::TimeDelta latency) {
  base::UmaHistogramTimes(
      base::StrCat({kOidcEnrollmentHistogramName,
                    (is_dasher_based == std::nullopt)
                        ? std::string()
                        : GetIdentityTypeSuffix(is_dasher_based.value()),
                    ".RegistrationLatency", GetSuccessSuffix(success)}),
      latency);
}

void RecordOidcEnrollmentPolicyFetchLatency(bool is_dasher_based,
                                            bool success,
                                            const base::TimeDelta latency) {
  base::UmaHistogramTimes(
      base::StrCat({kOidcEnrollmentHistogramName,
                    GetIdentityTypeSuffix(is_dasher_based),
                    ".PolicyFetchLatency", GetSuccessSuffix(success)}),
      latency);
}
