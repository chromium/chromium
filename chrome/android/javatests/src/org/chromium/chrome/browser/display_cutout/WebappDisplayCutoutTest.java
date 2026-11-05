// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import android.content.Intent;
import android.os.Build;
import android.view.WindowManager;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.display_cutout.DisplayCutoutTestRule.TestDisplayCutoutController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;

/** Tests the display cutout on a WebApp. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappDisplayCutoutTest {
    /** Opts standalone webapps into the short-edges cutout mode, which is off by default. */
    private static final String SHORT_EDGES_CUTOUT_MODE_STANDALONE =
            ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE + ":enable_standalone/true";

    private final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();
    private final DisplayCutoutTestRule mDisplayCutoutTestRule = new DisplayCutoutTestRule();

    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mActivityTestRule).around(mDisplayCutoutTestRule);

    /** Test that a safe area is not applied when we have viewport-fit=cover and a normal webapp. */
    @Test
    @LargeTest
    public void testViewportFitWebapp() throws TimeoutException {
        startWebapp(DisplayMode.UNDEFINED);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /** Test that a safe area is applied when we have viewport-fit=cover and a fullscreen webapp. */
    @Test
    @LargeTest
    @DisabledTest(message = "Flaky test - see: https://crbug.com/40767445")
    public void testViewportFitWebapp_Fullscreen() throws TimeoutException {
        startWebapp(DisplayMode.FULLSCREEN);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITH_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that a safe area is not applied when we have viewport-fit=cover and a minimal UI display
     * mode.
     */
    @Test
    @LargeTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511288753
    public void testViewportFitWebapp_MinimalUi() throws TimeoutException {
        startWebapp(DisplayMode.MINIMAL_UI);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that a safe area is applied when we have viewport-fit=cover and a standalone display
     * mode with the short-edges cutout feature and its standalone parameter enabled.
     */
    @Test
    @LargeTest
    @EnableFeatures(SHORT_EDGES_CUTOUT_MODE_STANDALONE)
    // The coordinated cutout path requires edge-to-edge everywhere, unsupported pre-R.
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone() throws TimeoutException {
        startWebapp(DisplayMode.STANDALONE);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        mDisplayCutoutTestRule.waitForSafeAreaTopInset();
    }

    /**
     * Test that a standalone webapp opted into the short-edges cutout mode does not draw under the
     * cutout when the page never opts in via viewport-fit=cover.
     */
    @Test
    @LargeTest
    @EnableFeatures(SHORT_EDGES_CUTOUT_MODE_STANDALONE)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone_DefaultViewportFit() throws TimeoutException {
        startWebapp(DisplayMode.STANDALONE);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that a standalone webapp opted into the short-edges cutout mode updates the window
     * layout when the page dynamically changes viewport-fit via JavaScript after load.
     */
    @Test
    @LargeTest
    @EnableFeatures(SHORT_EDGES_CUTOUT_MODE_STANDALONE)
    // The coordinated cutout path requires edge-to-edge everywhere, unsupported pre-R.
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone_DynamicViewportFit() throws TimeoutException {
        startWebapp(DisplayMode.STANDALONE);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);

        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
        mDisplayCutoutTestRule.waitForSafeAreaTopInset();

        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);

        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that a fullscreen webapp with the short-edges cutout feature enabled enters immersive
     * mode with short-edges immediately on creation, without waiting for the page to declare
     * viewport-fit=cover.
     */
    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // The coordinated cutout path requires edge-to-edge everywhere, unsupported pre-R.
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Fullscreen_DefaultViewportFit() throws TimeoutException {
        Tab tab = startWebapp(DisplayMode.FULLSCREEN);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mDisplayCutoutTestRule.setDisplayCutoutController(
                                TestDisplayCutoutController.createWithRealBrowserCutoutSupplier(
                                        tab)));
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_AUTO);

        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES);
    }

    /**
     * Test that the pre-flag behavior is preserved for a standalone display mode when the
     * short-edges cutout feature is disabled: no safe area and DEFAULT cutout mode.
     */
    @Test
    @LargeTest
    @DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone_FeatureDisabled() throws TimeoutException {
        startWebapp(DisplayMode.STANDALONE);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    /**
     * Test that a standalone webapp keeps the pre-flag behavior when the short-edges cutout feature
     * is enabled without its standalone parameter, which is the default experiment configuration.
     */
    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // Tablets and desktop run the webapp in a windowed container where SHORT_EDGES never applies.
    @DisableIf.Device(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testViewportFitWebapp_Standalone_StandaloneParamDisabled() throws TimeoutException {
        startWebapp(DisplayMode.STANDALONE);
        mDisplayCutoutTestRule.setViewportFit(DisplayCutoutTestRule.VIEWPORT_FIT_COVER);

        mDisplayCutoutTestRule.waitForSafeArea(DisplayCutoutTestRule.TEST_SAFE_AREA_WITHOUT_CUTOUT);
        mDisplayCutoutTestRule.waitForLayoutInDisplayCutoutMode(
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT);
    }

    private Tab startWebapp(@DisplayMode.EnumType int displayMode) {
        Intent intent =
                mActivityTestRule
                        .createIntent()
                        .putExtra(
                                WebappConstants.EXTRA_URL,
                                mActivityTestRule
                                        .getTestServer()
                                        .getURL(DisplayCutoutTestRule.TEST_PAGE))
                        .putExtra(WebappConstants.EXTRA_DISPLAY_MODE, displayMode);
        mActivityTestRule.startWebappActivity(intent);

        WebappActivity activity = mActivityTestRule.getActivity();
        Tab tab = ThreadUtils.runOnUiThreadBlocking(activity::getActivityTab);
        mDisplayCutoutTestRule.attach(activity, tab);
        return tab;
    }
}
