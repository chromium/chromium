// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.ui.system.StatusBarColorController.UNDEFINED_STATUS_BAR_COLOR;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;

import javax.inject.Inject;

/**
 * Manages the status bar color for a CustomTabActivity.
 */
@ActivityScope
public class CustomTabStatusBarColorProvider {
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final ActivityTabProvider mActivityTabProvider;
    private final StatusBarColorController mStatusBarColorController;

    private boolean mUseTabThemeColor;

    @Inject
    public CustomTabStatusBarColorProvider(CustomTabIntentDataProvider intentDataProvider,
            ActivityTabProvider activityTabProvider,
            StatusBarColorController statusBarColorController) {
        mIntentDataProvider = intentDataProvider;
        mActivityTabProvider = activityTabProvider;
        mStatusBarColorController = statusBarColorController;
    }

    /**
     * Sets whether the tab's theme color should be used for the status bar and triggers an update
     * of the status bar color if needed.
     */
    public void setUseTabThemeColor(boolean useTabThemeColor) {
        if (mUseTabThemeColor == useTabThemeColor) return;

        mUseTabThemeColor = useTabThemeColor;
        mStatusBarColorController.updateStatusBarColor(mActivityTabProvider.get());
    }

    int getBaseStatusBarColor(int fallbackStatusBarColor) {
        if (mIntentDataProvider.isOpenedByChrome()) return fallbackStatusBarColor;

        return mActivityTabProvider.get() != null && mUseTabThemeColor
                ? UNDEFINED_STATUS_BAR_COLOR
                : mIntentDataProvider.getToolbarColor();
    }

    boolean isStatusBarDefaultThemeColor(boolean isFallbackColorDefault) {
        return mIntentDataProvider.isOpenedByChrome() && isFallbackColorDefault;
    }
}
