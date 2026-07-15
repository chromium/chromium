// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.view.WindowManager;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests the display cutout on a WebApp. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
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
    @DisabledTest(message = "Flaky test - see: https://crbug.com/40767445")
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
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288753
    public void testViewportFitWebapp_MinimalUi() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that a safe area is applied when we have viewport-fit=cover and a standalone display
     * mode with the short-edges cutout feature enabled.
     */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = DisplayMode.STANDALONE)
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        mTestRule.waitForSafeAreaTopInset();
    }

    /**
     * Test that a standalone webapp with the short-edges cutout feature enabled does not draw under
     * the cutout when the page never opts in via viewport-fit=cover.
     */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = DisplayMode.STANDALONE)
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone_DefaultViewportFit() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that a standalone webapp with the short-edges cutout feature enabled updates the window
     * layout when the page dynamically changes viewport-fit via JavaScript after load.
     */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = DisplayMode.STANDALONE)
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone_DynamicViewportFit() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);

        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        mTestRule.waitForSafeAreaTopInset();

        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);

        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that a fullscreen webapp with the short-edges cutout feature enabled enters immersive
     * mode with short-edges immediately on creation, without waiting for the page to declare
     * viewport-fit=cover.
     */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(
            displayMode = DisplayMode.FULLSCREEN,
            useRealBrowserCutoutSupplier = true)
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Fullscreen_DefaultViewportFit() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);

        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that the pre-flag behavior is preserved for a standalone display mode when the
     * short-edges cutout feature is disabled: no safe area and DEFAULT cutout mode.
     */
    @Test
    @LargeTest
    @WebappDisplayCutoutTestRule.TestConfiguration(displayMode = DisplayMode.STANDALONE)
    @DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone_FeatureDisabled() throws TimeoutException {
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }
}
