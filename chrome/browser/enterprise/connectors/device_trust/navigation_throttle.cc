// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/device_trust_constants.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/device_signals_consent/consent_requester.h"
#include "components/device_signals/core/browser/pref_names.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace enterprise_connectors {

using DeviceTrustCallback = DeviceTrustService::DeviceTrustCallback;

namespace {

constexpr char kErrorPropertyName[] = "error";
constexpr char kSpecificErrorCodePropertyName[] = "code";

const std::string CreateErrorJsonString(
    const DeviceTrustResponse& dt_response) {
  DCHECK(dt_response.error);
  base::Value::Dict error_response;
  error_response.Set(kErrorPropertyName,
                     DeviceTrustErrorToString(dt_response.error.value()));

  if (dt_response.attestation_result &&
      !IsSuccessAttestationResult(dt_response.attestation_result.value())) {
    error_response.Set(
        kSpecificErrorCodePropertyName,
        AttestationErrorToString(dt_response.attestation_result.value()));
  }

  std::string out_json;
  if (!base::JSONWriter::Write(error_response, &out_json)) {
    return "{\"error\":\"failed_to_serialize_error\"}";
  }
  return out_json;
}

bool VerifyURL(GURL url) {
  return (url.is_valid() && url.SchemeIsHTTPOrHTTPS());
}

Profile* GetProfile(content::NavigationHandle* navigation_handle) {
  if (!navigation_handle || !navigation_handle->GetWebContents()) {
    return nullptr;
  }

  return Profile::FromBrowserContext(
      navigation_handle->GetWebContents()->GetBrowserContext());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
DTOrigin GetAttestationFlowOrigin(content::BrowserContext* context) {
  if (context->IsOffTheRecord() && ash::ProfileHelper::IsSigninProfile(
                                       Profile::FromBrowserContext(context))) {
    return DTOrigin::kLoginScreen;
  }

  return DTOrigin::kInSession;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

// Const headers used in the handshake flow.
constexpr char kDeviceTrustHeader[] = "X-Device-Trust";
constexpr char kDeviceTrustHeaderValue[] = "VerifiedAccess";
constexpr char kVerifiedAccessChallengeHeader[] = "X-Verified-Access-Challenge";
constexpr char kVerifiedAccessResponseHeader[] =
    "X-Verified-Access-Challenge-Response";

// static
std::unique_ptr<DeviceTrustNavigationThrottle>
DeviceTrustNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  auto* profile = GetProfile(navigation_handle);
  auto* device_trust_service =
      DeviceTrustServiceFactory::GetForProfile(profile);

  auto* user_permission_service =
      enterprise_signals::UserPermissionServiceFactory::GetForProfile(profile);
  if ((!device_trust_service || !device_trust_service->IsEnabled()) &&
      (!user_permission_service ||
       !user_permission_service->ShouldCollectConsent())) {
    return nullptr;
  }

  return std::make_unique<DeviceTrustNavigationThrottle>(
      device_trust_service, user_permission_service, navigation_handle);
}

DeviceTrustNavigationThrottle::DeviceTrustNavigationThrottle(
    DeviceTrustService* device_trust_service,
    device_signals::UserPermissionService* user_permission_service,
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      device_trust_service_(device_trust_service),
      user_permission_service_(user_permission_service),
      consent_requester_(ConsentRequester::CreateConsentRequester(
          GetProfile(navigation_handle))) {}

DeviceTrustNavigationThrottle::~DeviceTrustNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::WillStartRequest() {
  auto consent_dialog_check_result = MayTriggerConsentDialog();

  return (consent_dialog_check_result.action() == PROCEED)
             ? AddHeadersIfNeeded()
             : consent_dialog_check_result;
}

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::WillRedirectRequest() {
  return AddHeadersIfNeeded();
}

const char* DeviceTrustNavigationThrottle::GetNameForLogging() {
  return "DeviceTrustNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::MayTriggerConsentDialog() {
  if (!enterprise_signals::features::IsConsentDialogEnabled()) {
    return PROCEED;
  }
  const GURL& url = navigation_handle()->GetURL();
  if (!user_permission_service_ ||
      !user_permission_service_->ShouldCollectConsent() || !VerifyURL(url) ||
      !navigation_handle()->HasUserGesture() ||
      !navigation_handle()->IsInMainFrame()) {
    return PROCEED;
  }
  if (!consent_requester_) {
    return PROCEED;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<DeviceTrustNavigationThrottle> throttler) {
            if (throttler) {
              throttler->consent_requester_->RequestConsent(base::BindRepeating(
                  &DeviceTrustNavigationThrottle::OnConsentPrefUpdated,
                  throttler));
            }
          },
          weak_ptr_factory_.GetWeakPtr()));

  return DEFER;
}

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::AddHeadersIfNeeded() {
  const GURL& url = navigation_handle()->GetURL();
  if (!VerifyURL(url)) {
    return PROCEED;
  }
  if (!device_trust_service_ || !device_trust_service_->IsEnabled() ||
      !user_permission_service_ ||
      user_permission_service_->CanCollectSignals() !=
          device_signals::UserPermission::kGranted) {
    return PROCEED;
  }
  const std::set<DTCPolicyLevel> levels = device_trust_service_->Watches(url);
  if (levels.empty()) {
    return PROCEED;
  }

  // If we are starting an attestation flow.
  if (navigation_handle()->GetResponseHeaders() == nullptr ||
      !navigation_handle()->GetResponseHeaders()->HasHeader(
          kVerifiedAccessChallengeHeader)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    LogOrigin(GetAttestationFlowOrigin(
        navigation_handle()->GetWebContents()->GetBrowserContext()));
    LogEnrollmentStatus();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    LogAttestationFunnelStep(DTAttestationFunnelStep::kAttestationFlowStarted);
    navigation_handle()->SetRequestHeader(kDeviceTrustHeader,
                                          kDeviceTrustHeaderValue);
    return PROCEED;
  }

  // Reaching this point means there is a challenge coming from the Idp.
  // Remove request header since is not needed for challenge response.
  navigation_handle()->RemoveRequestHeader(kDeviceTrustHeader);

  // Get challenge.
  const net::HttpResponseHeaders* headers =
      navigation_handle()->GetResponseHeaders();
  std::string challenge;
  if (headers->GetNormalizedHeader(kVerifiedAccessChallengeHeader,
                                   &challenge)) {
    LogAttestationFunnelStep(DTAttestationFunnelStep::kChallengeReceived);

    // Create callback for `ReplyChallengeResponseAndResume` which will
    // be called after the challenge response is created. With this
    // we can defer the navigation to unblock the main thread.
    const base::TimeTicks start_time = base::TimeTicks::Now();
    DeviceTrustCallback resume_navigation_callback = base::BindOnce(
        &DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume,
        weak_ptr_factory_.GetWeakPtr(), start_time);

    // Call `DeviceTrustService::BuildChallengeResponse` which is one step on
    // the chain that builds the challenge response. In this chain we post a
    // task that won't run in the main thread.
    //
    // Because BuildChallengeResponse() may run the resume callback
    // synchronously, this call is deferred to ensure that this method returns
    // DEFER before `resume_navigation_callback` is invoked.
    LogAttestationPolicyLevel(levels);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::WeakPtr<DeviceTrustNavigationThrottle> throttler,
               const std::string& challenge,
               const std::set<DTCPolicyLevel>& levels,
               DeviceTrustCallback resume_navigation_callback) {
              if (throttler) {
                throttler->device_trust_service_->BuildChallengeResponse(
                    challenge, levels, std::move(resume_navigation_callback));
              }
            },
            weak_ptr_factory_.GetWeakPtr(), challenge, levels,
            std::move(resume_navigation_callback)));

    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DeviceTrustNavigationThrottle::OnResponseTimedOut,
                       weak_ptr_factory_.GetWeakPtr(), start_time),
        timeouts::kHandshakeTimeout);

    is_resumed_ = false;
    return DEFER;
  }
  return PROCEED;
}

void DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume(
    base::TimeTicks start_time,
    const DeviceTrustResponse& dt_response) {
  if (is_resumed_) {
    return;
  }
  is_resumed_ = true;

  // Make a copy to allow mutations.
  auto copied_dt_response = dt_response;

  if (copied_dt_response.challenge_response.empty() &&
      !copied_dt_response.error) {
    // An empty `challenge_response` value must be treated as a failure. If
    // `error` isn't set, then default-set it to unknown.
    copied_dt_response.error = DeviceTrustError::kUnknown;
  }

  LogDeviceTrustResponse(copied_dt_response, start_time);

  if (copied_dt_response.error) {
    navigation_handle()->SetRequestHeader(
        kVerifiedAccessResponseHeader,
        CreateErrorJsonString(copied_dt_response));
  } else {
    LogAttestationFunnelStep(DTAttestationFunnelStep::kChallengeResponseSent);
    navigation_handle()->SetRequestHeader(
        kVerifiedAccessResponseHeader, copied_dt_response.challenge_response);
  }

  Resume();
}

void DeviceTrustNavigationThrottle::OnResponseTimedOut(
    base::TimeTicks start_time) {
  if (is_resumed_) {
    return;
  }
  is_resumed_ = true;

  DeviceTrustResponse timeout_response;
  timeout_response.error = DeviceTrustError::kTimeout;

  LogDeviceTrustResponse(timeout_response, start_time);

  navigation_handle()->SetRequestHeader(
      kVerifiedAccessResponseHeader, CreateErrorJsonString(timeout_response));
  Resume();
}

void DeviceTrustNavigationThrottle::OnConsentPrefUpdated() {
  if (AddHeadersIfNeeded().action() == PROCEED) {
    Resume();
  }
}

}  // namespace enterprise_connectors
