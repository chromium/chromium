// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_THROTTLE_H_
#define ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_THROTTLE_H_

#include "android_webview/browser/supervised_user/aw_supervised_user_url_classifier.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/navigation_throttle.h"
#include "net/http/http_request_headers.h"

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
// Methods on this class should be called on the UI thread. Instances of this
// class can be created on any thread.
//
// Lifetime: Temporary. This is scoped to content::NavigationRequest, which
// lives from navigation start until the navigation has been committed.
class AwSupervisedUserThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<AwSupervisedUserThrottle> Create(
      content::NavigationHandle* navigation_handle,
      AwSupervisedUserUrlClassifier* bridge);

  explicit AwSupervisedUserThrottle(
      content::NavigationHandle* navigation_handle,
      AwSupervisedUserUrlClassifier* url_classifier);
  AwSupervisedUserThrottle(const AwSupervisedUserThrottle&) = delete;
  AwSupervisedUserThrottle& operator=(const AwSupervisedUserThrottle&) = delete;
  ~AwSupervisedUserThrottle() override;

  // content::NavigationThrottle :
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

 private:
  content::NavigationThrottle::ThrottleCheckResult CheckShouldBlockUrl(
      const GURL& url);
  void OnShouldBlockUrlResult(bool shouldBlockUrl);

  bool deferred_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool blocked_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  size_t pending_checks_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  const raw_ptr<AwSupervisedUserUrlClassifier> url_classifier_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AwSupervisedUserThrottle> weak_factory_{this};
};
}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SUPERVISED_USER_AW_SUPERVISED_USER_THROTTLE_H_
