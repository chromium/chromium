// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bottombar;

import android.app.Activity;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;

/**
 * Tracks whether the user's bottom bar Settings choice still matches the state the owning
 * activity's UI was created with, and triggers activity recreation when it diverges. Only the
 * Settings choice is tracked; eligibility changes (e.g. form factor) are left to the owner. The
 * bottom bar is wired into the activity's layout and toolbar during pre-inflation startup, so the
 * owner is recreated rather than toggling the bar in place.
 *
 * <p>A visible activity ({@link ActivityState#STARTED}, {@link ActivityState#RESUMED}, or {@link
 * ActivityState#PAUSED}) is recreated immediately when the preference flips. A stopped activity
 * (e.g. behind Settings, which shares the main thread) defers recreation by {@link
 * #RECREATE_SETTLE_DELAY_MS} so the relaunch does not stall the Settings switch animation, with
 * {@link #onStartWithNative()} as a fallback if the user returns before the delay elapses.
 * Re-checking {@link #hasEnabledStateChanged()} when the delayed task or {@link
 * #onStartWithNative()} runs makes flipping the toggle and flipping it back a no-op.
 */
@NullMarked
public class BottomBarEnabledChangeObserver
        implements OnSharedPreferenceChangeListener, StartStopWithNativeObserver, DestroyObserver {
    /**
     * Delay before recreating a stopped activity for a bottom bar toggle, so the relaunch does not
     * land while the Settings switch is still animating. SwitchCompat's thumb animation is 250 ms
     * and the preference row ripple is ~300 ms; 500 ms covers both with headroom.
     */
    @VisibleForTesting static final long RECREATE_SETTLE_DELAY_MS = 500;

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final boolean mInitialEnabledState;
    private final Runnable mRecreateCallback;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mDelayedRecreateRunnable = this::recreateIfEnabledStateChanged;

    /**
     * Creates the observer and starts listening for preference and lifecycle changes. If the
     * current enabled state already differs from {@code initialEnabledState}, triggers recreation
     * immediately.
     *
     * @param activity The activity whose bottom bar state is being observed.
     * @param lifecycleDispatcher Used to observe {@link #onStartWithNative()} and {@link
     *     #onDestroy()}.
     * @param initialEnabledState Whether the bottom bar was enabled when the activity's UI was
     *     created.
     * @param recreateCallback Invoked when the activity should be recreated.
     */
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    public BottomBarEnabledChangeObserver(
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            boolean initialEnabledState,
            Runnable recreateCallback) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mInitialEnabledState = initialEnabledState;
        mRecreateCallback = recreateCallback;
        mLifecycleDispatcher.register(this);
        ContextUtils.getAppSharedPreferences().registerOnSharedPreferenceChangeListener(this);
        recreateIfEnabledStateChanged();
    }

    /**
     * Returns whether the user's Settings choice currently differs from the state at activity
     * creation. Eligibility is not re-checked: the owner only creates this observer when the bottom
     * bar is eligible, and this observer reacts only to the Settings switch.
     */
    @VisibleForTesting
    boolean hasEnabledStateChanged() {
        return BottomBarConfigUtils.isBottomBarUserEnabled() != mInitialEnabledState;
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences prefs, @Nullable String key) {
        if (!ChromePreferenceKeys.BOTTOM_BAR_ENABLED.equals(key)) return;
        mHandler.removeCallbacks(mDelayedRecreateRunnable);
        if (!hasEnabledStateChanged()) return;

        @ActivityState int state = ApplicationStatus.getStateForActivity(mActivity);
        boolean isVisible =
                state == ActivityState.RESUMED
                        || state == ActivityState.PAUSED
                        || state == ActivityState.STARTED;
        if (isVisible) {
            recreateIfEnabledStateChanged();
            return;
        }
        mHandler.postDelayed(mDelayedRecreateRunnable, RECREATE_SETTLE_DELAY_MS);
    }

    @Override
    public void onStartWithNative() {
        recreateIfEnabledStateChanged();
    }

    @Override
    public void onStopWithNative() {
        // Intentionally a no-op: a stopped activity keeps listening for preference changes and
        // re-checks the enabled state in onStartWithNative().
    }

    @Override
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    public void onDestroy() {
        mHandler.removeCallbacks(mDelayedRecreateRunnable);
        mLifecycleDispatcher.unregister(this);
        ContextUtils.getAppSharedPreferences().unregisterOnSharedPreferenceChangeListener(this);
    }

    private void recreateIfEnabledStateChanged() {
        mHandler.removeCallbacks(mDelayedRecreateRunnable);
        if (mActivity.isFinishing() || mActivity.isDestroyed()) return;
        if (!hasEnabledStateChanged()) return;
        onDestroy();
        mRecreateCallback.run();
    }
}
