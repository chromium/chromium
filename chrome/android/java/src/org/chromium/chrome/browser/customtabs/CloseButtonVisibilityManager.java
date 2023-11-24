// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.graphics.drawable.Drawable;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.toolbar.ToolbarManager;

import javax.inject.Inject;

/** Manages the visibility of the close button. */
@ActivityScope
public class CloseButtonVisibilityManager {
    private final Drawable mCloseButtonDrawable;

    private @Nullable ToolbarManager mToolbarManager;
    private boolean mIsVisible = true;

    @Inject
    public CloseButtonVisibilityManager(BrowserServicesIntentDataProvider intentDataProvider) {
        mCloseButtonDrawable = intentDataProvider.getCloseButtonDrawable();
    }

    public void setVisibility(boolean isVisible) {
        if (mIsVisible == isVisible) return;

        mIsVisible = isVisible;
        updateCloseButtonVisibility();
    }

    public void onToolbarInitialized(ToolbarManager toolbarManager) {
        mToolbarManager = toolbarManager;
        updateCloseButtonVisibility();
    }

    private void updateCloseButtonVisibility() {
        if (mToolbarManager == null) return;

        mToolbarManager.setCloseButtonDrawable(mIsVisible ? mCloseButtonDrawable : null);
    }
}
