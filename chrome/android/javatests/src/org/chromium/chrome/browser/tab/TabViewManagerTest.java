// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/** Tests for the {@link TabViewManagerImpl} class. */
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TabViewManagerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Verify that controller margins are correctly applied to the {@link View} that is provided by
     * a {@link TabViewProvider}.
     */
    @Test
    @SmallTest
    public void testControllerMargins() {
        ChromeActivity activity = mActivityTestRule.getActivity();
        BrowserControlsManager browserControls = activity.getBrowserControlsManager();
        View view = new View(activity);
        TabViewProvider tvp =
                new TabViewProvider() {
                    @Override
                    public int getTabViewProviderType() {
                        return 0;
                    }

                    @Override
                    public View getView() {
                        return view;
                    }

                    @Override
                    public int getBackgroundColor(Context context) {
                        return 0;
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    activity.getActivityTab().getTabViewManager().addTabViewProvider(tvp);
                });

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) view.getLayoutParams();
        int expectedTopMargin =
                browserControls.getTopControlsHeight() + browserControls.getTopControlOffset();
        int expectedBottomMargin =
                browserControls.getBottomControlsHeight()
                        - browserControls.getBottomControlOffset();

        Assert.assertEquals(
                "Top margin for view was not set correctly in TabViewManagerImpl",
                expectedTopMargin,
                layoutParams.topMargin);
        Assert.assertEquals(
                "Bottom margin for view was not set correctly in TabViewManagerImpl",
                expectedBottomMargin,
                layoutParams.bottomMargin);
        Assert.assertEquals(
                "Left margin for view was not set correctly in TabViewManagerImpl",
                0,
                layoutParams.leftMargin);
        Assert.assertEquals(
                "Right margin for view was not set correctly in TabViewManagerImpl",
                0,
                layoutParams.rightMargin);
    }
}
