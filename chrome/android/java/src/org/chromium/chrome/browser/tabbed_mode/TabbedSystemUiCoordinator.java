// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.os.Build;
import android.view.Window;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * A UI coordinator that manages the system status bar and bottom navigation bar for
 * ChromeTabbedActivity.
 *
 * TODO(https://crbug.com/943371): Create a base SystemUiCoordinator to own the
 *     StatusBarColorController, and have this class extend that one.
 */
public class TabbedSystemUiCoordinator {
    private @Nullable TabbedNavigationBarColorController mNavigationBarColorController;

    /**
     * Construct a new {@link TabbedSystemUiCoordinator}.
     *
     * @param window The {@link Window} associated with the containing activity.
     * @param tabModelSelector The {@link TabModelSelector} for the containing activity.
     * @param layoutManagerSupplier {@link LayoutManager} associated with the containing activity.
     * @param mFullscreenManager The {@link FullscreenManager} used for containing activity
     */
    public TabbedSystemUiCoordinator(Window window, TabModelSelector tabModelSelector,
            @Nullable ObservableSupplier<LayoutManager> layoutManagerSupplier,
            FullscreenManager mFullscreenManager) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            assert layoutManagerSupplier != null;
            mNavigationBarColorController = new TabbedNavigationBarColorController(
                    window, tabModelSelector, layoutManagerSupplier, mFullscreenManager);
        }
    }

    /**
     * Gets the {@link TabbedNavigationBarColorController}. Note that this returns null for version
     * lower than {@link Build.VERSION_CODES#O_MR1}.
     */
    @Nullable
    TabbedNavigationBarColorController getNavigationBarColorController() {
        return mNavigationBarColorController;
    }

    public void destroy() {
        if (mNavigationBarColorController != null) mNavigationBarColorController.destroy();
    }
}
