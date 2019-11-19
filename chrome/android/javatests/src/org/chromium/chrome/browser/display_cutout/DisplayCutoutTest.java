// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.os.Build;
import android.support.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/**
 * Tests the display cutout.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class DisplayCutoutTest {
    @Rule
    public DisplayCutoutTestRule mTestRule =
            new DisplayCutoutTestRule<ChromeActivity>(ChromeActivity.class);

    /**
     * Test that no safe area is applied when we have viewport fit auto
     */
    @Test
    @LargeTest
    public void testViewportFitAuto() throws TimeoutException {
        // Display Cutout API requires Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that no safe area is applied when we have viewport fit contain.
     */
    @Test
    @LargeTest
    public void testViewportFitContain() throws TimeoutException {
        // Display Cutout API requires Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_CONTAIN);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
    }

    /**
     * Test that the safe area is applied when we have viewport fit cover.
     */
    @Test
    @LargeTest
    public void testViewportFitCover() throws TimeoutException {
        // Display Cutout API requires Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mTestRule.exitFullscreen();

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that the safe area is applied when we have viewport fit cover forced by the user agent.
     */
    @Test
    @LargeTest
    public void testViewportFitCoverForced() throws TimeoutException {
        // Display Cutout API requires Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        mTestRule.enterFullscreen();

        // Set the viewport fit internally as this value is internal only.
        mTestRule.setViewportFitInternal(ViewportFit.COVER_FORCED_BY_USER_AGENT);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mTestRule.exitFullscreen();

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that no safe area is applied when we have no viewport fit.
     */
    @Test
    @LargeTest
    public void testViewportFitDefault() throws TimeoutException {
        // Display Cutout API requires Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mTestRule.setViewportFit("");
        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that the safe area is calculated correctly using the device's dip scale.
     */
    @Test
    @LargeTest
    public void testViewportFitDipScale() throws TimeoutException {
        // Display Cutout API requires Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        mTestRule.enterFullscreen();
        mTestRule.setDipScale(DisplayCutoutTestRule.TEST_HIGH_DIP_SCALE);
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT_HIGH_DIP);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that the safe area is calculated correctly when using a subframe.
     */
    @Test
    @LargeTest
    public void testViewportFitSubframe() throws TimeoutException {
        // Display Cutout API requires Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_CONTAIN);

        mTestRule.enterFullscreenOnSubframe();
        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        mTestRule.waitForSafeAreaOnSubframe(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
    }

    /**
     * Test that we do not break if we have cover but no cutout.
     */
    @Test
    @LargeTest
    public void testViewportFitCoverNoCutout() throws TimeoutException {
        // Display Cutout API requires Android P+.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;

        mTestRule.setDeviceHasCutout(false);
        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                DisplayCutoutTestRule.LayoutParamsApi28.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }
}
