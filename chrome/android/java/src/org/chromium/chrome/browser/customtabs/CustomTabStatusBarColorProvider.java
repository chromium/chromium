// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.ui.system.StatusBarColorController.DEFAULT_STATUS_BAR_COLOR;
import static org.chromium.chrome.browser.ui.system.StatusBarColorController.UNDEFINED_STATUS_BAR_COLOR;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarColorController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarColorController.ToolbarColorType;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;

import javax.inject.Inject;

/** Manages the status bar color for a CustomTabActivity. */
@ActivityScope
public class CustomTabStatusBarColorProvider {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final StatusBarColorController mStatusBarColorController;

    private boolean mUseTabThemeColor;

    @Inject
    public CustomTabStatusBarColorProvider(
            BrowserServicesIntentDataProvider intentDataProvider,
            StatusBarColorController statusBarColorController) {
        mIntentDataProvider = intentDataProvider;
        mStatusBarColorController = statusBarColorController;
    }

    /**
     * Sets whether the tab's theme color should be used for the status bar and triggers an update
     * of the status bar color if needed.
     */
    public void setUseTabThemeColor(boolean useTabThemeColor) {
        if (mUseTabThemeColor == useTabThemeColor) return;

        mUseTabThemeColor = useTabThemeColor;
        mStatusBarColorController.updateStatusBarColor();
    }

    int getBaseStatusBarColor(Tab tab) {
        @ToolbarColorType
        int toolbarColorType =
                CustomTabToolbarColorController.computeToolbarColorType(
                        mIntentDataProvider, mUseTabThemeColor, tab);
        return switch (toolbarColorType) {
            case ToolbarColorType.THEME_COLOR -> UNDEFINED_STATUS_BAR_COLOR;
            case ToolbarColorType.DEFAULT_COLOR -> DEFAULT_STATUS_BAR_COLOR;
            case ToolbarColorType.INTENT_TOOLBAR_COLOR -> mIntentDataProvider
                    .getColorProvider()
                    .getToolbarColor();
            default -> DEFAULT_STATUS_BAR_COLOR;
        };
    }
}
