// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.content.res.ColorStateList;

import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarManager;

/** Maintains the toolbar color for {@link CustomTabActivity}. */
public class CustomTabToolbarColorController
        implements ThemeColorProvider.ThemeColorObserver, ThemeColorProvider.TintObserver {
    private final BrowserServicesThemeColorProvider mBrowserServicesThemeColorProvider;

    private ToolbarManager mToolbarManager;

    public CustomTabToolbarColorController(
            BrowserServicesThemeColorProvider browserServicesThemeColorProvider) {
        mBrowserServicesThemeColorProvider = browserServicesThemeColorProvider;
        mBrowserServicesThemeColorProvider.addThemeColorObserver(this);
        mBrowserServicesThemeColorProvider.addTintObserver(this);
    }

    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        updateBackgroundColor();
    }

    @Override
    public void onTintChanged(
            @Nullable ColorStateList tint,
            @Nullable ColorStateList activityFocusTint,
            int brandedColorScheme) {
        updateTint();
    }

    /**
     * Notifies the ColorController that the ToolbarManager has been created and is ready for use.
     * ToolbarManager isn't passed directly to the constructor because it's not guaranteed to be
     * initialized yet.
     */
    public void onToolbarInitialized(ToolbarManager manager) {
        mToolbarManager = manager;
        assert manager != null : "Toolbar manager not initialized";

        updateBackgroundColor();
        updateTint();
    }

    private void updateBackgroundColor() {
        if (mToolbarManager == null) return;

        mToolbarManager.setShouldUpdateToolbarPrimaryColor(true);
        mToolbarManager.onThemeColorChanged(
                mBrowserServicesThemeColorProvider.getThemeColor(), false);
        mToolbarManager.setShouldUpdateToolbarPrimaryColor(false);
    }

    private void updateTint() {
        if (mToolbarManager == null) return;

        mToolbarManager.setShouldUpdateToolbarPrimaryColor(true);
        mToolbarManager.onTintChanged(
                mBrowserServicesThemeColorProvider.getTint(),
                mBrowserServicesThemeColorProvider.getActivityFocusTint(),
                mBrowserServicesThemeColorProvider.getBrandedColorScheme());
        mToolbarManager.setShouldUpdateToolbarPrimaryColor(false);
    }
}
