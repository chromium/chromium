// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.os.Build;
import android.view.WindowManager;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/** Tests the display cutout on a WebApp. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
public class WebappDisplayCutoutTest {
    @Rule public WebappDisplayCutoutTestRule mTestRule = new WebappDisplayCutoutTestRule();

    /** Test that a safe area is not applied when we have viewport-fit=cover and a normal webapp. */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = DisplayMode.UNDEFINED)
    public void testViewportFitWebapp() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /** Test that a safe area is applied when we have viewport-fit=cover and a fullscreen webapp. */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = DisplayMode.FULLSCREEN)
    @DisabledTest(message = "Flaky test - see: https://crbug.com/1211064")
    public void testViewportFitWebapp_Fullscreen() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that a safe area is not applied when we have viewport-fit=cover and a minimal UI display
     * mode.
     */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = DisplayMode.MINIMAL_UI)
    public void testViewportFitWebapp_MinimalUi() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that a safe area is not applied when we have viewport-fit=cover and a standalone display
     * mode.
     */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = DisplayMode.STANDALONE)
    public void testViewportFitWebapp_Standalone() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }
}
