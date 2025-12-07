// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;

/**
 * Basic implementation for displaying help support for Chrome.
 *
 * <p>NOTE: This class is designed to be replaced by downstream targets.
 */
@NullMarked
public class FallbackHelpAndFeedbackLauncherDelegate implements HelpAndFeedbackLauncherDelegate {
    private static final String TAG = "HelpAndFeedback";

    @Override
    public void show(Activity activity, String helpContext, FeedbackCollector collector) {
        Log.d(TAG, "Feedback data: " + collector.getBundle());
        HelpAndFeedbackLauncherDelegate.launchFallbackSupportUri(activity);
    }

    @Override
    public void showFeedback(Activity activity, FeedbackCollector collector) {
        Log.d(TAG, "Feedback data: " + collector.getBundle());
        HelpAndFeedbackLauncherDelegate.launchFallbackSupportUri(activity);
    }
}
