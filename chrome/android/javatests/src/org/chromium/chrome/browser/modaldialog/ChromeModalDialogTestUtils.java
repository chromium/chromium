// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.view.View;

import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.app.ChromeActivity;

/** Utility methods and classes for testing modal dialogs. */
public class ChromeModalDialogTestUtils {
    /**
     * Checks whether the browser controls and tab obscured state is appropriately set.
     *
     * @param activity The activity to use to query for appropriate state
     * @param restricted If true, the menu should be enabled and the tabs should be obscured.
     */
    public static void checkBrowserControls(ChromeActivity activity, boolean restricted) {
        boolean isViewObscuringTabContent =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> activity.getTabObscuringHandler().isTabContentObscured());
        boolean isViewObscuringToolbar =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> activity.getTabObscuringHandler().isToolbarObscured());
        boolean isMenuEnabled =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            View menu = activity.getToolbarManager().getMenuButtonView();
                            Assert.assertNotNull("Toolbar menu is incorrectly null.", menu);
                            return menu.isEnabled();
                        });

        if (restricted) {
            Assert.assertTrue("All tabs should be obscured", isViewObscuringTabContent);
            Assert.assertFalse("Menu is incorrectly enabled.", isMenuEnabled);
        } else {
            Assert.assertFalse("Tabs shouldn't be obscured", isViewObscuringTabContent);
            Assert.assertTrue("Menu is incorrectly disabled.", isMenuEnabled);
        }
        Assert.assertFalse(
                "Tab modal dialogs should never obscure the toolbar.", isViewObscuringToolbar);
    }
}
