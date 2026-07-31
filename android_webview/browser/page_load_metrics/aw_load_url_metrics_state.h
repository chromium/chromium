// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_LOAD_URL_METRICS_STATE_H_
#define ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_LOAD_URL_METRICS_STATE_H_

#include "base/time/time.h"
#include "content/public/browser/web_contents_user_data.h"

namespace android_webview {

// Stores the timestamp of the AwContents.loadUrl API call. This state is
// attached to the WebContents so that it can be retrieved by
// AwLoadUrlMetricsObserver. It is only used by AwLoadUrlMetricsObserver.
class LoadUrlMetricsState
    : public content::WebContentsUserData<LoadUrlMetricsState> {
 public:
  ~LoadUrlMetricsState() override;

  base::TimeTicks load_url_timestamp() const { return load_url_timestamp_; }

 private:
  friend class content::WebContentsUserData<LoadUrlMetricsState>;

  LoadUrlMetricsState(content::WebContents* web_contents,
                      base::TimeTicks load_url_timestamp);

  // The timestamp of the AwContents.loadUrl API call.
  base::TimeTicks load_url_timestamp_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PAGE_LOAD_METRICS_AW_LOAD_URL_METRICS_STATE_H_
