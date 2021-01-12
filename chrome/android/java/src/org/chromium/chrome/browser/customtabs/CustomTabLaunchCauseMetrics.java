// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.metrics.LaunchCauseMetrics;

/**
 * LaunchCauseMetrics for CustomTabActivity.
 */
public class CustomTabLaunchCauseMetrics extends LaunchCauseMetrics {
    private final CustomTabActivity mActivity;

    public CustomTabLaunchCauseMetrics(CustomTabActivity activity) {
        mActivity = activity;
    }

    @Override
    public @LaunchCause int computeLaunchCause() {
        if (mActivity.getActivityType() == ActivityType.TRUSTED_WEB_ACTIVITY) {
            return LaunchCause.TWA;
        }
        assert mActivity.getActivityType() == ActivityType.CUSTOM_TAB;
        return LaunchCause.CUSTOM_TAB;
    }
}
