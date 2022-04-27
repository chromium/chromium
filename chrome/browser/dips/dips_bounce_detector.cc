// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_bounce_detector.h"

#include <vector>

#include "chrome/browser/dips/cookie_access_filter.h"
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
  CookieAccessFilter filter;
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
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  auto* existing_state = static_cast<BounceDetectionState*>(
      navigation_handle->GetUserData(kBounceDetectionStateKey));
  if (existing_state) {
    existing_state->filter.AddAccess(details.url, details.type);
    return;
  }

  auto new_state = std::make_unique<BounceDetectionState>();
  new_state->filter.AddAccess(details.url, details.type);
  navigation_handle->SetUserData(kBounceDetectionStateKey,
                                 std::move(new_state));
}

void DIPSBounceDetector::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // We can be sure OnCookiesAccessed() was called for all redirects at this
  // point.
  auto* state = static_cast<BounceDetectionState*>(
      navigation_handle->GetUserData(kBounceDetectionStateKey));
  if (state) {
    std::vector<size_t> accessor_idxs;
    if (!state->filter.Filter(navigation_handle->GetRedirectChain(),
                              &accessor_idxs)) {
      // We failed to map all the OnCookiesAccessed calls to the redirect chain.
      // TODO(rtarpine): report metrics to see if this happens in practice
      return;
    }
    for (size_t accessor_idx : accessor_idxs) {
      if (accessor_idx == navigation_handle->GetRedirectChain().size() - 1) {
        // the last entry in GetRedirectChain() is the final URL, not actually a
        // redirect.
        continue;
      }
      stateful_redirect_handler_.Run(navigation_handle, accessor_idx);
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DIPSBounceDetector);
