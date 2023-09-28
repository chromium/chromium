// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/supervised_user/aw_supervised_user_throttle.h"

#include "base/check_op.h"
#include "services/network/public/cpp/resource_request.h"

namespace {

const char kCancelReason[] = "SupervisedUserThrottle";

}  // anonymous namespace

namespace android_webview {

// static
std::unique_ptr<AwSupervisedUserThrottle> AwSupervisedUserThrottle::Create(
    AwSupervisedUserUrlClassifier* url_classifier) {
  return base::WrapUnique<AwSupervisedUserThrottle>(
      new AwSupervisedUserThrottle(url_classifier));
}

AwSupervisedUserThrottle::AwSupervisedUserThrottle(
    AwSupervisedUserUrlClassifier* url_classifier)
    : url_classifier_(url_classifier) {
  DCHECK(url_classifier_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AwSupervisedUserThrottle::~AwSupervisedUserThrottle() = default;

void AwSupervisedUserThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  pending_checks_++;
  CheckShouldBlockUrl(request->url);
}

void AwSupervisedUserThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_request_headers,
    net::HttpRequestHeaders* modified_request_headers,
    net::HttpRequestHeaders* modified_cors_exempt_request_headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (blocked_) {
    // onShouldBlockUrlResult() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  pending_checks_++;
  CheckShouldBlockUrl(redirect_info->new_url);
}

void AwSupervisedUserThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (blocked_) {
    // onShouldBlockUrlResult() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  if (pending_checks_ == 0) {
    return;
  }

  DCHECK(!deferred_);
  deferred_ = true;
  *defer = true;
}

void AwSupervisedUserThrottle::CheckShouldBlockUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_classifier_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AwSupervisedUserUrlClassifier::ShouldBlockUrl,
          base::Unretained(url_classifier_), url,
          base::BindOnce(&AwSupervisedUserThrottle::OnShouldBlockUrlResult,
                         weak_factory_.GetWeakPtr())));
}

void AwSupervisedUserThrottle::OnShouldBlockUrlResult(bool shouldBlockUrl) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!blocked_);
  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (shouldBlockUrl) {
    blocked_ = true;
    pending_checks_ = 0;

    DCHECK(delegate_);
    delegate_->CancelWithError(net::ERR_ACCESS_DENIED, kCancelReason);
  } else {
    if (pending_checks_ == 0 && deferred_) {
      deferred_ = false;
      delegate_->Resume();
    }
  }
}

}  // namespace android_webview
