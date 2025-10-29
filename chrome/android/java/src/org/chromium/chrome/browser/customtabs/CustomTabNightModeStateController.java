// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;

import androidx.appcompat.app.AppCompatDelegate;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;

/** Maintains and provides the night mode state for {@link CustomTabActivity}. */
@NullMarked
public class CustomTabNightModeStateController implements DestroyObserver, NightModeStateProvider {
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final SystemNightModeMonitor.Observer mSystemNightModeObserver = this::updateNightMode;

    /**
     * The color scheme requested for the CCT. Only {@link CustomTabsIntent#COLOR_SCHEME_LIGHT}
     * and {@link CustomTabsIntent#COLOR_SCHEME_DARK} should be considered - fall back to the
     * system status for {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM} when enabled.
     */
    private int mRequestedColorScheme;

    private @MonotonicNonNull AppCompatDelegate mAppCompatDelegate;

    @Nullable // Null initially, so that the first update is always applied (see updateNightMode()).
    private Boolean mIsInNightMode;

    CustomTabNightModeStateController(ActivityLifecycleDispatcher lifecycleDispatcher) {
        lifecycleDispatcher.register(this);
    }

    /**
     * Initializes the initial night mode state.
     * @param delegate The {@link AppCompatDelegate} that controls night mode state in support
     *                 library.
     * @param intent  The {@link Intent} to retrieve information about the initial state.
     */
    void initialize(AppCompatDelegate delegate, Intent intent) {
        if (!NightModeUtils.isNightModeSupported()) {
            // Always stay in light mode if night mode is not available.
            mRequestedColorScheme = CustomTabsIntent.COLOR_SCHEME_LIGHT;
            return;
        }

        mRequestedColorScheme = getRequestedColorScheme(intent);
        mAppCompatDelegate = delegate;

        updateNightMode();

        // No need to observe system settings if the intent specifies a light/dark color scheme.
        if (mRequestedColorScheme == CustomTabsIntent.COLOR_SCHEME_SYSTEM) {
            SystemNightModeMonitor.getInstance().addObserver(mSystemNightModeObserver);
        }
    }

    private int getRequestedColorScheme(Intent intent) {
        return IntentUtils.safeGetIntExtra(
                intent, CustomTabsIntent.EXTRA_COLOR_SCHEME, CustomTabsIntent.COLOR_SCHEME_SYSTEM);
    }

    // DestroyObserver implementation.
    @Override
    public void onDestroy() {
        SystemNightModeMonitor.getInstance().removeObserver(mSystemNightModeObserver);
    }

    // NightModeStateProvider implementation.
    @Override
    public boolean isInNightMode() {
        return mIsInNightMode != null && mIsInNightMode;
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public boolean shouldOverrideConfiguration() {
        // Don't override configuration because the initial night mode state is only available
        // during CustomTabActivity#onCreate().
        return false;
    }

    @RequiresNonNull({"mAppCompatDelegate"})
    private void updateNightMode() {
        boolean shouldBeInNightMode = shouldBeInNightMode();
        if (mIsInNightMode != null && mIsInNightMode == shouldBeInNightMode) return;

        mIsInNightMode = shouldBeInNightMode;
        mAppCompatDelegate.setLocalNightMode(
                mIsInNightMode
                        ? AppCompatDelegate.MODE_NIGHT_YES
                        : AppCompatDelegate.MODE_NIGHT_NO);
        for (Observer observer : mObservers) {
            observer.onNightModeStateChanged();
        }
    }

    private boolean shouldBeInNightMode() {
        return switch (mRequestedColorScheme) {
            case CustomTabsIntent.COLOR_SCHEME_LIGHT -> false;
            case CustomTabsIntent.COLOR_SCHEME_DARK -> true;
            default -> SystemNightModeMonitor.getInstance().isSystemNightModeOn();
        };
    }
}
