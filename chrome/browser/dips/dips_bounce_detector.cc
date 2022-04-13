// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"

using content::NavigationHandle;

namespace {

// BounceDetectionState gets attached to NavigationHandle (which is a
// SupportsUserData subclass) to store which URLs accessed cookies (since
// WebContentsObserver::OnCookiesAccessed is called in an unpredictable order
// with respect to WCO::DidRedirectNavigation).
class BounceDetectionState : public base::SupportsUserData::Data {
 public:
  // We use a vector rather than a set of URLs because order can matter. If the
  // same URL appears twice in a redirect chain, we might be able to distinguish
  // between them.
  std::vector<GURL> cookie_accessors;
};

const char kBounceDetectionStateKey[] = "BounceDetectionState";

}  // namespace

DIPSBounceDetector::DIPSBounceDetector(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DIPSBounceDetector>(*web_contents),
      // It's safe to use unretained because the callback is owned by this.
      stateful_redirect_handler_(
          base::BindRepeating(&DIPSBounceDetector::HandleStatefulRedirect,
                              base::Unretained(this))) {}

DIPSBounceDetector::~DIPSBounceDetector() = default;

void DIPSBounceDetector::HandleStatefulRedirect(
    content::NavigationHandle* navigation_handle,
    int redirect_index) {
  // TODO: fire UKM metric
}

void DIPSBounceDetector::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  auto* existing_state = static_cast<BounceDetectionState*>(
      navigation_handle->GetUserData(kBounceDetectionStateKey));
  if (existing_state) {
    existing_state->cookie_accessors.push_back(details.url);
    return;
  }

  auto new_state = std::make_unique<BounceDetectionState>();
  new_state->cookie_accessors.push_back(details.url);
  navigation_handle->SetUserData(kBounceDetectionStateKey,
                                 std::move(new_state));
}

void DIPSBounceDetector::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // We can be sure OnCookiesAccessed() was called for all redirects at this
  // point.
  auto* state = static_cast<BounceDetectionState*>(
      navigation_handle->GetUserData(kBounceDetectionStateKey));
  if (state) {
    // Compare GetRedirectChain() to cookie_accessors to determine which
    // redirects accessed cookies.
    //
    // (Note that GetRedirectChain() is guaranteed not to be empty, and the last
    // entry is the final URL, not actually a redirect.)
    for (size_t accessor_idx = 0, redirect_idx = 0;
         accessor_idx < state->cookie_accessors.size() &&
         redirect_idx < navigation_handle->GetRedirectChain().size() - 1;
         redirect_idx++) {
      const auto& url = navigation_handle->GetRedirectChain()[redirect_idx];
      if (url == state->cookie_accessors[accessor_idx]) {
        stateful_redirect_handler_.Run(navigation_handle, redirect_idx);
        ++accessor_idx;
      }
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSBounceDetector);
