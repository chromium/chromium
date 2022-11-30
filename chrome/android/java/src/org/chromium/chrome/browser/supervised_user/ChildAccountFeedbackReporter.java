// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import android.app.Activity;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.feedback.ChromeFeedbackCollector;
import org.chromium.chrome.browser.feedback.FeedbackReporter;
import org.chromium.chrome.browser.feedback.ScreenshotTask;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java implementation of ChildAccountFeedbackReporterAndroid.
 */
public final class ChildAccountFeedbackReporter {
    private static FeedbackReporter sFeedbackReporter;

    public static void reportFeedback(
            Activity activity, String description, String url, Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sFeedbackReporter == null) {
            sFeedbackReporter = AppHooks.get().createFeedbackReporter();
        }

        new ChromeFeedbackCollector(activity, null /* categoryTag */, description,
                new ScreenshotTask(activity),
                new ChromeFeedbackCollector.InitParams(profile, url, null),
                collector -> { sFeedbackReporter.reportFeedback(collector); }, profile);
    }

    @CalledByNative
    public static void reportFeedbackWithWindow(
            WindowAndroid window, String description, String url, Profile profile) {
        reportFeedback(window.getActivity().get(), description, url, profile);
    }

    private ChildAccountFeedbackReporter() {}
}
