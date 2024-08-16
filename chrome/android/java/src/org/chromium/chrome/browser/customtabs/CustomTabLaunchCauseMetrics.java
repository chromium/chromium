// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.flags.ActivityType;

/** LaunchCauseMetrics for CustomTabActivity. */
public class CustomTabLaunchCauseMetrics extends LaunchCauseMetrics {
    private final CustomTabActivity mActivity;

    public CustomTabLaunchCauseMetrics(CustomTabActivity activity) {
        super(activity);
        mActivity = activity;
    }

    @Override
    public @LaunchCause int computeIntentLaunchCause() {
        int type = mActivity.getActivityType();
        if (type == ActivityType.TRUSTED_WEB_ACTIVITY) {
            return LaunchCause.TWA;
        }
        if (type == ActivityType.CUSTOM_TAB) {
            return LaunchCause.CUSTOM_TAB;
        }
        return LaunchCause.AUTH_TAB;
    }
}
