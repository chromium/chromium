// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;

import org.chromium.chrome.browser.metrics.LaunchCauseMetrics;

/**
 * LaunchCauseMetrics for WebappActivity.
 */
public class WebappLaunchCauseMetrics extends LaunchCauseMetrics {
    public WebappLaunchCauseMetrics(Activity activity) {
        super(activity);
    }

    @Override
    public @LaunchCause int computeLaunchCause() {
        // TODO(https://crbug.com/1163961): Implement Webapp launch cause metrics.
        return LaunchCause.OTHER;
    }
}
