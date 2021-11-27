// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/same_origin_observer.h"

#include "base/callback.h"
#include "base/check.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

SameOriginObserver::SameOriginObserver(
    content::WebContents* observed_contents,
    const GURL& reference_origin,
    base::RepeatingCallback<void(content::WebContents*)>
        on_same_origin_state_changed)
    : observed_contents_(observed_contents),
      reference_origin_(reference_origin),
      on_same_origin_state_changed_(on_same_origin_state_changed) {
  DCHECK(observed_contents);
  is_same_origin_ = url::IsSameOriginWith(
      reference_origin_,
      observed_contents_->GetLastCommittedURL().DeprecatedGetOriginAsURL());
  Observe(observed_contents);
}

SameOriginObserver::~SameOriginObserver() = default;

void SameOriginObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  const GURL& new_origin =
      navigation_handle->GetURL().DeprecatedGetOriginAsURL();
  bool is_now_same_origin =
      url::IsSameOriginWith(reference_origin_, new_origin);
  if (is_same_origin_ != is_now_same_origin) {
    is_same_origin_ = is_now_same_origin;
    on_same_origin_state_changed_.Run(observed_contents_.get());
  }
}
