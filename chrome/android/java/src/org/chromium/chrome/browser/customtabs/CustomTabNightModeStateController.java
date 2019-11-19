// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.support.v7.app.AppCompatDelegate;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.PowerSavingModeMonitor;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Maintains and provides the night mode state for {@link CustomTabActivity}.
 */
public class CustomTabNightModeStateController implements Destroyable, NightModeStateProvider {
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final PowerSavingModeMonitor mPowerSavingModeMonitor;
    private final SystemNightModeMonitor mSystemNightModeMonitor;
    private final SystemNightModeMonitor.Observer mSystemNightModeObserver = this::updateNightMode;
    private final Runnable mPowerSaveModeObserver = this::updateNightMode;

    /**
     * The color scheme requested for the CCT. Only {@link CustomTabsIntent#COLOR_SCHEME_LIGHT}
     * and {@link CustomTabsIntent#COLOR_SCHEME_DARK} should be considered - fall back to the
     * system status for {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM} when enabled.
     */
    private int mRequestedColorScheme;
    private AppCompatDelegate mAppCompatDelegate;

    @Nullable // Null initially, so that the first update is always applied (see updateNightMode()).
    private Boolean mIsInNightMode;

    CustomTabNightModeStateController(ActivityLifecycleDispatcher lifecycleDispatcher,
            SystemNightModeMonitor systemNightModeMonitor,
            PowerSavingModeMonitor powerSavingModeMonitor) {
        mSystemNightModeMonitor = systemNightModeMonitor;
        mPowerSavingModeMonitor = powerSavingModeMonitor;
        lifecycleDispatcher.register(this);
    }

    /**
     * Initializes the initial night mode state.
     * @param delegate The {@link AppCompatDelegate} that controls night mode state in support
     *                 library.
     * @param intent  The {@link Intent} to retrieve information about the initial state.
     */
    void initialize(AppCompatDelegate delegate, Intent intent) {
        if (!NightModeUtils.isNightModeSupported()
                || !FeatureUtilities.isNightModeForCustomTabsAvailable()) {
            // Always stay in light mode if night mode is not available.
            mRequestedColorScheme = CustomTabsIntent.COLOR_SCHEME_LIGHT;
            return;
        }

        mRequestedColorScheme = IntentUtils.safeGetIntExtra(
                intent, CustomTabsIntent.EXTRA_COLOR_SCHEME, CustomTabsIntent.COLOR_SCHEME_SYSTEM);
        mAppCompatDelegate = delegate;

        updateNightMode();

        // No need to observe system settings if the intent specifies a light/dark color scheme.
        if (mRequestedColorScheme == CustomTabsIntent.COLOR_SCHEME_SYSTEM) {
            mSystemNightModeMonitor.addObserver(mSystemNightModeObserver);
            mPowerSavingModeMonitor.addObserver(mPowerSaveModeObserver);
        }
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        mSystemNightModeMonitor.removeObserver(mSystemNightModeObserver);
        mPowerSavingModeMonitor.removeObserver(mPowerSaveModeObserver);
    }

    // NightModeStateProvider implementation.
    @Override
    public boolean isInNightMode() {
        return mIsInNightMode != null && mIsInNightMode;
    }

    @Override
    public void addObserver(@NonNull Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(@NonNull Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public boolean shouldOverrideConfiguration() {
        // Don't override configuration because the initial night mode state is only available
        // during CustomTabActivity#onCreate().
        return false;
    }

    private void updateNightMode() {
        boolean shouldBeInNightMode = shouldBeInNightMode();
        if (mIsInNightMode != null && mIsInNightMode == shouldBeInNightMode) return;

        mIsInNightMode = shouldBeInNightMode;
        mAppCompatDelegate.setLocalNightMode(mIsInNightMode ? AppCompatDelegate.MODE_NIGHT_YES
                                                             : AppCompatDelegate.MODE_NIGHT_NO);
        for (Observer observer : mObservers) {
            observer.onNightModeStateChanged();
        }
    }

    private boolean shouldBeInNightMode() {
        switch (mRequestedColorScheme) {
            case CustomTabsIntent.COLOR_SCHEME_LIGHT:
                return false;
            case CustomTabsIntent.COLOR_SCHEME_DARK:
                return true;
            default:
                return mSystemNightModeMonitor.isSystemNightModeOn() ||
                        mPowerSavingModeMonitor.powerSavingIsOn();
        }
    }
}
