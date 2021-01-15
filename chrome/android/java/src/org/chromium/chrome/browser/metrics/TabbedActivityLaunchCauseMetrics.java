// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.app.Activity;
import android.content.Intent;

import org.chromium.base.IntentUtils;

/**
 * LaunchCauseMetrics for ChromeTabbedActivity.
 */
public class TabbedActivityLaunchCauseMetrics extends LaunchCauseMetrics {
    private final Activity mActivity;

    public TabbedActivityLaunchCauseMetrics(Activity activity) {
        super(activity);
        mActivity = activity;
    }

    @Override
    public @LaunchCause int computeLaunchCause() {
        Intent launchIntent = mActivity.getIntent();
        if (launchIntent == null) return LaunchCause.OTHER;

        if (IntentUtils.isMainIntentFromLauncher(launchIntent)) {
            return LaunchCause.MAIN_LAUNCHER_ICON;
        }

        // TODO(https://crbug.com/1163961): Implement remaining ChromeTabbedActivity launch cause
        // metrics.

        return LaunchCause.OTHER;
    }
}
