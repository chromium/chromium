// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"

namespace ash {
namespace cert_provisioning {

#define CP_RESULT "ChromeOS.CertProvisioning.Result"
#define CP_EVENT "ChromeOS.CertProvisioning.Event"
#define CP_KEYPAIR_GENERATION_TIME \
  "ChromeOS.CertProvisioning.KeypairGenerationTime"
#define CP_VA_TIME "ChromeOS.CertProvisioning.VaTime"
#define CP_CSR_SIGN_TIME "ChromeOS.CertProvisioning.CsrSignTime"

#define CP_USER ".User"
#define CP_DEVICE ".Device"

namespace {
// "*.User" should have index 0, "*.Device" should have index 1 (same as values
// of CertScope).
const char* const kResult[] = {CP_RESULT CP_USER, CP_RESULT CP_DEVICE};
const char* const kEvent[] = {CP_EVENT CP_USER, CP_EVENT CP_DEVICE};
const char* const kKeypairGenerationTime[] = {
    CP_KEYPAIR_GENERATION_TIME CP_USER, CP_KEYPAIR_GENERATION_TIME CP_DEVICE};
const char* const kVaTime[] = {CP_VA_TIME CP_USER, CP_VA_TIME CP_DEVICE};
const char* const kSignCsrTime[] = {CP_CSR_SIGN_TIME CP_USER,
                                    CP_CSR_SIGN_TIME CP_DEVICE};

// CertScope has stable indexes because it is also used for serialization.
constexpr int ToIdx(CertScope scope) {
  static_assert(static_cast<int>(CertScope::kMaxValue) == 1,
                "CertScope was modified, update arrays with metric names");
  return static_cast<int>(scope);
}
}  // namespace

void RecordResult(CertScope scope,
                  CertProvisioningWorkerState final_state,
                  CertProvisioningWorkerState prev_state) {
  base::UmaHistogramEnumeration(kResult[ToIdx(scope)], final_state);
  if (final_state == CertProvisioningWorkerState::kFailed) {
    base::UmaHistogramEnumeration(kResult[ToIdx(scope)], prev_state);
  }
}

void RecordEvent(CertScope scope, CertProvisioningEvent event) {
  base::UmaHistogramEnumeration(kEvent[ToIdx(scope)], event);
}

void RecordKeypairGenerationTime(CertScope scope, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(kKeypairGenerationTime[ToIdx(scope)], sample,
                                base::Milliseconds(1), base::Minutes(2), 25);
}

void RecordVerifiedAccessTime(CertScope scope, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(kVaTime[ToIdx(scope)], sample,
                                base::Milliseconds(1), base::Minutes(2), 25);
}

void RecordCsrSignTime(CertScope scope, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(kSignCsrTime[ToIdx(scope)], sample,
                                base::Milliseconds(1), base::Minutes(2), 25);
}

}  // namespace cert_provisioning
}  // namespace ash
