// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.os.Build.VERSION_CODES;
import android.view.WindowManager;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests the display cutout. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// The combination of these two features causes the first page load to layout correctly in this
// test suite. TODO(crbug.com/377778493): To fix this test to work properly w/ EdgeToEdge.
@Features.DisableFeatures({
    ChromeFeatureList.DRAW_CUTOUT_EDGE_TO_EDGE,
})
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/376095153
public class DisplayCutoutTest {
    private final AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();
    private final DisplayCutoutTestRule mDisplayCutoutTestRule = new DisplayCutoutTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mDisplayCutoutTestRule);

    private WebPageStation mPage;

    @Before
    public void setUp() {
        WebPageStation page = mActivityTestRule.startOnBlankPage();
        mDisplayCutoutTestRule.attach(page.getActivity(), page.getTab());
        mPage =
                page.loadWebPageProgrammatically(
                        mActivityTestRule.getTestServer().getURL(DisplayCutoutTestRule.TEST_PAGE));
    }

    /** Test that no safe area is applied when we have viewport fit auto */
    @Test
    @LargeTest
    public void testViewportFitAuto() throws TimeoutException {
        mDisplayCutoutTestRule.enterFullscreen();
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /** Test that no safe area is applied when we have viewport fit contain. */
    @Test
    @LargeTest
    public void testViewportFitContain() throws TimeoutException {
        mDisplayCutoutTestRule.enterFullscreen();
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_CONTAIN);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
    }

    /** Test that the safe area is applied when we have viewport fit cover. */
    @Test
    @LargeTest
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "https://crbug.com/402051996")
    public void testViewportFitCover() throws TimeoutException {
        mDisplayCutoutTestRule.enterFullscreen();
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mDisplayCutoutTestRule.exitFullscreen();

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that the safe area is applied when we have viewport fit cover forced by the user agent.
     */
    @Test
    @LargeTest
    @DisabledTest(message = "issuetracker.google.com/353900381")
    public void testViewportFitCoverForced() throws TimeoutException {
        mDisplayCutoutTestRule.enterFullscreen();

        // Set the viewport fit internally as this value is internal only.
        mDisplayCutoutTestRule.setViewportFitInternal(ViewportFit.COVER_FORCED_BY_USER_AGENT);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mDisplayCutoutTestRule.exitFullscreen();

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test without Fullscreen to make sure that viewport fit cover does not draw under the cutout.
     */
    @Test
    @LargeTest
    public void testViewportFitCover_NotFullscreen() throws TimeoutException {
        // Start without entering fullscreen.
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);
        try {
            mDisplayCutoutTestRule.waitForSafeArea(
                    DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
            mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
        } catch (AssertionError e) {
            throw new AssertionError(
                    "When not in Fullscreen the Safe Area should not include the cutout!", e);
        }
    }

    /** Test that no safe area is applied when we have no viewport fit. */
    @Test
    @LargeTest
    @DisableIf.Build(
            supported_abis_includes = "armeabi-v7a",
            message = "https://crbug.com/402051996")
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "https://crbug.com/402051996")
    public void testViewportFitDefault() throws TimeoutException {
        mDisplayCutoutTestRule.enterFullscreen();
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        mDisplayCutoutTestRule.setViewportFit("");
        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /** Test that the safe area is calculated correctly using the device's dip scale. */
    @Test
    @LargeTest
    @DisableIf.Build(sdk_equals = 34, message = "crbug.com/403286999")
    public void testViewportFitDipScale() throws TimeoutException {
        mDisplayCutoutTestRule.enterFullscreen();
        mDisplayCutoutTestRule.setDipScale(DisplayCutoutTestRule.TEST_HIGH_DIP_SCALE);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(
                DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT_HIGH_DIP);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /** Test that the safe area is calculated correctly when using a subframe. */
    @Test
    @LargeTest
    public void testViewportFitSubframe() throws TimeoutException {
        mDisplayCutoutTestRule.enterFullscreen();
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_CONTAIN);

        mDisplayCutoutTestRule.enterFullscreenOnSubframe();
        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        mDisplayCutoutTestRule.waitForSafeAreaOnSubframe(
                DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
    }

    /** Test that we do not break if we have cover but no cutout. */
    @Test
    @LargeTest
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "https://crbug.com/402051996")
    public void testViewportFitCoverNoCutout() throws TimeoutException {
        mDisplayCutoutTestRule.setDeviceHasCutout(false);
        mDisplayCutoutTestRule.enterFullscreen();
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
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
        final SettableMonotonicObservableSupplier<Integer> browserCutoutModeSupplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return ObservableSuppliers.createMonotonic();
                        });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    browserCutoutModeSupplier.set(
                            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
                    Tab tab = mPage.getTab();
                    mDisplayCutoutTestRule.setDisplayCutoutController(
                            DisplayCutoutTestRule.TestDisplayCutoutController.create(
                                    tab, browserCutoutModeSupplier));
                });

        mDisplayCutoutTestRule.enterFullscreen();
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    browserCutoutModeSupplier.set(
                            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
                });
        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER);
    }
}
