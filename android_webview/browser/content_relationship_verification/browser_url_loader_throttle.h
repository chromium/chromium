// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_CONTENT_RELATIONSHIP_VERIFICATION_BROWSER_URL_LOADER_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_CONTENT_RELATIONSHIP_VERIFICATION_BROWSER_URL_LOADER_THROTTLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/content_relationship_verification/response_header_verifier.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/gurl.h"

namespace android_webview {

// TODO(crbug.com/1376958): Add CSP as method to allow content access in this
// throttle and then move it to components/third_party_restrictions.

// BrowserURLLoaderThrottle is used in the browser process to perform a
// digital asset links verification to determine whether a URL and also its
// redirect URLs are considered first party content and will be loaded.
//
// This throttle never defers starting the URL request or following redirects.
// If any of the checks for the original URL and redirect chain are not complete
// by the time the response headers are available, the request is deferred
// until all the checks are done. It cancels the load if any URLs turn out to
// be bad.
class BrowserURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  class OriginVerificationSchedulerBridge {
   public:
    using OriginVerifierCallback = base::OnceCallback<void(bool /*verified*/)>;

    OriginVerificationSchedulerBridge();
    virtual ~OriginVerificationSchedulerBridge();

    OriginVerificationSchedulerBridge(
        const OriginVerificationSchedulerBridge&) = delete;
    OriginVerificationSchedulerBridge& operator=(
        const OriginVerificationSchedulerBridge&) = delete;

    virtual void Verify(std::string url, OriginVerifierCallback callback) = 0;
  };

  static std::unique_ptr<BrowserURLLoaderThrottle> Create(
      OriginVerificationSchedulerBridge* bridge);

  BrowserURLLoaderThrottle(const BrowserURLLoaderThrottle&) = delete;
  BrowserURLLoaderThrottle& operator=(const BrowserURLLoaderThrottle&) = delete;

  ~BrowserURLLoaderThrottle() override;

  // blink::URLLoaderThrottle implementation.

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_request_headers,
      net::HttpRequestHeaders* modified_request_headers,
      net::HttpRequestHeaders* modified_cors_exempt_request_headers) override;

  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  const char* NameForLoggingWillProcessResponse() override;

 private:
  explicit BrowserURLLoaderThrottle(OriginVerificationSchedulerBridge* bridge);

  void OnDalVerificationComplete(std::string url, bool dal_verified);

  bool VerifyHeader(const network::mojom::URLResponseHead& response_head);

  raw_ptr<OriginVerificationSchedulerBridge> bridge_;

  GURL url_;

  base::WeakPtrFactory<BrowserURLLoaderThrottle> weak_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_CONTENT_RELATIONSHIP_VERIFICATION_BROWSER_URL_LOADER_THROTTLE_H_
