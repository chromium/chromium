// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.chromium.base.Log;

/**
 * Basic implementation for displaying help support for Chrome.
 *
 * <p>NOTE: This class is designed to be replaced by downstream targets.
 */
public class FallbackHelpAndFeedbackLauncherDelegate implements HelpAndFeedbackLauncherDelegate {
    private static final String TAG = "HelpAndFeedback";

    @Override
    public void show(Activity activity, String helpContext, @NonNull FeedbackCollector collector) {
        Log.d(TAG, "Feedback data: " + collector.getBundle());
        HelpAndFeedbackLauncherDelegate.launchFallbackSupportUri(activity);
    }

    @Override
    public void showFeedback(Activity activity, @NonNull FeedbackCollector collector) {
        Log.d(TAG, "Feedback data: " + collector.getBundle());
        HelpAndFeedbackLauncherDelegate.launchFallbackSupportUri(activity);
    }
}
