// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.Bitmap;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.UiUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/** Test that the screenshot was successfully taken when navigating as expected. */
@RunWith(ParameterizedRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "hide-scrollbars",
    "enable-features=" + BlinkFeatures.BACK_FORWARD_TRANSITIONS
})
@DoNotBatch(reason = "Affect nav settings")
@EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
public class ScreenshotCaptureTest {
    @Rule
    public final SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_NAVIGATION_GESTURENAV)
                    .build();

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/google.html";

    private EmbeddedTestServer mTestServer;
    private Bitmap mCapturedBitmap;
    private ScreenshotCaptureTestHelper mScreenshotCaptureTestHelper;

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());

        var mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer);
        var mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;
        mScreenshotCaptureTestHelper = new ScreenshotCaptureTestHelper();
    }

    @After
    public void tearDown() {
        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(null);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testNavigatingAwayFromNtpToNormalPage(boolean nightModeEnabled)
            throws IOException, TimeoutException, InterruptedException {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());

        CallbackHelper callbackHelper = new CallbackHelper();
        int currentNavIndex =
                mActivityTestRule
                        .getActivity()
                        .getCurrentWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getCurrentEntryIndex();

        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(
                new ScreenshotCaptureTestHelper.NavScreenshotCallback() {
                    @Override
                    public Bitmap onAvailable(int navIndex, Bitmap bitmap, boolean requested) {
                        Assert.assertEquals(
                                "Should capture the screenshot of the previous page.",
                                currentNavIndex,
                                navIndex);
                        Assert.assertTrue(requested);
                        mCapturedBitmap = bitmap;
                        callbackHelper.notifyCalled();
                        return null;
                    }
                });

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));

        callbackHelper.waitForOnly();
        mRenderTestRule.compareForResult(mCapturedBitmap, "navigate_away_from_ntp_to_normal_page");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testNavigatingAwayFromNtpToWebUiPage(boolean nightModeEnabled)
            throws IOException, TimeoutException, InterruptedException {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());

        CallbackHelper callbackHelper = new CallbackHelper();
        int currentNavIndex =
                mActivityTestRule
                        .getActivity()
                        .getCurrentWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getCurrentEntryIndex();

        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(
                new ScreenshotCaptureTestHelper.NavScreenshotCallback() {
                    @Override
                    public Bitmap onAvailable(int navIndex, Bitmap bitmap, boolean requested) {
                        Assert.assertEquals(
                                "Should capture the screenshot of the previous page.",
                                currentNavIndex,
                                navIndex);
                        Assert.assertTrue(requested);
                        mCapturedBitmap = bitmap;
                        callbackHelper.notifyCalled();
                        return null;
                    }
                });

        mActivityTestRule.loadUrl(UrlConstants.GPU_URL);

        callbackHelper.waitForOnly();
        mRenderTestRule.compareForResult(mCapturedBitmap, "navigate_away_from_ntp_to_webui_page");
    }

    @Test
    @MediumTest
    public void testNotCaptureSadTab() throws TimeoutException, InterruptedException {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));
        WebContentsUtils.crashTabAndWait(mActivityTestRule.getWebContents());
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

        // Sad tab is not considered as a native page.
        Assert.assertFalse(mActivityTestRule.getActivity().getActivityTab().isNativePage());
        CallbackHelper callbackHelper = new CallbackHelper();
        int currentNavIndex =
                mActivityTestRule
                        .getActivity()
                        .getCurrentWebContents()
                        .getNavigationController()
                        .getNavigationHistory()
                        .getCurrentEntryIndex();

        mScreenshotCaptureTestHelper.setNavScreenshotCallbackForTesting(
                new ScreenshotCaptureTestHelper.NavScreenshotCallback() {
                    @Override
                    public Bitmap onAvailable(int navIndex, Bitmap bitmap, boolean requested) {
                        Assert.assertEquals(
                                "Should attempt to capture the screenshot of the previous page.",
                                currentNavIndex,
                                navIndex);
                        Assert.assertFalse("No screenshot should be captured", requested);
                        Assert.assertNull("No screenshot should be captured", bitmap);
                        callbackHelper.notifyCalled();
                        return null;
                    }
                });

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE_2));
        callbackHelper.waitForOnly();
    }
}
