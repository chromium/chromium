// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/same_origin_observer.h"

#include "content/public/browser/web_contents.h"

SameOriginObserver::SameOriginObserver(
    content::WebContents* observed_contents,
    const url::Origin& reference_origin,
    base::RepeatingCallback<void(content::WebContents*)>
        on_same_origin_state_changed)
    : content::WebContentsObserver(observed_contents),
      reference_origin_(reference_origin),
      on_same_origin_state_changed_(on_same_origin_state_changed) {
  DCHECK(observed_contents);
  is_same_origin_ = reference_origin_.IsSameOriginWith(
      observed_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

SameOriginObserver::~SameOriginObserver() = default;

void SameOriginObserver::PrimaryPageChanged(content::Page& page) {
  bool is_now_same_origin = reference_origin_.IsSameOriginWith(
      page.GetMainDocument().GetLastCommittedOrigin());

  if (is_same_origin_ != is_now_same_origin) {
    is_same_origin_ = is_now_same_origin;
    on_same_origin_state_changed_.Run(web_contents());
  }
}
