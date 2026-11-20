// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/metrics/glic_structured_metrics_conversions.h"

#if BUILDFLAG(STRUCTURED_METRICS_ENABLED)
namespace glic {

// Written as an exhaustive switch with no default so that adding a
// ClientLoadErrorReason without adding the matching structured.xml variant is a
// compile error rather than a silently mislabelled metric.
// LINT.IfChange(ToStructuredClientLoadErrorReason)
metrics::structured::events::v2::glic::GlicClientLoadErrorReason
ToStructuredClientLoadErrorReason(ClientLoadErrorReason reason) {
  using Reason =
      metrics::structured::events::v2::glic::GlicClientLoadErrorReason;
  switch (reason) {
    case ClientLoadErrorReason::kUnknown:
      return Reason::UNKNOWN;
    case ClientLoadErrorReason::kUnavailable:
      return Reason::UNAVAILABLE;
    case ClientLoadErrorReason::kIneligibleAccount:
      return Reason::INELIGIBLE_ACCOUNT;
    case ClientLoadErrorReason::kLocationMismatch:
      return Reason::LOCATION_MISMATCH;
    case ClientLoadErrorReason::kDisabledByAdmin:
      return Reason::DISABLED_BY_ADMIN;
    case ClientLoadErrorReason::kOffline:
      return Reason::OFFLINE;
    case ClientLoadErrorReason::kSignIn:
      return Reason::SIGN_IN;
    case ClientLoadErrorReason::kCookieSyncFailed:
      return Reason::COOKIE_SYNC_FAILED;
    case ClientLoadErrorReason::kGuestLoadFailed:
      return Reason::GUEST_LOAD_FAILED;
    case ClientLoadErrorReason::kGuestProcessGone:
      return Reason::GUEST_PROCESS_GONE;
    case ClientLoadErrorReason::kClientError:
      return Reason::CLIENT_ERROR;
    case ClientLoadErrorReason::kClientLoadTimeout:
      return Reason::CLIENT_LOAD_TIMEOUT;
    case ClientLoadErrorReason::kWarmedTimeout:
      return Reason::WARMED_TIMEOUT;
    case ClientLoadErrorReason::kGuestOriginBlocked:
      return Reason::GUEST_ORIGIN_BLOCKED;
    case ClientLoadErrorReason::kGuestApiNotAllowed:
      return Reason::GUEST_API_NOT_ALLOWED;
    case ClientLoadErrorReason::kPendingProviderChange:
      return Reason::PENDING_PROVIDER_CHANGE;
    case ClientLoadErrorReason::kGuestHttpClientError:
      return Reason::GUEST_HTTP_CLIENT_ERROR;
    case ClientLoadErrorReason::kGuestHttpServerError:
      return Reason::GUEST_HTTP_SERVER_ERROR;
    case ClientLoadErrorReason::kGuestProcessCrashed:
      return Reason::GUEST_PROCESS_CRASHED;
    case ClientLoadErrorReason::kGuestProcessKilled:
      return Reason::GUEST_PROCESS_KILLED;
    case ClientLoadErrorReason::kGuestProcessOom:
      return Reason::GUEST_PROCESS_OOM;
    case ClientLoadErrorReason::kGuestProcessLaunchFailed:
      return Reason::GUEST_PROCESS_LAUNCH_FAILED;
    case ClientLoadErrorReason::kGuestProcessIntegrityFailure:
      return Reason::GUEST_PROCESS_INTEGRITY_FAILURE;
    case ClientLoadErrorReason::kClientLoadTimeoutNavigationInProgress:
      return Reason::CLIENT_LOAD_TIMEOUT_NAVIGATION_IN_PROGRESS;
    case ClientLoadErrorReason::kClientLoadTimeoutNavigationNotCommitted:
      return Reason::CLIENT_LOAD_TIMEOUT_NAVIGATION_NOT_COMMITTED;
    case ClientLoadErrorReason::kClientLoadTimeoutBeforeOnload:
      return Reason::CLIENT_LOAD_TIMEOUT_BEFORE_ONLOAD;
    case ClientLoadErrorReason::kClientLoadTimeoutAfterOnload:
      return Reason::CLIENT_LOAD_TIMEOUT_AFTER_ONLOAD;
    case ClientLoadErrorReason::kProviderSwitchNotice:
      return Reason::PROVIDER_SWITCH_NOTICE;
  }
}
// LINT.ThenChange(//chrome/browser/glic/glic_enums.h:ClientLoadErrorReason)

}  // namespace glic
#endif  // BUILDFLAG(STRUCTURED_METRICS_ENABLED)
