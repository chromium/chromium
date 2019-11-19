// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.ui.styles.ChromeColors;

import javax.inject.Inject;

/**
 * Maintains the toolbar color for {@link CustomTabActivity}.
 */
@ActivityScope
public class CustomTabToolbarColorController {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final ChromeActivity mActivity;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final CustomTabActivityTabProvider mTabProvider;

    /** Keeps track of the original color before the preview was shown. */
    private int mOriginalColor;

    /** True if a change to the toolbar color was made because of a preview. */
    private boolean mTriggeredPreviewChange;

    private final CustomTabActivityTabProvider.Observer mActivityTabObserver =
            new CustomTabActivityTabProvider.Observer() {
                @Override
                public void onTabSwapped(@NonNull Tab tab) {
                    updateColor(tab);
                }
            };

    private ToolbarManager mToolbarManager;

    @Inject
    public CustomTabToolbarColorController(BrowserServicesIntentDataProvider intentDataProvider,
            ChromeActivity activity, CustomTabActivityTabProvider tabProvider,
            TabObserverRegistrar tabObserverRegistrar) {
        mIntentDataProvider = intentDataProvider;
        mActivity = activity;
        mTabProvider = tabProvider;
        mTabObserverRegistrar = tabObserverRegistrar;
    }

    /**
     * Notifies the ColorController that the ToolbarManager has been created and is ready for
     * use. ToolbarManager isn't passed directly to the constructor because it's not guaranteed to
     * be initialized yet.
     */
    public void onToolbarInitialized(ToolbarManager manager) {
        mToolbarManager = manager;
        assert manager != null : "Toolbar manager not initialized";

        // Logic isn't shared with WebappActivity yet.
        if (mIntentDataProvider.getWebappExtras() != null) return;

        int toolbarColor = mIntentDataProvider.getToolbarColor();
        manager.onThemeColorChanged(toolbarColor, false);
        if (!mIntentDataProvider.isOpenedByChrome()) {
            manager.setShouldUpdateToolbarPrimaryColor(false);
        }
        observeTabToUpdateColor();
        mTabProvider.addObserver(mActivityTabObserver);
    }

    private void observeTabToUpdateColor() {
        mTabObserverRegistrar.registerActivityTabObserver(new EmptyTabObserver() {
            @Override
            public void onPageLoadFinished(Tab tab, String url) {
                // Update the color when the page load finishes.
                updateColor(tab);
            }

            @Override
            public void onUrlUpdated(Tab tab) {
                // Update the color on every new URL.
                updateColor(tab);
            }
        });
    }

    /**
     * Updates the color of the Activity's CCT Toolbar. When a preview is shown, it should
     * be reset to the default color. If the user later navigates away from that preview to
     * a non-preview page, reset the color back to the original. This does not interfere
     * with site-specific theme colors which are disabled when a preview is being shown.
     */
    private void updateColor(Tab tab) {
        ToolbarManager manager = mToolbarManager;

        // Record the original toolbar color in case we need to revert back to it later
        // after a preview has been shown then the user navigates to another non-preview
        // page.
        if (mOriginalColor == 0) mOriginalColor = manager.getPrimaryColor();

        final boolean shouldUpdateOriginal = manager.getShouldUpdateToolbarPrimaryColor();
        manager.setShouldUpdateToolbarPrimaryColor(true);

        if (tab.isPreview()) {
            final int defaultColor =
                    ChromeColors.getDefaultThemeColor(mActivity.getResources(), false);
            manager.onThemeColorChanged(defaultColor, false);
            mTriggeredPreviewChange = true;
        } else if (mOriginalColor != manager.getPrimaryColor() && mTriggeredPreviewChange) {
            manager.onThemeColorChanged(mOriginalColor, false);
            mTriggeredPreviewChange = false;
            mOriginalColor = 0;
        }

        manager.setShouldUpdateToolbarPrimaryColor(shouldUpdateOriginal);
    }
}
