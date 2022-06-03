// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.view.View;

import org.junit.Assert;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Utility methods and classes for testing modal dialogs.
 */
public class ChromeModalDialogTestUtils {
    /**
     * Checks whether the browser controls and tab obscured state is appropriately set.
     * @param activity The activity to use to query for appropriate state
     * @param restricted If true, the menu should be enabled and the tabs should be obscred.
     */
    public static void checkBrowserControls(ChromeActivity activity, boolean restricted) {
        boolean isViewObscuringAllTabs = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> activity.getTabObscuringHandler().areAllTabsObscured());
        boolean isMenuEnabled = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            View menu = activity.getToolbarManager().getMenuButtonView();
            Assert.assertNotNull("Toolbar menu is incorrectly null.", menu);
            return menu.isEnabled();
        });

        if (restricted) {
            Assert.assertTrue("All tabs should be obscured", isViewObscuringAllTabs);
            Assert.assertFalse("Menu is incorrectly enabled.", isMenuEnabled);
        } else {
            Assert.assertFalse("Tabs shouldn't be obscured", isViewObscuringAllTabs);
            Assert.assertTrue("Menu is incorrectly disabled.", isMenuEnabled);
        }
    }
}
