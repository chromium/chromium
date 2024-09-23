// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BooleanSupplier;

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
    @VisibleForTesting static final String PACKAGE_NAME_EMPTY_1P = "1p";
    private final CustomTabActivityNavigationController mNavigationController;
    private final BooleanSupplier mIsCctFinishing;
    private final BrowserServicesIntentDataProvider mIntent;

    // Getting the package name from the Intent only works when the client is still connected.
    @Nullable private final String mCachedPackageName;

    private long mOnStartTimestampMs;

    // This should be kept in sync with the definition |CustomTabsCloseCause|
    // in tools/metrics/histograms/enums.xml.
    @IntDef({
        CloseCause.USER_ACTION_CHROME,
        CloseCause.USER_ACTION_ANDROID,
        CloseCause.AUTOCLOSE,
        CloseCause.AUTH_TAB
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface CloseCause {
        int USER_ACTION_CHROME = 0;
        int USER_ACTION_ANDROID = 1;
        int AUTOCLOSE = 2;
        int AUTH_TAB = 3;
        int COUNT = 4;
    }

    private @CloseCause int mCloseCause;

    public CustomTabsOpenTimeRecorder(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CustomTabActivityNavigationController navigationController,
            BooleanSupplier isCctFinishing,
            BrowserServicesIntentDataProvider intent) {
        lifecycleDispatcher.register(this);
        mNavigationController = navigationController;
        mIsCctFinishing = isCctFinishing;
        mIntent = intent;
        mCachedPackageName = mIntent.getClientPackageName();
    }

    @Override
    public void onStartWithNative() {
        mOnStartTimestampMs = SystemClock.elapsedRealtime();
        mCloseCause = CloseCause.AUTOCLOSE;
    }

    @Override
    public void onStopWithNative() {
        assert mOnStartTimestampMs != 0;
        if (mCloseCause == CloseCause.AUTOCLOSE && mIntent.isAuthTab()) {
            mCloseCause = CloseCause.AUTH_TAB;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.CloseCause", mCloseCause, CloseCause.COUNT);

        long duration = SystemClock.elapsedRealtime() - mOnStartTimestampMs;
        // Additional check with |mIsCctFinishing| can eliminate some false positives.
        // See Javadoc for more details.
        if (mCloseCause == CloseCause.AUTOCLOSE && mIsCctFinishing.getAsBoolean()) {
            RecordHistogram.recordLongTimesHistogram(
                    "CustomTabs.AutoclosedSessionDuration", duration);
        }

        if (mIsCctFinishing.getAsBoolean()) {
            long time = System.currentTimeMillis() / DateUtils.SECOND_IN_MILLIS;
            boolean wasUserClose =
                    mCloseCause != CloseCause.AUTOCLOSE && mCloseCause != CloseCause.AUTH_TAB;
            boolean isPartial = mIntent.isPartialCustomTab();

            long recordDuration = Math.min(duration / DateUtils.SECOND_IN_MILLIS, 300);
            // For the real implementation, there'll be a native method on this class or a new
            // class entirely. Just for the proof-of-concept I tacked the native method onto another
            // class that already have natives.
            CustomTabsOpenTimeRecorderJni.get()
                    .recordCustomTabSession(
                            time,
                            getPackageName(isPartial),
                            recordDuration,
                            wasUserClose,
                            isPartial);
        }

        mOnStartTimestampMs = 0;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    String getPackageName(boolean isPartial) {
        boolean isEmpty = TextUtils.isEmpty(mCachedPackageName);
        if (isPartial && isEmpty) {
            // Return a non-empty name for trusted intents.
            if (mIntent.isOpenedByChrome()) {
                return ContextUtils.getApplicationContext().getPackageName();
            }
            if (mIntent.isTrustedIntent()) return PACKAGE_NAME_EMPTY_1P;
        }
        return isEmpty ? "" : mCachedPackageName;
    }

    void updateCloseCause() {
        @FinishReason int finishReason = mNavigationController.getFinishReason();
        if (finishReason == FinishReason.USER_NAVIGATION
                || finishReason == FinishReason.REPARENTING
                || finishReason == FinishReason.OPEN_IN_BROWSER) {
            mCloseCause = CloseCause.USER_ACTION_CHROME;
        }
    }

    void onUserLeaveHint() {
        mCloseCause = CloseCause.USER_ACTION_ANDROID;
    }

    @NativeMethods
    interface Natives {
        void recordCustomTabSession(
                long time,
                String packageName,
                long sessionDuration,
                boolean wasUserClosed,
                boolean isPartialCct);
    }
}
