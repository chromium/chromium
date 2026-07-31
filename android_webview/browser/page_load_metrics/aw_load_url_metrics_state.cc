// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/page_load_metrics/aw_load_url_metrics_state.h"

namespace android_webview {

LoadUrlMetricsState::LoadUrlMetricsState(content::WebContents* web_contents,
                                         base::TimeTicks load_url_timestamp)
    : content::WebContentsUserData<LoadUrlMetricsState>(*web_contents),
      load_url_timestamp_(load_url_timestamp) {}

LoadUrlMetricsState::~LoadUrlMetricsState() = default;

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoadUrlMetricsState);

}  // namespace android_webview
