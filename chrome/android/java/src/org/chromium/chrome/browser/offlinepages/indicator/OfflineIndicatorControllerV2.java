// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.status_indicator.StatusIndicatorCoordinator;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.content_public.common.ContentSwitches;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that controls visibility and content of {@link StatusIndicatorCoordinator} to relay
 * connectivity information.
 */
public class OfflineIndicatorControllerV2 {
    @IntDef({
        UmaEnum.CAN_ANIMATE_NATIVE_CONTROLS,
        UmaEnum.CAN_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED,
        UmaEnum.CANNOT_ANIMATE_NATIVE_CONTROLS,
        UmaEnum.CANNOT_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface UmaEnum {
        int CAN_ANIMATE_NATIVE_CONTROLS = 0;
        int CAN_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED = 1;
        int CANNOT_ANIMATE_NATIVE_CONTROLS = 2;
        int CANNOT_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED = 3;

        int NUM_ENTRIES = 4;
    }

    static final long STATUS_INDICATOR_WAIT_BEFORE_HIDE_DURATION_MS = 2000;

    // TODO(tbansal): Consider moving the cooldown logic to OfflineDetector.java.
    // The cooldown period was added to prevent showing/changing the indicator too frequently. In
    // longer term, OfflineDetector should protect against sending signals to this class too
    // frequently.
    static final long STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS = 5000;

    public static final String OFFLINE_INDICATOR_SHOWN_DURATION_V2 =
            "OfflineIndicator.ShownDurationV2";

    @SuppressLint("StaticFieldLeak")
    private static OfflineDetector sMockOfflineDetector;

    private static Supplier<Long> sMockElapsedTimeSupplier;
    private static OfflineIndicatorMetricsDelegate sMockOfflineIndicatorMetricsDelegate;

    private Context mContext;
    private StatusIndicatorCoordinator mStatusIndicator;
    private Handler mHandler;
    private OfflineDetector mOfflineDetector;
    private ObservableSupplier<Boolean> mIsUrlBarFocusedSupplier;
    private Supplier<Boolean> mCanAnimateBrowserControlsSupplier;
    private Callback<Boolean> mOnUrlBarFocusChanged;
    private Runnable mShowRunnable;
    private Runnable mUpdateAndHideRunnable;
    private Runnable mHideRunnable;
    private Runnable mOnUrlBarUnfocusedRunnable;
    private Runnable mUpdateStatusIndicatorDelayedRunnable;
    private long mLastActionTime;
    private boolean mIsOffline;
    private boolean mIsOfflineStateInitialized;
    private boolean mIsForeground;
    private OfflineIndicatorMetricsDelegate mMetricsDelegate;

    /**
     * Constructs the offline indicator.
     * @param context The {@link Context}.
     * @param statusIndicator The {@link StatusIndicatorCoordinator} instance this controller will
     *                        control based on the connectivity.
     * @param isUrlBarFocusedSupplier The {@link ObservableSupplier} that will supply the UrlBar's
     *                                focus state and notify a listener when it changes.
     * @param canAnimateNativeBrowserControls Will supply a boolean meaning whether the native
     *                                        browser controls can be animated. This is used for
     *                                        collecting metrics.
     * TODO(sinansahin): We can remove canAnimateNativeBrowserControls once we're done with metrics
     *                   collection.
     */
    public OfflineIndicatorControllerV2(
            Context context,
            StatusIndicatorCoordinator statusIndicator,
            ObservableSupplier<Boolean> isUrlBarFocusedSupplier,
            Supplier<Boolean> canAnimateNativeBrowserControls) {
        if (CommandLine.getInstance()
                .hasSwitch(ContentSwitches.FORCE_ONLINE_CONNECTION_STATE_FOR_INDICATOR)) {
            // If "force online connection state" switch is set, the offline indicator should never
            // show.
            return;
        }

        mContext = context;
        mStatusIndicator = statusIndicator;
        mHandler = new Handler();

        if (sMockOfflineIndicatorMetricsDelegate != null) {
            mMetricsDelegate = sMockOfflineIndicatorMetricsDelegate;
        } else {
            mMetricsDelegate = new OfflineIndicatorMetricsDelegate();
        }

        // If we're offline at start-up, we should have a small enough last action time so that we
        // don't wait for the cool-down.
        mLastActionTime = getElapsedTime() - STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS;
        if (sMockOfflineDetector != null) {
            mOfflineDetector = sMockOfflineDetector;
        } else {
            mOfflineDetector =
                    new OfflineDetector(
                            (Boolean offline) -> onConnectionStateChanged(offline),
                            (Boolean isForeground) -> onApplicationStateChanged(isForeground),
                            mContext);
        }

        // Initializes the application state.
        onApplicationStateChanged(mOfflineDetector.isApplicationForeground());

        mShowRunnable =
                () -> {
                    RecordUserAction.record("OfflineIndicator.Shown");

                    mMetricsDelegate.onIndicatorShown();

                    setLastActionTime();

                    final int backgroundColor =
                            mContext.getColor(R.color.offline_indicator_offline_color);
                    final int textColor = mContext.getColor(R.color.default_text_color_light);
                    final Drawable statusIcon =
                            mContext.getDrawable(R.drawable.ic_cloud_offline_24dp);
                    final int iconTint = mContext.getColor(R.color.default_icon_color_light);
                    mStatusIndicator.show(
                            mContext.getString(R.string.offline_indicator_v2_offline_text),
                            statusIcon,
                            backgroundColor,
                            textColor,
                            iconTint);
                };

        mHideRunnable =
                () -> {
                    mHandler.postDelayed(
                            () -> mStatusIndicator.hide(),
                            STATUS_INDICATOR_WAIT_BEFORE_HIDE_DURATION_MS);
                };

        mUpdateAndHideRunnable =
                () -> {
                    RecordUserAction.record("OfflineIndicator.Hidden");

                    mMetricsDelegate.onIndicatorHidden();

                    setLastActionTime();

                    final int backgroundColor =
                            ChromeSemanticColorUtils.getOfflineIndicatorBackOnlineColor(mContext);
                    final int textColor = SemanticColorUtils.getDefaultTextColorOnAccent1(mContext);
                    final Drawable statusIcon = mContext.getDrawable(R.drawable.ic_globe_24dp);
                    final int iconTint = SemanticColorUtils.getDefaultIconColorInverse(mContext);
                    mStatusIndicator.updateContent(
                            mContext.getString(R.string.offline_indicator_v2_back_online_text),
                            statusIcon,
                            backgroundColor,
                            textColor,
                            iconTint,
                            mHideRunnable);
                };

        mIsUrlBarFocusedSupplier = isUrlBarFocusedSupplier;
        mCanAnimateBrowserControlsSupplier = canAnimateNativeBrowserControls;
        // TODO(crbug.com/40128377): Move the UrlBar focus related code to the widget or glue code.
        mOnUrlBarFocusChanged =
                (hasFocus) -> {
                    if (!hasFocus && mOnUrlBarUnfocusedRunnable != null) {
                        mOnUrlBarUnfocusedRunnable.run();
                        mOnUrlBarUnfocusedRunnable = null;
                    }
                };
        mIsUrlBarFocusedSupplier.addObserver(mOnUrlBarFocusChanged);

        mUpdateStatusIndicatorDelayedRunnable =
                () -> {
                    final boolean offline = mOfflineDetector.isConnectionStateOffline();
                    if (offline != mIsOffline) {
                        updateStatusIndicator(offline);
                    }
                };
    }

    public void onConnectionStateChanged(boolean offline) {
        if (mIsOfflineStateInitialized && mIsOffline == offline) {
            return;
        }

        mHandler.removeCallbacks(mUpdateStatusIndicatorDelayedRunnable);
        // TODO(crbug.com/40691334): This currently only protects the widget from going into a bad
        // state. We need a better way to handle flaky connections.
        final long elapsedTimeSinceLastAction = getElapsedTime() - mLastActionTime;
        if (elapsedTimeSinceLastAction < STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS) {
            mHandler.postDelayed(
                    mUpdateStatusIndicatorDelayedRunnable,
                    STATUS_INDICATOR_COOLDOWN_BEFORE_NEXT_ACTION_MS - elapsedTimeSinceLastAction);
            return;
        }

        updateStatusIndicator(offline);
    }

    public void onApplicationStateChanged(boolean isForeground) {
        if (mIsForeground == isForeground) return;

        if (isForeground) {
            mMetricsDelegate.onAppForegrounded();
        } else {
            mMetricsDelegate.onAppBackgrounded();
        }
        mIsForeground = isForeground;
    }

    public void destroy() {
        if (mOfflineDetector != null) {
            mOfflineDetector.destroy();
            mOfflineDetector = null;
        }

        if (mIsUrlBarFocusedSupplier != null) {
            mIsUrlBarFocusedSupplier.removeObserver(mOnUrlBarFocusChanged);
            mIsUrlBarFocusedSupplier = null;
        }

        mOnUrlBarFocusChanged = null;

        if (mHandler != null) {
            mHandler.removeCallbacks(mHideRunnable);
            mHandler.removeCallbacks(mUpdateStatusIndicatorDelayedRunnable);
        }
    }

    private void updateStatusIndicator(boolean offline) {
        mIsOffline = offline;
        if (!mIsOfflineStateInitialized) {
            mMetricsDelegate.onOfflineStateInitialized(/* isOffline= */ offline);
        }
        if (!mIsOfflineStateInitialized && !offline) {
            mIsOfflineStateInitialized = true;
            return;
        }
        mIsOfflineStateInitialized = true;
        int surfaceState;
        if (mIsUrlBarFocusedSupplier.get()) {
            // We should clear the runnable if we would be assigning an unnecessary show or hide
            // runnable. E.g, without this condition, we would be trying to hide the indicator when
            // it's not shown if we were set to show the widget but then went back online.
            if ((!offline && mOnUrlBarUnfocusedRunnable == mShowRunnable)
                    || (offline && mOnUrlBarUnfocusedRunnable == mUpdateAndHideRunnable)) {
                mOnUrlBarUnfocusedRunnable = null;
                return;
            }
            mOnUrlBarUnfocusedRunnable = offline ? mShowRunnable : mUpdateAndHideRunnable;
            surfaceState =
                    mCanAnimateBrowserControlsSupplier.get()
                            ? UmaEnum.CAN_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED
                            : UmaEnum.CANNOT_ANIMATE_NATIVE_CONTROLS_OMNIBOX_FOCUSED;
        } else {
            assert mOnUrlBarUnfocusedRunnable == null;
            (offline ? mShowRunnable : mUpdateAndHideRunnable).run();
            surfaceState =
                    mCanAnimateBrowserControlsSupplier.get()
                            ? UmaEnum.CAN_ANIMATE_NATIVE_CONTROLS
                            : UmaEnum.CANNOT_ANIMATE_NATIVE_CONTROLS;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "OfflineIndicator.ConnectivityChanged.DeviceState."
                        + (offline ? "Offline" : "Online"),
                surfaceState,
                UmaEnum.NUM_ENTRIES);
    }

    private long getElapsedTime() {
        return sMockElapsedTimeSupplier != null
                ? sMockElapsedTimeSupplier.get()
                : SystemClock.elapsedRealtime();
    }

    private void setLastActionTime() {
        mLastActionTime = getElapsedTime();
    }

    @VisibleForTesting
    static void setMockOfflineDetector(OfflineDetector offlineDetector) {
        sMockOfflineDetector = offlineDetector;
    }

    @VisibleForTesting
    static void setMockElapsedTimeSupplier(Supplier<Long> supplier) {
        sMockElapsedTimeSupplier = supplier;
    }

    void setHandlerForTesting(Handler handler) {
        mHandler = handler;
    }

    @VisibleForTesting
    static void setMockOfflineIndicatorMetricsDelegate(
            OfflineIndicatorMetricsDelegate offlineIndicatorMetricsDelegate) {
        sMockOfflineIndicatorMetricsDelegate = offlineIndicatorMetricsDelegate;
    }
}
