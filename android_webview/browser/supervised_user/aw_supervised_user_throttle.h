// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_THROTTLE_H_

#include "android_webview/browser/supervised_user/aw_supervised_user_url_classifier.h"
#include "base/memory/raw_ptr.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace android_webview {

// This throttle is used to check if a given url (http and https only)
// is allowed to be loaded by the current user.
//
// This throttle never defers starting the URL request or following redirects.
// If any of the checks for the original URL and redirect chain are not complete
// by the time the response headers are available, the request is deferred
// until all the checks are done. It cancels the load if any URLs turn out to
// be bad.
//
// Lifetime: Temporary. Created and destroyed for every URL request.
class AwSupervisedUserThrottle : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<AwSupervisedUserThrottle> Create(
      AwSupervisedUserUrlClassifier* bridge);

  explicit AwSupervisedUserThrottle(
      AwSupervisedUserUrlClassifier* url_classifier);
  AwSupervisedUserThrottle(const AwSupervisedUserThrottle&) = delete;
  AwSupervisedUserThrottle& operator=(const AwSupervisedUserThrottle&) = delete;
  ~AwSupervisedUserThrottle() override;

  // blink::URLLoaderThrottle:
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

 private:
  void CheckShouldBlockUrl(const GURL& url);
  void OnShouldBlockUrlResult(bool shouldBlockUrl);

  bool deferred_ = false;
  bool blocked_ = false;
  size_t pending_checks_ = 0;

  const raw_ptr<AwSupervisedUserUrlClassifier> url_classifier_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AwSupervisedUserThrottle> weak_factory_{this};
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_THROTTLE_H_
