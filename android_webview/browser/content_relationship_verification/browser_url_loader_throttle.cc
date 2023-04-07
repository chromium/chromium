// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_relationship_verification/browser_url_loader_throttle.h"

#include "base/android/build_info.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/content_relationship_verification/content_relationship_verification_constants.h"
#include "components/content_relationship_verification/response_header_verifier.h"
#include "content/public/browser/browser_thread.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

using content_relationship_verification::ResponseHeaderVerificationResult;

namespace android_webview {

BrowserURLLoaderThrottle::OriginVerificationSchedulerBridge::
    OriginVerificationSchedulerBridge() = default;
BrowserURLLoaderThrottle::OriginVerificationSchedulerBridge::
    ~OriginVerificationSchedulerBridge() = default;

// static
std::unique_ptr<BrowserURLLoaderThrottle> BrowserURLLoaderThrottle::Create(
    OriginVerificationSchedulerBridge* bridge) {
  return base::WrapUnique<BrowserURLLoaderThrottle>(
      new BrowserURLLoaderThrottle(bridge));
}

BrowserURLLoaderThrottle::BrowserURLLoaderThrottle(
    OriginVerificationSchedulerBridge* bridge)
    : bridge_(bridge) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

BrowserURLLoaderThrottle::~BrowserURLLoaderThrottle() = default;

bool BrowserURLLoaderThrottle::VerifyHeader(
    const network::mojom::URLResponseHead& response_head) {
  std::string header_value;
  response_head.headers->GetNormalizedHeader(
      content_relationship_verification::kEmbedderAncestorHeader,
      &header_value);
  ResponseHeaderVerificationResult header_verification_result =
      content_relationship_verification::ResponseHeaderVerifier::Verify(
          base::android::BuildInfo::GetInstance()->host_package_name(),
          header_value);

  if (header_verification_result == ResponseHeaderVerificationResult::kAllow) {
    return true;
  } else if (header_verification_result ==
             ResponseHeaderVerificationResult::kMissing) {
    // TODO(crbug.com/1376958): Check for permission.
    return true;
  }
  return false;
}

void BrowserURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  url_ = request->url;
}

void BrowserURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  DCHECK(delegate_);

  GURL originating_url = url_;
  url_ = redirect_info->new_url;

  if (VerifyHeader(response_head)) {
    return;
  }

  *defer = true;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OriginVerificationSchedulerBridge::Verify, base::Unretained(bridge_),
          url_.spec(),
          base::BindOnce(&BrowserURLLoaderThrottle::OnDalVerificationComplete,
                         weak_factory_.GetWeakPtr(), originating_url.spec())));
}

void BrowserURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(delegate_);

  if (VerifyHeader(*response_head)) {
    return;
  }

  *defer = true;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OriginVerificationSchedulerBridge::Verify, base::Unretained(bridge_),
          response_url.spec(),
          base::BindOnce(&BrowserURLLoaderThrottle::OnDalVerificationComplete,
                         weak_factory_.GetWeakPtr(), response_url.spec())));
}

void BrowserURLLoaderThrottle::OnDalVerificationComplete(std::string url,
                                                         bool dal_verified) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(delegate_);

  if (dal_verified) {
    delegate_->Resume();
    return;
  }
  delegate_->CancelWithExtendedError(
      content_relationship_verification::
          kNetErrorCodeForContentRelationshipVerification,
      static_cast<int>(content_relationship_verification::kExtendedErrorReason),
      content_relationship_verification::kCustomCancelReasonForURLLoader);
}

const char* BrowserURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "DigitalAssetLinksBrowserThrottle";
}

}  // namespace android_webview
