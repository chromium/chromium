// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabbed_mode;

import android.os.Build;
import android.view.Window;

import androidx.annotation.Nullable;

import org.chromium.base.ObservableSupplier;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.ImmersiveModeManager;

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
     * @param immersiveModeManager The {@link ImmersiveModeManager} for the containing activity.
     * @param overviewModeBehaviorSupplier An {@link ObservableSupplier} for the
     *         {@link OverviewModeBehavior} associated with the containing activity.
     */
    public TabbedSystemUiCoordinator(Window window, TabModelSelector tabModelSelector,
            @Nullable ImmersiveModeManager immersiveModeManager,
            @Nullable ObservableSupplier<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            assert overviewModeBehaviorSupplier != null;
            mNavigationBarColorController = new TabbedNavigationBarColorController(
                    window, tabModelSelector, immersiveModeManager, overviewModeBehaviorSupplier);
        }
    }

    public void destroy() {
        if (mNavigationBarColorController != null) mNavigationBarColorController.destroy();
    }
}
