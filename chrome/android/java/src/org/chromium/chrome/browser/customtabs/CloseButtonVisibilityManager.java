// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.graphics.drawable.Drawable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.toolbar.ToolbarManager;

/** Manages the visibility of the close button. */
@NullMarked
public class CloseButtonVisibilityManager {
    private final @Nullable Drawable mCloseButtonDrawable;

    private @Nullable ToolbarManager mToolbarManager;
    private @Nullable CustomTabToolbarButtonsCoordinator mToolbarButtonsCoordinator;
    private boolean mIsVisible;

    public CloseButtonVisibilityManager(BrowserServicesIntentDataProvider intentDataProvider) {
        mCloseButtonDrawable = intentDataProvider.getCloseButtonDrawable();
        mIsVisible = intentDataProvider.isCloseButtonEnabled();
    }

    public void setVisibility(boolean isVisible) {
        if (mIsVisible == isVisible) return;

        mIsVisible = isVisible;
        updateCloseButtonVisibility();
    }

    public void onToolbarInitialized(
            ToolbarManager toolbarManager,
            CustomTabToolbarButtonsCoordinator toolbarButtonsCoordinator) {
        mToolbarManager = toolbarManager;
        mToolbarButtonsCoordinator = toolbarButtonsCoordinator;
        updateCloseButtonVisibility();
    }

    private void updateCloseButtonVisibility() {
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) {
            if (mToolbarButtonsCoordinator == null) return;

            mToolbarButtonsCoordinator.setCloseButtonVisible(mIsVisible);

            return;
        }

        if (mToolbarManager == null) return;

        mToolbarManager.setCloseButtonDrawable(mIsVisible ? mCloseButtonDrawable : null);
    }
}
