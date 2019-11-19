// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.support.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.webapps.WebDisplayMode;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/**
 * Tests the display cutout on a WebApp.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappDisplayCutoutTest {
    @Rule
    public WebappDisplayCutoutTestRule mTestRule = new WebappDisplayCutoutTestRule();

    /**
     * Test that a safe area is not applied when we have viewport-fit=cover and a normal webapp.
     */
    @Test
    @LargeTest
    @FlakyTest(message = "crbug.com/862728")
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = WebDisplayMode.UNDEFINED)
    public void testViewportFitWebapp() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that a safe area is applied when we have viewport-fit=cover and a fullscreen webapp.
     */
    @Test
    @LargeTest
    @FlakyTest(message = "crbug.com/862728")
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = WebDisplayMode.FULLSCREEN)
    public void testViewportFitWebapp_Fullscreen() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that a safe area is not applied when we have viewport-fit=cover and a minimal UI
     * display mode.
     */
    @Test
    @LargeTest
    @FlakyTest(message = "crbug.com/862728")
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = WebDisplayMode.MINIMAL_UI)
    public void testViewportFitWebapp_MinimalUi() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that a safe area is not applied when we have viewport-fit=cover and a standalone
     * display mode.
     */
    @Test
    @LargeTest
    @FlakyTest(message = "crbug.com/862728")
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = WebDisplayMode.STANDALONE)
    public void testViewportFitWebapp_Standalone() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }
}
