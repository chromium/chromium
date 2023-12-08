// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/crd_uma_logger.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_remote_command_utils.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "device_management_backend.pb.h"

namespace {

constexpr base::TimeDelta kMinDuration = base::Minutes(1);
constexpr base::TimeDelta kMaxDuration = base::Hours(8);
constexpr base::TimeDelta kBucketSize = base::Minutes(10);

}  // namespace

namespace policy {

CrdUmaLogger::CrdUmaLogger(CrdSessionType session_type,
                           UserSessionType user_session_type)
    : session_type_(session_type), user_session_type_(user_session_type) {}

void CrdUmaLogger::LogSessionLaunchResult(
    ExtendedStartCrdSessionResultCode result_code) {
  base::UmaHistogramEnumeration(
      GetUmaHistogramName(kMetricDeviceRemoteCommandCrdResultTemplate),
      result_code);
}

void CrdUmaLogger::LogSessionDuration(base::TimeDelta duration) {
  // Warning: changing the number of buckets logged will make it impossible to
  // compare UMA logs recorded before and after the change!
  base::UmaHistogramCustomTimes(
      /*name=*/GetUmaHistogramName(
          kMetricDeviceRemoteCommandCrdSessionDurationTemplate),
      /*sample=*/duration,
      /*min=*/kMinDuration,
      /*max=*/kMaxDuration,
      /*buckets=*/kMaxDuration / kBucketSize);
}

std::string CrdUmaLogger::GetUmaHistogramName(const char* name_template) const {
  return base::StringPrintfNonConstexpr(name_template, FormatCrdSessionType(),
                                        FormatUserSessionType());
}

// Created a separate method to have fixed values for UMA logs.
// The returned strings should not be changed, as they are logged as part of the
// UMA keys.
const char* CrdUmaLogger::FormatUserSessionType() const {
  switch (user_session_type_) {
    case UserSessionType::USER_SESSION_TYPE_UNKNOWN:
      return "UnknownUserSession";
    case UserSessionType::AUTO_LAUNCHED_KIOSK_SESSION:
      return "AutoLaunchedKioskSession";
    case UserSessionType::MANUALLY_LAUNCHED_KIOSK_SESSION:
      return "ManuallyLaunchedKioskSession";
    case UserSessionType::AFFILIATED_USER_SESSION:
      return "AffiliatedUserSession";
    case UserSessionType::UNAFFILIATED_USER_SESSION:
      return "UnaffiliatedUserSession";
    case UserSessionType::MANAGED_GUEST_SESSION:
      return "ManagedGuestSession";
    case UserSessionType::GUEST_SESSION:
      return "GuestSession";
    case UserSessionType::NO_SESSION:
      return "NoUserSession";
  }
}

const char* CrdUmaLogger::FormatCrdSessionType() const {
  switch (session_type_) {
    case CrdSessionType::CRD_SESSION_TYPE_UNKNOWN:
      return "Unknown";
    case CrdSessionType::REMOTE_ACCESS_SESSION:
      return "RemoteAccess";
    case CrdSessionType::REMOTE_SUPPORT_SESSION:
      return "RemoteSupport";
  }
}

}  // namespace policy
