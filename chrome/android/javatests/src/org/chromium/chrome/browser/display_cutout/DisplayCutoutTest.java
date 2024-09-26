// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.WindowManager;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.concurrent.TimeoutException;

/** Tests the display cutout. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
public class DisplayCutoutTest {
    @Rule
    public DisplayCutoutTestRule mTestRule =
            new DisplayCutoutTestRule<ChromeActivity>(ChromeActivity.class);

    /** Test that no safe area is applied when we have viewport fit auto */
    @Test
    @LargeTest
    public void testViewportFitAuto() throws TimeoutException {
        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /** Test that no safe area is applied when we have viewport fit contain. */
    @Test
    @LargeTest
    public void testViewportFitContain() throws TimeoutException {
        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_CONTAIN);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
    }

    /** Test that the safe area is applied when we have viewport fit cover. */
    @Test
    @LargeTest
    public void testViewportFitCover() throws TimeoutException {
        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mTestRule.exitFullscreen();

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that the safe area is applied when we have viewport fit cover forced by the user agent.
     */
    @Test
    @LargeTest
    @DisabledTest(message = "issuetracker.google.com/353900381")
    public void testViewportFitCoverForced() throws TimeoutException {
        mTestRule.enterFullscreen();

        // Set the viewport fit internally as this value is internal only.
        mTestRule.setViewportFitInternal(ViewportFit.COVER_FORCED_BY_USER_AGENT);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mTestRule.exitFullscreen();

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test without Fullscreen to make sure that viewport fit cover does not draw under the cutout.
     */
    @Test
    @LargeTest
    public void testViewportFitCover_NotFullscreen() throws TimeoutException {
        // Start without entering fullscreen.
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);
        try {
            mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
            mTestRule.waitForLayoutInDisplayCutoutMode(
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
        } catch (AssertionError e) {
            throw new AssertionError(
                    "When not in Fullscreen the Safe Area should not include the cutout!", e);
        }
    }

    /** Test that no safe area is applied when we have no viewport fit. */
    @Test
    @LargeTest
    public void testViewportFitDefault() throws TimeoutException {
        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mTestRule.setViewportFit("");
        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /** Test that the safe area is calculated correctly using the device's dip scale. */
    @Test
    @LargeTest
    public void testViewportFitDipScale() throws TimeoutException {
        mTestRule.enterFullscreen();
        mTestRule.setDipScale(DisplayCutoutTestRule.TEST_HIGH_DIP_SCALE);
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT_HIGH_DIP);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /** Test that the safe area is calculated correctly when using a subframe. */
    @Test
    @LargeTest
    public void testViewportFitSubframe() throws TimeoutException {
        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_CONTAIN);

        mTestRule.enterFullscreenOnSubframe();
        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        mTestRule.waitForSafeAreaOnSubframe(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
    }

    /** Test that we do not break if we have cover but no cutout. */
    @Test
    @LargeTest
    public void testViewportFitCoverNoCutout() throws TimeoutException {
        mTestRule.setDeviceHasCutout(false);
        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that the display cutout mode requested by the activity (ex. by the Trusted Web Activity)
     * takes precedence over the display cutout mode requested by the web page.
     */
    @Test
    @LargeTest
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.TIRAMISU, message = "crbug.com/365516493")
    public void testBrowserDisplayCutoutTakesPrecedence() throws Exception {
        final ObservableSupplierImpl<Integer> browserCutoutModeSupplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new ObservableSupplierImpl<Integer>();
                        });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    browserCutoutModeSupplier.set(
                            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
                    Tab tab = mTestRule.getActivity().getActivityTab();
                    mTestRule.setDisplayCutoutController(
                            DisplayCutoutTestRule.TestDisplayCutoutController.create(
                                    tab, browserCutoutModeSupplier));
                });

        mTestRule.enterFullscreen();
        mTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    browserCutoutModeSupplier.set(
                            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
                });
        mTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
    }
}
