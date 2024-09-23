// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.graphics.Bitmap;
import android.os.Build.VERSION_CODES;

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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.TestAnimations;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManagerTestUtils;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
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
    "enable-features=BackForwardTransitions<Study",
    "force-fieldtrials=Study/Group",
    "force-fieldtrial-params=Study.Group:transition_from_native_pages/true/"
            + "transition_to_native_pages/true"
})
@DoNotBatch(reason = "Affect nav settings")
@EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
@DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/337886037")
@DisableIf.Build(supported_abis_includes = "x86_64", message = "https://crbug.com/337886037")
public class ScreenshotCaptureTest {
    @Rule
    public final SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_NAVIGATION_GESTURENAV)
                    .build();

    private static final String TEST_PAGE = "/chrome/test/data/android/simple.html";
    private static final String TEST_PAGE_2 = "/chrome/test/data/android/google.html";
    private static final String LONG_HTML_TEST_PAGE =
            UrlUtils.encodeHtmlDataUri(
                    "<html>"
                            + "<head>"
                            + "  <meta name=\"viewport\" content=\"width=device-width\">"
                            + "</head>"
                            + "<body style='height:100000px;'>"
                            + "</body>"
                            + "</html>");

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
        TestAnimations.setEnabled(true);
        // Fix the port cause the screenshot includes the url bar
        mTestServer =
                EmbeddedTestServer.createAndStartServerWithPort(
                        ApplicationProvider.getApplicationContext(), 46985);

        var mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer);
        var mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;
        mScreenshotCaptureTestHelper = new ScreenshotCaptureTestHelper();
        mHomepageTestRule.useChromeNtpForTest();
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
    public void testNavigatingAwayFromNativeBookmarkToNormalPage(boolean nightModeEnabled)
            throws IOException, TimeoutException, InterruptedException {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.BOOKMARKS_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

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
        mRenderTestRule.compareForResult(
                mCapturedBitmap, "navigate_away_from_native_bookmark_to_normal_page");
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
    @Feature({"RenderTest"})
    // The test is based on 3-button mode. The newer version defaults to gesture mode.
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.Q)
    @DisabledTest(message = "https://crbug.com/357833738")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testNavigatingBackToNtpFromNormalPage(boolean nightModeEnabled)
            throws IOException, TimeoutException, InterruptedException {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());

        mActivityTestRule.loadUrl(mTestServer.getURL(TEST_PAGE));

        GestureNavigationTestUtils mNavUtils = new GestureNavigationTestUtils(mActivityTestRule);
        mNavUtils.swipeFromEdgeAndHold(/* leftEdge= */ true);

        CallbackHelper callbackHelper = new CallbackHelper();
        mActivityTestRule
                .getWebContents()
                .captureContentAsBitmapForTesting(
                        bitmap -> {
                            try {
                                mRenderTestRule.compareForResult(
                                        bitmap, "navigate_back_to_ntp_from_normal_page");
                            } catch (IOException e) {
                                throw new RuntimeException(e);
                            }
                            callbackHelper.notifyCalled();
                        });
        callbackHelper.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(() -> mNavUtils.getNavigationHandler().release(true));
        // Wait animation to be finished. Reduce flakiness caused by being destroyed during a
        // running animation.
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    // The test is based on 3-button mode. The newer version defaults to gesture mode.
    @DisableIf.Build(sdk_is_greater_than = VERSION_CODES.Q)
    @DisabledTest(message = "https://crbug.com/357833738")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testNavigatingBackToNtpFromNormalPageWithoutTopControls(boolean nightModeEnabled)
            throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                TabStateBrowserControlsVisibilityDelegate::disablePageLoadDelayForTests);
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());
        FullscreenManagerTestUtils.disableBrowserOverrides();
        mActivityTestRule.loadUrl(LONG_HTML_TEST_PAGE);
        BrowserControlsManager browserControlManager =
                mActivityTestRule.getActivity().getBrowserControlsManager();
        int browserControlsHeight = browserControlManager.getTopControlsHeight();
        FullscreenManagerTestUtils.waitForBrowserControlsToBeMoveable(
                mActivityTestRule, mActivityTestRule.getActivity().getActivityTab());
        FullscreenManagerTestUtils.scrollBrowserControls(mActivityTestRule, false);

        FullscreenManagerTestUtils.waitForBrowserControlsPosition(
                mActivityTestRule, -browserControlsHeight);
        GestureNavigationTestUtils mNavUtils = new GestureNavigationTestUtils(mActivityTestRule);
        mNavUtils.swipeFromEdgeAndHold(/* leftEdge= */ true);

        CallbackHelper callbackHelper = new CallbackHelper();
        mActivityTestRule
                .getWebContents()
                .captureContentAsBitmapForTesting(
                        bitmap -> {
                            try {
                                mRenderTestRule.compareForResult(
                                        bitmap,
                                        "navigate_back_to_ntp_from_normal_page_without_top_controls");
                            } catch (IOException e) {
                                throw new RuntimeException(e);
                            }
                            callbackHelper.notifyCalled();
                        });
        callbackHelper.waitForOnly();
        ThreadUtils.runOnUiThreadBlocking(() -> mNavUtils.getNavigationHandler().release(true));
        // Wait animation to be finished. Reduce flakiness caused by being destroyed during a
        // running animation.
        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());
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

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/357833738")
    public void testNavigateToNTPByHomeButton()
            throws InterruptedException, IOException, TimeoutException {
        mActivityTestRule.startMainActivityWithURL(mTestServer.getURL(TEST_PAGE));

        UiUtils.settleDownUI(InstrumentationRegistry.getInstrumentation());

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

        onView(withId(R.id.home_button)).perform(click());
        NewTabPageTestUtils.waitForNtpLoaded(mActivityTestRule.getActivity().getActivityTab());

        // Expect to capture a screenshot of TEST_PAGE
        callbackHelper.waitForOnly();
        mRenderTestRule.compareForResult(mCapturedBitmap, "navigate_to_ntp_by_home_button");
    }
}
