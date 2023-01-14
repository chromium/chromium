// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"

namespace ash {
namespace cert_provisioning {

#define CP_PREFIX "ChromeOS.CertProvisioning"

#define CP_EVENT ".Event"
#define CP_KEYPAIR_GENERATION_TIME ".KeypairGenerationTime"

#define CP_RESULT "ChromeOS.CertProvisioning.Result"
#define CP_VA_TIME "ChromeOS.CertProvisioning.VaTime"
#define CP_CSR_SIGN_TIME "ChromeOS.CertProvisioning.CsrSignTime"

#define CP_DYNAMIC ".Dynamic"

#define CP_USER ".User"
#define CP_DEVICE ".Device"

namespace {
// "*.User" should have index 0, "*.Device" should have index 1 (same as values
// of CertScope).
const char* const kResult[] = {CP_RESULT CP_USER, CP_RESULT CP_DEVICE};
const char* const kEvent[][2] = {
    {CP_PREFIX CP_EVENT CP_USER, CP_PREFIX CP_EVENT CP_DEVICE},
    {CP_PREFIX CP_EVENT CP_DYNAMIC CP_USER,
     CP_PREFIX CP_EVENT CP_DYNAMIC CP_DEVICE}};
const char* const kKeypairGenerationTime[][2] = {
    {CP_PREFIX CP_KEYPAIR_GENERATION_TIME CP_USER,
     CP_PREFIX CP_KEYPAIR_GENERATION_TIME CP_DEVICE},
    {CP_PREFIX CP_KEYPAIR_GENERATION_TIME CP_DYNAMIC CP_USER,
     CP_PREFIX CP_KEYPAIR_GENERATION_TIME CP_DYNAMIC CP_DEVICE}};
const char* const kVaTime[] = {CP_VA_TIME CP_USER, CP_VA_TIME CP_DEVICE};
const char* const kSignCsrTime[] = {CP_CSR_SIGN_TIME CP_USER,
                                    CP_CSR_SIGN_TIME CP_DEVICE};

// CertScope has stable indexes because it is also used for serialization.
constexpr int ScopeToIdx(CertScope scope) {
  static_assert(static_cast<int>(CertScope::kMaxValue) == 1,
                "CertScope was modified, update arrays with metric names");
  return static_cast<int>(scope);
}

// CertScope has stable indexes because it is also used for serialization.
constexpr int ProtocolVersionToIdx(ProtocolVersion protocol_version) {
  switch (protocol_version) {
    case ProtocolVersion::kStatic:
      return 0;
    case ProtocolVersion::kDynamic:
      return 1;
  }
}
}  // namespace

void RecordResult(CertScope scope,
                  CertProvisioningWorkerState final_state,
                  CertProvisioningWorkerState prev_state) {
  base::UmaHistogramEnumeration(kResult[ScopeToIdx(scope)], final_state);
  if (final_state == CertProvisioningWorkerState::kFailed) {
    base::UmaHistogramEnumeration(kResult[ScopeToIdx(scope)], prev_state);
  }
}

void RecordEvent(ProtocolVersion protocol_version,
                 CertScope scope,
                 CertProvisioningEvent event) {
  base::UmaHistogramEnumeration(
      kEvent[ProtocolVersionToIdx(protocol_version)][ScopeToIdx(scope)], event);
}

void RecordKeypairGenerationTime(ProtocolVersion protocol_version,
                                 CertScope scope,
                                 base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(
      kKeypairGenerationTime[ProtocolVersionToIdx(protocol_version)]
                            [ScopeToIdx(scope)],
      sample, base::Milliseconds(1), base::Minutes(2), 25);
}

void RecordVerifiedAccessTime(CertScope scope, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(kVaTime[ScopeToIdx(scope)], sample,
                                base::Milliseconds(1), base::Minutes(2), 25);
}

void RecordCsrSignTime(CertScope scope, base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(kSignCsrTime[ScopeToIdx(scope)], sample,
                                base::Milliseconds(1), base::Minutes(2), 25);
}

}  // namespace cert_provisioning
}  // namespace ash
