// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_SAME_ORIGIN_OBSERVER_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_SAME_ORIGIN_OBSERVER_H_

#include "base/functional/callback.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/origin.h"

namespace content {
class WebContents;
}

// This observer class will trigger the provided callback whenever the observed
// WebContents's origin either now or no longer matches the provided origin.
// This will not trigger the callback until the navigation has been committed,
// so that WebContents::GetLastCommittedURL will return the new origin, and thus
// allow for easier code re-use. Note that that Loading hasn't actually started
// yet, so this is still suitable for listening to for, e.g., terminating a tab
// capture when a site is no longer the same origin.
class SameOriginObserver : public content::WebContentsObserver {
 public:
  SameOriginObserver(content::WebContents* observed_contents,
                     const url::Origin& reference_origin,
                     base::RepeatingCallback<void(content::WebContents*)>
                         on_same_origin_state_changed);
  ~SameOriginObserver() override;

  // WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override;

 private:
  const url::Origin reference_origin_;
  base::RepeatingCallback<void(content::WebContents*)>
      on_same_origin_state_changed_;
  bool is_same_origin_ = false;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_SAME_ORIGIN_OBSERVER_H_
