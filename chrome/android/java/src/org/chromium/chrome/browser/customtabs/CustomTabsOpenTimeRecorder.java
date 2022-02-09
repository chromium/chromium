// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.BooleanSupplier;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records how long CCTs are open and the cause of closing. The open time is logged only if CCTs
 * are closed automatically without user intervention.
 * <p>CCTs can be closed automatically when the client app closes the CCT by launching another
 * activity with a flag CLEAR_TOP. We can't detect this exactly, but we use following heuristics:
 *
 *   <li> {@link Activity#onUserLeaveHint()} occurs when user presses home button to put CCT
 *        to background.
 *   <li> {@link Activity#isFinishing()} is _likely_ {@code false} at {@link Activity#onStop}
 *        when user enters Recent screen on Android P/Q/R in system gesture navigation mode.
 *
 * <p>The above signals help eliminate some cases that look like autoclosing but are actually
 * user-intervened ones.
 */
class CustomTabsOpenTimeRecorder implements StartStopWithNativeObserver {
    private final CustomTabActivityNavigationController mNavigationController;
    private final BooleanSupplier mIsCctFinishing;

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
            CustomTabActivityNavigationController navigationController,
            BooleanSupplier isCctFinishing) {
        lifecycleDispatcher.register(this);
        mNavigationController = navigationController;
        mIsCctFinishing = isCctFinishing;
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

        // Additional check with |mIsCctFinishing| can eliminate some false positives.
        // See Javadoc for more details.
        if (mCloseCause == CloseCause.AUTOCLOSE && mIsCctFinishing.getAsBoolean()) {
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
