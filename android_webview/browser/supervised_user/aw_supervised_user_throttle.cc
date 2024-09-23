// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/supervised_user/aw_supervised_user_throttle.h"

#include "android_webview/browser/supervised_user/aw_supervised_user_blocking_page.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"

namespace android_webview {

// static
std::unique_ptr<AwSupervisedUserThrottle> AwSupervisedUserThrottle::Create(
    content::NavigationHandle* navigation_handle,
    AwSupervisedUserUrlClassifier* url_classifier) {
  return base::WrapUnique<AwSupervisedUserThrottle>(
      new AwSupervisedUserThrottle(navigation_handle, url_classifier));
}

AwSupervisedUserThrottle::AwSupervisedUserThrottle(
    content::NavigationHandle* navigation_handle,
    AwSupervisedUserUrlClassifier* url_classifier)
    : NavigationThrottle(navigation_handle), url_classifier_(url_classifier) {
  DCHECK(url_classifier_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AwSupervisedUserThrottle::~AwSupervisedUserThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
AwSupervisedUserThrottle::WillStartRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  pending_checks_++;
  return CheckShouldBlockUrl(navigation_handle()->GetURL());
}

content::NavigationThrottle::ThrottleCheckResult
AwSupervisedUserThrottle::WillRedirectRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (blocked_) {
    // onShouldBlockUrlResult() has set |blocked_| to true and called
    // |CancelDeferredNavigation()|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    return NavigationThrottle::DEFER;
  }
  pending_checks_++;
  return CheckShouldBlockUrl(navigation_handle()->GetURL());
}

const char* AwSupervisedUserThrottle::GetNameForLogging() {
  return "AwSupervisedUserThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
AwSupervisedUserThrottle::WillProcessResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (blocked_) {
    // onShouldBlockUrlResult() has set |blocked_| to true and called
    // |CancelDeferredNavigation()|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    return NavigationThrottle::DEFER;
  }

  if (pending_checks_ == 0) {
    return NavigationThrottle::PROCEED;
  }

  DCHECK(!deferred_);
  deferred_ = true;
  return NavigationThrottle::DEFER;
}

content::NavigationThrottle::ThrottleCheckResult
AwSupervisedUserThrottle::CheckShouldBlockUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_classifier_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AwSupervisedUserUrlClassifier::ShouldBlockUrl,
          base::Unretained(url_classifier_), url,
          base::BindOnce(&AwSupervisedUserThrottle::OnShouldBlockUrlResult,
                         weak_factory_.GetWeakPtr())));
  deferred_ = true;
  return NavigationThrottle::DEFER;
}

void AwSupervisedUserThrottle::OnShouldBlockUrlResult(bool shouldBlockUrl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!blocked_);
  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (shouldBlockUrl) {
    blocked_ = true;
    pending_checks_ = 0;

    std::unique_ptr<security_interstitials::SecurityInterstitialPage>
        blocking_page = AwSupervisedUserBlockingPage::CreateBlockingPage(
            navigation_handle()->GetWebContents(),
            navigation_handle()->GetURL());
    std::string error_page_content = blocking_page->GetHTMLContents();
    // AssociateBlockingPage takes ownership of the blocking page.
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(navigation_handle(), std::move(blocking_page));
    CancelDeferredNavigation(content::NavigationThrottle::ThrottleCheckResult(
        CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content));

  } else {
    if (pending_checks_ == 0 && deferred_) {
      deferred_ = false;
      Resume();
    }
  }
}

}  // namespace android_webview
