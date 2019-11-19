// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.status_indicator;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Integration tests for status indicator covering related code in
 * {@link StatusIndicatorCoordinator} and {@link TabbedRootUiCoordinator}.
 */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.OFFLINE_INDICATOR_V2})
public class StatusIndicatorTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private StatusIndicatorCoordinator mStatusIndicatorCoordinator;
    private StatusIndicatorSceneLayer mStatusIndicatorSceneLayer;
    private View mStatusIndicatorContainer;
    private ViewGroup.MarginLayoutParams mControlContainerLayoutParams;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();

        mStatusIndicatorCoordinator = ((TabbedRootUiCoordinator) mActivityTestRule.getActivity()
                                               .getRootUiCoordinatorForTesting())
                                              .getStatusIndicatorCoordinatorForTesting();
        mStatusIndicatorSceneLayer = mStatusIndicatorCoordinator.getSceneLayer();
        mStatusIndicatorContainer =
                mActivityTestRule.getActivity().findViewById(R.id.status_indicator);
        final View controlContainer =
                mActivityTestRule.getActivity().findViewById(R.id.control_container);
        mControlContainerLayoutParams =
                (ViewGroup.MarginLayoutParams) controlContainer.getLayoutParams();
    }

    @Test
    @MediumTest
    public void testShowAndHide() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        assertThat("Wrong initial Android view visibility.",
                mStatusIndicatorContainer.getVisibility(), equalTo(View.GONE));
        Assert.assertFalse("Wrong initial composited view visibility.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
        assertThat("Wrong initial control container top margin.",
                mControlContainerLayoutParams.topMargin, equalTo(0));

        TestThreadUtils.runOnUiThreadBlocking(mStatusIndicatorCoordinator::show);

        assertThat("Android view is not visible.", mStatusIndicatorContainer.getVisibility(),
                equalTo(View.VISIBLE));
        Assert.assertTrue("Composited view is not visible.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
        assertThat("Wrong control container top margin.", mControlContainerLayoutParams.topMargin,
                equalTo(mStatusIndicatorContainer.getHeight()));

        TestThreadUtils.runOnUiThreadBlocking(mStatusIndicatorCoordinator::hide);

        assertThat("Android view is not gone.", mStatusIndicatorContainer.getVisibility(),
                equalTo(View.GONE));
        Assert.assertFalse("Composited view is visible.",
                mStatusIndicatorSceneLayer.isSceneOverlayTreeShowing());
        assertThat("Wrong control container top margin.", mControlContainerLayoutParams.topMargin,
                equalTo(0));
    }
}
