// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;

/**
 * Maintains and provides the night mode state for {@link CustomTabActivity}.
 */
public class CustomTabNightModeStateController implements DestroyObserver {

    /**
     * The color scheme requested for the CCT. Only {@link CustomTabsIntent#COLOR_SCHEME_LIGHT}
     * and {@link CustomTabsIntent#COLOR_SCHEME_DARK} should be considered - fall back to the
     * system status for {@link CustomTabsIntent#COLOR_SCHEME_SYSTEM} when enabled.
     */
    private int mRequestedColorScheme;
    private AppCompatDelegate mAppCompatDelegate;

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

        mRequestedColorScheme = IntentUtils.safeGetIntExtra(
                intent, CustomTabsIntent.EXTRA_COLOR_SCHEME, CustomTabsIntent.COLOR_SCHEME_SYSTEM);
        mAppCompatDelegate = delegate;

    }

    // DestroyObserver implementation.
    @Override
    public void onDestroy() {
    }

}
