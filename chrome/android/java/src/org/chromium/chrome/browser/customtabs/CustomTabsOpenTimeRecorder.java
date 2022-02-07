// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records how long CCTs are open. Only logs if CCTs are closed automatically
 * without user intervention.
 */
class CustomTabsOpenTimeRecorder implements StartStopWithNativeObserver {
    private final CustomTabActivityNavigationController mNavigationController;

    private long mOnStartTimestampMs;

    // This should be kept in sync with the definition |CustomTabsCloseCause|
    // in tools/metrics/histograms/enums.xml.
    @IntDef({CloseCause.USER_ACTION_CHROME, CloseCause.USER_ACTION_ANDROID, CloseCause.AUTOCLOSE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CloseCause {
        int USER_ACTION_CHROME = 0;
        int USER_ACTION_ANDROID = 1;
        int AUTOCLOSE = 2;
        int COUNT = 3;
    }

    private @CloseCause int mCloseCause;

    public CustomTabsOpenTimeRecorder(ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabActivityNavigationController navigationController) {
        lifecycleDispatcher.register(this);
        mNavigationController = navigationController;
    }

    @Override
    public void onStartWithNative() {
        mOnStartTimestampMs = SystemClock.elapsedRealtime();
        mCloseCause = CloseCause.AUTOCLOSE;
    }

    @Override
    public void onStopWithNative() {
        assert mOnStartTimestampMs != 0;
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.CloseCause", mCloseCause, CloseCause.COUNT);
        if (mCloseCause == CloseCause.AUTOCLOSE) {
            long duration = SystemClock.elapsedRealtime() - mOnStartTimestampMs;
            RecordHistogram.recordLongTimesHistogram(
                    "CustomTabs.AutoclosedSessionDuration", duration);
        }
        mOnStartTimestampMs = 0;
    }

    void updateCloseCause() {
        if (mNavigationController.getFinishReason() == FinishReason.USER_NAVIGATION) {
            mCloseCause = CloseCause.USER_ACTION_CHROME;
        }
    }

    void onUserLeaveHint() {
        mCloseCause = CloseCause.USER_ACTION_ANDROID;
    }
}
