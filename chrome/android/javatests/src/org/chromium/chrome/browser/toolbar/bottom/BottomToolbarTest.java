// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.ExecutionException;

/**
 * Integration tests for the bottom toolbar.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
public class BottomToolbarTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        FeatureUtilities.setIsBottomToolbarEnabledForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        FeatureUtilities.setIsBottomToolbarEnabledForTesting(null);
    }

    @Test
    @MediumTest
    public void testBottomToolbarVisibility() {
        Assert.assertNotNull("BottomToolbarCoordinator should be constructed.",
                mActivityTestRule.getActivity().getToolbarManager().getBottomToolbarCoordinator());

        View bottomToolbar = mActivityTestRule.getActivity().findViewById(R.id.bottom_toolbar);
        Assert.assertEquals("Bottom toolbar view should be visible.", View.VISIBLE,
                bottomToolbar.getVisibility());
    }

    @Test
    @MediumTest
    public void testBottomToolbarTabSwitcherButton() throws ExecutionException {
        Assert.assertFalse("Tab switcher should not be visible.",
                mActivityTestRule.getActivity().getOverviewModeBehavior().overviewVisible());

        ViewGroup bottomToolbar = mActivityTestRule.getActivity().findViewById(R.id.bottom_toolbar);
        View tabSwitcherButton = bottomToolbar.findViewById(R.id.tab_switcher_button_wrapper);

        OverviewModeBehaviorWatcher overviewModeWatcher = new OverviewModeBehaviorWatcher(
                mActivityTestRule.getActivity().getOverviewModeBehavior(), true, false);
        TestThreadUtils.runOnUiThreadBlocking(() -> tabSwitcherButton.callOnClick());
        overviewModeWatcher.waitForBehavior();

        Assert.assertTrue("Tab switcher should be visible.",
                mActivityTestRule.getActivity().getOverviewModeBehavior().overviewVisible());
    }
}
