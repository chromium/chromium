// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;

import androidx.annotation.Px;

import org.chromium.chrome.browser.fullscreen.FullscreenManager;

/**
 * CustomTabHeightStrategy for Partial Custom Tab Side-Sheet implementation. An instance of this
 * class should be owned by the CustomTabActivity.
 */
public class PartialCustomTabSideSheetStrategy extends PartialCustomTabBaseStrategy {
    public PartialCustomTabSideSheetStrategy(Activity activity, @Px int initialHeight,
            boolean isFixedHeight, CustomTabHeightStrategy.OnResizedCallback onResizedCallback,
            FullscreenManager fullscreenManager, boolean isTablet, boolean interactWithBackground) {
        super(activity, initialHeight, isFixedHeight, onResizedCallback, fullscreenManager,
                isTablet, interactWithBackground);
    }

    @Override
    @PartialCustomTabType
    public int getStrategyType() {
        return PartialCustomTabType.SIDE_SHEET;
    }

    @Override
    public void onShowSoftInput(Runnable softKeyboardRunnable) {
        softKeyboardRunnable.run();
    }

    @Override
    protected void updatePosition() {}
}