// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/metrics_utils.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace enterprise_connectors {

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
  PrefService* prefs =
      Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext())
          ->GetPrefs();
  // TODO(b/183690432): Check if the browser or device is being managed
  // to create the throttle.
  if (!DeviceTrustConnectorService::IsConnectorEnabled(prefs))
    return nullptr;

  return std::make_unique<DeviceTrustNavigationThrottle>(navigation_handle);
}

DeviceTrustNavigationThrottle::DeviceTrustNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : DeviceTrustNavigationThrottle(
          DeviceTrustServiceFactory::GetForProfile(Profile::FromBrowserContext(
              navigation_handle->GetWebContents()->GetBrowserContext())),
          navigation_handle) {}

DeviceTrustNavigationThrottle::DeviceTrustNavigationThrottle(
    DeviceTrustService* device_trust_service,
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      device_trust_service_(device_trust_service) {}

DeviceTrustNavigationThrottle::~DeviceTrustNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::WillStartRequest() {
  return AddHeadersIfNeeded();
}

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::WillRedirectRequest() {
  return AddHeadersIfNeeded();
}

const char* DeviceTrustNavigationThrottle::GetNameForLogging() {
  return "DeviceTrustNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
DeviceTrustNavigationThrottle::AddHeadersIfNeeded() {
  const GURL& url = navigation_handle()->GetURL();
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return PROCEED;

  if (!device_trust_service_ || !device_trust_service_->IsEnabled())
    return PROCEED;

  if (!device_trust_service_->Watches(url))
    return PROCEED;

  // If we are starting an attestation flow.
  if (navigation_handle()->GetResponseHeaders() == nullptr) {
    LogAttestationFunnelStep(DTAttestationFunnelStep::kAttestationFlowStarted);
    navigation_handle()->SetRequestHeader(kDeviceTrustHeader,
                                          kDeviceTrustHeaderValue);
    return PROCEED;
  }

  // If a challenge is coming from the Idp.
  if (navigation_handle()->GetResponseHeaders()->HasHeader(
          kVerifiedAccessChallengeHeader)) {
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
      AttestationCallback resume_navigation_callback = base::BindOnce(
          &DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume,
          weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now());

      // Call `DeviceTrustService::BuildChallengeResponse` which is one step on
      // the chain that builds the challenge response. In this chain we post a
      // task that won't run in the main thread.
      //
      // Because BuildChallengeResponse() may run the resume callback
      // synchronously, this call is deferred to ensure that this method returns
      // DEFER before `resume_navigation_callback` is invoked.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WeakPtr<DeviceTrustNavigationThrottle> throttler,
                 const std::string& challenge,
                 AttestationCallback resume_navigation_callback) {
                if (throttler) {
                  throttler->device_trust_service_->BuildChallengeResponse(
                      challenge, std::move(resume_navigation_callback));
                }
              },
              weak_ptr_factory_.GetWeakPtr(), challenge,
              std::move(resume_navigation_callback)));
      return DEFER;
    }
  }
  return PROCEED;
}

void DeviceTrustNavigationThrottle::ReplyChallengeResponseAndResume(
    base::TimeTicks start_time,
    const std::string& challenge_response) {
  LogAttestationResponseLatency(start_time,
                                /*success=*/!challenge_response.empty());

  if (!challenge_response.empty()) {
    LogAttestationFunnelStep(DTAttestationFunnelStep::kChallengeResponseSent);
    navigation_handle()->SetRequestHeader(kVerifiedAccessResponseHeader,
                                          challenge_response);
  }

  Resume();
}

}  // namespace enterprise_connectors
