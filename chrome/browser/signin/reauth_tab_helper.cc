// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/reauth_tab_helper.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/signin/reauth_result.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_status_code.h"
#include "url/origin.h"

namespace signin {

namespace {

bool IsExpectedResponseCode(int response_code) {
  return response_code == net::HTTP_OK || response_code == net::HTTP_NO_CONTENT;
}

}  // namespace

// static
void ReauthTabHelper::CreateForWebContents(content::WebContents* web_contents,
                                           const GURL& reauth_url,
                                           bool restrict_to_reauth_origin,
                                           ReauthCallback callback) {
  DCHECK(web_contents);
  if (!FromWebContents(web_contents)) {
    web_contents->SetUserData(
        UserDataKey(), base::WrapUnique(new ReauthTabHelper(
                           web_contents, reauth_url, restrict_to_reauth_origin,
                           std::move(callback))));
  } else {
    std::move(callback).Run(signin::ReauthResult::kCancelled);
  }
}

ReauthTabHelper::~ReauthTabHelper() = default;

bool ReauthTabHelper::ShouldAllowNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return true;

  if (!restrict_to_reauth_origin_)
    return true;

  return url::IsSameOriginWith(reauth_url_, navigation_handle->GetURL());
}

void ReauthTabHelper::CompleteReauth(signin::ReauthResult result) {
  if (callback_)
    std::move(callback_).Run(result);
}

void ReauthTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!navigation_handle->IsInPrimaryMainFrame())
    return;

  is_within_reauth_origin_ &=
      url::IsSameOriginWith(reauth_url_, navigation_handle->GetURL());

  if (navigation_handle->IsErrorPage()) {
    has_last_committed_error_page_ = true;
    return;
  }

  has_last_committed_error_page_ = false;

  GURL::Replacements replacements;
  replacements.ClearQuery();
  GURL url_without_query =
      navigation_handle->GetURL().ReplaceComponents(replacements);
  if (url_without_query != reauth_url_)
    return;

  if (!navigation_handle->GetResponseHeaders() ||
      !IsExpectedResponseCode(
          navigation_handle->GetResponseHeaders()->response_code())) {
    CompleteReauth(signin::ReauthResult::kUnexpectedResponse);
  }

  CompleteReauth(signin::ReauthResult::kSuccess);
}

void ReauthTabHelper::WebContentsDestroyed() {
  CompleteReauth(signin::ReauthResult::kDismissedByUser);
}

bool ReauthTabHelper::is_within_reauth_origin() {
  return is_within_reauth_origin_;
}

bool ReauthTabHelper::has_last_committed_error_page() {
  return has_last_committed_error_page_;
}

ReauthTabHelper::ReauthTabHelper(content::WebContents* web_contents,
                                 const GURL& reauth_url,
                                 bool restrict_to_reauth_origin,
                                 ReauthCallback callback)
    : content::WebContentsObserver(web_contents),
      reauth_url_(reauth_url),
      restrict_to_reauth_origin_(restrict_to_reauth_origin),
      callback_(std::move(callback)) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ReauthTabHelper)

}  // namespace signin
