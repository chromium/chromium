// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.NATIVE_URLS_OF_INTEREST;
import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.graphics.PointF;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import androidx.annotation.IntDef;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryItemView;
import org.chromium.chrome.browser.history.HistoryPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.RenderTestUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.browser.vr.util.VrInfoBarUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.concurrent.TimeoutException;

/**
 * End-to-end tests for testing navigation transitions (e.g. link clicking) in VR Browser mode, aka
 * "VR Shell".
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserNavigationTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mTestRule = new ChromeTabbedActivityVrTestRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule("components/test/data/vr_browser_ui/render_tests");

    private WebXrVrTestFramework mWebXrVrTestFramework;
    private VrBrowserTestFramework mVrBrowserTestFramework;

    private static final String TEST_PAGE_2D_URL =
            VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page");
    private static final String TEST_PAGE_2D_2_URL =
            VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page2");
    private static final String TEST_PAGE_WEBXR_URL =
            WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_navigation_webxr_page");
    private static final String TEST_PAGE_WEBXR_2_URL =
            WebXrVrTestFramework.getFileUrlForHtmlTestFile("test_navigation_webxr_page2");

    @IntDef({Page.PAGE_2D, Page.PAGE_2D_2, Page.PAGE_WEBXR})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Page {
        int PAGE_2D = 0;
        int PAGE_2D_2 = 1;
        int PAGE_WEBXR = 2;
    }

    @IntDef({PresentationMode.NON_PRESENTING, PresentationMode.PRESENTING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface PresentationMode {
        int NON_PRESENTING = 0;
        int PRESENTING = 1;
    }

    @IntDef({FullscreenMode.NON_FULLSCREENED, FullscreenMode.FULLSCREENED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface FullscreenMode {
        int NON_FULLSCREENED = 0;
        int FULLSCREENED = 1;
    }

    @Before
    public void setUp() {
        mWebXrVrTestFramework = new WebXrVrTestFramework(mTestRule);
        mVrBrowserTestFramework = new VrBrowserTestFramework(mTestRule);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
    }

    private String getUrl(@Page int page) {
        switch (page) {
            case Page.PAGE_2D:
                return TEST_PAGE_2D_URL;
            case Page.PAGE_2D_2:
                return TEST_PAGE_2D_2_URL;
            case Page.PAGE_WEBXR:
                return TEST_PAGE_WEBXR_URL;
            default:
                throw new UnsupportedOperationException("Don't know page type " + page);
        }
    }

    /**
     * Triggers navigation to either a 2D or WebXR page. Similar to
     * {@link ChromeActivityTestRule#loadUrl loadUrl} but makes sure page initiates the
     * navigation. This is desirable since we are testing navigation transitions end-to-end.
     */
    private void navigateTo(final @Page int to) {
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), getUrl(to), () -> {
                    mVrBrowserTestFramework.runJavaScriptOrFail(
                            "window.location.href = '" + getUrl(to) + "';", POLL_TIMEOUT_SHORT_MS);
                }, POLL_TIMEOUT_LONG_MS);
    }

    private void enterFullscreenOrFail(WebContents webContents) throws TimeoutException {
        DOMUtils.clickNode(webContents, "fullscreen", false /* goThroughRootAndroidView */);
        VrBrowserTestFramework.waitOnJavaScriptStep(webContents);
        Assert.assertTrue("Failed to enter fullscreen", DOMUtils.isFullscreen(webContents));
    }

    private void assertState(WebContents wc, @Page int page, @PresentationMode int presentationMode,
            @FullscreenMode int fullscreenMode) throws TimeoutException {
        Assert.assertTrue("Browser is not in VR", VrShellDelegate.isInVr());
        Assert.assertEquals("Browser is not on correct web site", getUrl(page), wc.getVisibleUrl());
        Assert.assertEquals("Browser's presentation mode does not match expectation",
                presentationMode == PresentationMode.PRESENTING,
                TestVrShellDelegate.getVrShellForTesting().getWebVrModeEnabled());
        Assert.assertEquals("Browser's fullscreen mode does not match expectation'",
                fullscreenMode == FullscreenMode.FULLSCREENED, DOMUtils.isFullscreen(wc));
        // Feedback infobar should never show up during navigations.
        VrInfoBarUtils.expectInfoBarPresent(mTestRule, false);
    }

    /**
     * Tests navigation from a 2D to a 2D page. Also tests that this navigation is
     * properly added to Chrome's history and is usable.
     */
    @Test
    @MediumTest
    public void test2dTo2d() throws TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_2D_2);

        assertState(mVrBrowserTestFramework.getCurrentWebContents(), Page.PAGE_2D_2,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);

        // Test that the navigations were added to history
        mTestRule.loadUrl(UrlConstants.HISTORY_URL, PAGE_LOAD_TIMEOUT_S);
        HistoryPage historyPage =
                (HistoryPage) mTestRule.getActivity().getActivityTab().getNativePage();
        ArrayList<HistoryItemView> itemViews = historyPage.getHistoryManagerForTesting()
                                                       .getAdapterForTests()
                                                       .getItemViewsForTests();
        Assert.assertEquals("Incorrect number of items in history", 2, itemViews.size());
        // History is in reverse chronological order, so the first navigation should actually be
        // after the second in the list
        Assert.assertEquals("First navigation did not show correctly in history",
                getUrl(Page.PAGE_2D), itemViews.get(1).getItem().getUrl());
        Assert.assertEquals("Second navigation did not show correctly in history",
                getUrl(Page.PAGE_2D_2), itemViews.get(0).getItem().getUrl());

        // Test that clicking on history items in VR works
        TestThreadUtils.runOnUiThreadBlocking(() -> itemViews.get(0).onClick());
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), getUrl(Page.PAGE_2D_2));
        assertState(mWebXrVrTestFramework.getCurrentWebContents(), Page.PAGE_2D_2,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a 2D to a WebXR page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void test2dToWebXr() throws IllegalArgumentException, TimeoutException {
        impl2dToWeb(Page.PAGE_WEBXR, mWebXrVrTestFramework);
    }

    private void impl2dToWeb(@Page int page, WebXrVrTestFramework framework)
            throws TimeoutException {
        framework.loadUrlAndAwaitInitialization(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);

        navigateTo(page);

        assertState(framework.getCurrentWebContents(), page, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened 2D to a WebXR page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void test2dFullscreenToWebXr()
            throws IllegalArgumentException, TimeoutException {
        impl2dFullscreenToWeb(Page.PAGE_WEBXR, mWebXrVrTestFramework);
    }

    private void impl2dFullscreenToWeb(@Page int page, WebXrVrTestFramework framework)
            throws TimeoutException {
        framework.loadUrlAndAwaitInitialization(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
        enterFullscreenOrFail(framework.getCurrentWebContents());

        navigateTo(page);

        assertState(framework.getCurrentWebContents(), page, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a WebXR to a 2D page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testWebXrTo2d() throws IllegalArgumentException, TimeoutException {
        webTo2dImpl(Page.PAGE_WEBXR, mWebXrVrTestFramework);
    }

    private void webTo2dImpl(@Page int page, WebXrVrTestFramework framework)
            throws TimeoutException {
        framework.loadUrlAndAwaitInitialization(getUrl(page), PAGE_LOAD_TIMEOUT_S);

        navigateTo(Page.PAGE_2D);

        assertState(framework.getCurrentWebContents(), Page.PAGE_2D,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a WebXR to a WebXR page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testWebXrToWebXr() throws IllegalArgumentException, TimeoutException {
        webToWebImpl(Page.PAGE_WEBXR, mWebXrVrTestFramework);
    }

    private void webToWebImpl(@Page int page, WebXrVrTestFramework framework)
            throws TimeoutException {
        framework.loadUrlAndAwaitInitialization(getUrl(page), PAGE_LOAD_TIMEOUT_S);

        navigateTo(page);

        assertState(framework.getCurrentWebContents(), page, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a presenting WebXR to a 2D page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testWebXrPresentingTo2d()
            throws IllegalArgumentException, TimeoutException {
        webPresentingTo2dImpl(Page.PAGE_WEBXR, mWebXrVrTestFramework);
    }

    private void webPresentingTo2dImpl(@Page int page, WebXrVrTestFramework framework)
            throws TimeoutException {
        framework.loadUrlAndAwaitInitialization(getUrl(page), PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();

        navigateTo(Page.PAGE_2D);

        assertState(framework.getCurrentWebContents(), Page.PAGE_2D,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a presenting WebXR to a WebXR page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testWebXrPresentingToWebXr()
            throws IllegalArgumentException, TimeoutException {
        webPresentingToWebImpl(Page.PAGE_WEBXR, mWebXrVrTestFramework);
    }

    private void webPresentingToWebImpl(@Page int page, WebXrVrTestFramework framework)
            throws TimeoutException {
        framework.loadUrlAndAwaitInitialization(getUrl(page), PAGE_LOAD_TIMEOUT_S);
        framework.enterSessionWithUserGestureOrFail();

        navigateTo(page);

        assertState(framework.getCurrentWebContents(), page, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened WebXR to a 2D page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testWebXrFullscreenTo2d()
            throws IllegalArgumentException, TimeoutException {
        webFullscreenTo2dImpl(Page.PAGE_WEBXR, mWebXrVrTestFramework);
    }

    private void webFullscreenTo2dImpl(@Page int page, WebXrVrTestFramework framework)
            throws TimeoutException {
        framework.loadUrlAndAwaitInitialization(getUrl(page), PAGE_LOAD_TIMEOUT_S);
        enterFullscreenOrFail(framework.getCurrentWebContents());

        navigateTo(Page.PAGE_2D);

        assertState(framework.getCurrentWebContents(), Page.PAGE_2D,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests navigation from a fullscreened WebXR to a WebXR page.
     */
    @Test
    @MediumTest
            @CommandLineFlags.Add({"enable-features=WebXR"})
            public void testWebXrFullscreenToWebXr()
            throws IllegalArgumentException, TimeoutException {
        webFullscreenToWebImpl(Page.PAGE_WEBXR, mWebXrVrTestFramework);
    }

    private void webFullscreenToWebImpl(@Page int page, WebXrVrTestFramework framework)
            throws TimeoutException {
        framework.loadUrlAndAwaitInitialization(getUrl(page), PAGE_LOAD_TIMEOUT_S);
        enterFullscreenOrFail(framework.getCurrentWebContents());

        navigateTo(page);

        assertState(framework.getCurrentWebContents(), page, PresentationMode.NON_PRESENTING,
                FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests that the back button is disabled in VR if pressing it in regular 2D Chrome would result
     * in Chrome being backgrounded.
     */
    @Test
    @MediumTest
    public void testBackDoesntBackgroundChrome() throws IllegalArgumentException {
        Assert.assertFalse(
                "Back button is enabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
        mTestRule.loadUrlInNewTab(getUrl(Page.PAGE_2D), false, TabLaunchType.FROM_CHROME_UI);
        Assert.assertFalse(
                "Back button is enabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
        final Tab tab =
                mTestRule.loadUrlInNewTab(getUrl(Page.PAGE_2D), false, TabLaunchType.FROM_LINK);
        Assert.assertTrue(
                "Back button is disabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTestRule.getActivity().getTabModelSelector().closeTab(tab); });
        Assert.assertFalse(
                "Back button is enabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
    }

    /**
     * Tests that the navigation buttons work only when they should, and are greyed out when not
     * usable.
     */
    @Test
    @MediumTest
    @Feature({"Browser", "RenderTest"})
    public void testNavigationButtons()
            throws IllegalArgumentException, InterruptedException, IOException {
        // TODO(https://crbug.com/930840): Remove this when the weird gradient behavior is fixed.
        mRenderTestRule.setPixelDiffThreshold(2);
        Assert.assertFalse(
                "Back button is enabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
        Assert.assertFalse(
                "Forward button is enabled.", VrBrowserTransitionUtils.isForwardButtonEnabled());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "navigation_buttons_both_disabled", mRenderTestRule);

        // Opening a new tab shouldn't enable the back button
        mTestRule.loadUrlInNewTab("about:blank", false, TabLaunchType.FROM_CHROME_UI);
        final int tabId = mTestRule.getActivity().getActivityTab().getId();
        Assert.assertFalse(
                "Back button is enabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
        Assert.assertFalse(
                "Forward button is enabled.", VrBrowserTransitionUtils.isForwardButtonEnabled());
        // Actually click on the back button and ensure it doesn't go back to the first tab.
        NativeUiUtils.clickElement(UserFriendlyElementName.BACK_BUTTON, new PointF());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "navigation_buttons_both_disabled", mRenderTestRule);
        Assert.assertEquals("Back button navigated to previous tab", tabId,
                mTestRule.getActivity().getActivityTab().getId());

        // Navigating to a new page should enable the back button
        mTestRule.loadUrl("chrome://version/");
        Assert.assertTrue(
                "Back button is disabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
        Assert.assertFalse(
                "Forward button is enabled.", VrBrowserTransitionUtils.isForwardButtonEnabled());
        // Overflow menu should still be visible since navigation doesn't close it.
        NativeUiUtils.waitForUiQuiescence();
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "navigation_buttons_back_enabled", mRenderTestRule);

        // Navigating back should disable the back button and enable the forward button.
        // We need to click twice - once to close the overflow menu, and once to actually click.
        NativeUiUtils.clickElement(UserFriendlyElementName.BACK_BUTTON, new PointF());
        NativeUiUtils.clickElement(UserFriendlyElementName.BACK_BUTTON, new PointF());
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), "about:blank");
        Assert.assertFalse(
                "Back button is enabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
        Assert.assertTrue(
                "Forward button is disabled.", VrBrowserTransitionUtils.isForwardButtonEnabled());
        // Once again, click on the back button and ensure it doesn't go back to the first tab.
        NativeUiUtils.clickElement(UserFriendlyElementName.BACK_BUTTON, new PointF());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "navigation_buttons_forward_enabled", mRenderTestRule);
        Assert.assertEquals("Back button navigated to previous tab", tabId,
                mTestRule.getActivity().getActivityTab().getId());

        // Navigating forward should disable the forward button and enable the back button
        NativeUiUtils.clickElement(UserFriendlyElementName.FORWARD_BUTTON, new PointF());
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), "chrome://version/");
        Assert.assertTrue(
                "Back button is disabled.", VrBrowserTransitionUtils.isBackButtonEnabled());
        Assert.assertFalse(
                "Forward button is enabled.", VrBrowserTransitionUtils.isForwardButtonEnabled());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        RenderTestUtils.dumpAndCompare(NativeUiUtils.FRAME_BUFFER_SUFFIX_BROWSER_UI,
                "navigation_buttons_back_enabled", mRenderTestRule);
    }

    /**
     * Tests that the refresh button in the overflow menu properly refreshes the page.
     */
    @Test
    @MediumTest
    public void testRefreshButton() throws InterruptedException {
        // The navigationStart time should change anytime we refresh, so save the value and compare
        // later.
        // Use a double because apparently returning Unix timestamps as floating point is a logical
        // thing for JavaScript to do and Long.valueOf is afraid of decimal points.
        String navStart = "window.performance.timing.navigationStart";
        final double navigationTimestamp =
                Double.valueOf(mVrBrowserTestFramework.runJavaScriptOrFail(
                                       navStart, POLL_TIMEOUT_SHORT_MS))
                        .doubleValue();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        NativeUiUtils.clickElement(UserFriendlyElementName.RELOAD_BUTTON, new PointF());

        // Wait for the navigationStart time to be newer than our saved time.
        CriteriaHelper.pollInstrumentationThread(() -> {
            return Double.valueOf(mVrBrowserTestFramework.runJavaScriptOrFail(
                                          navStart, POLL_TIMEOUT_SHORT_MS))
                           .doubleValue()
                    > navigationTimestamp;
        }, "Refresh button did not refresh page");
    }

    /**
     * Tests that navigation to/from native pages works properly and that interacting with the
     * screen doesn't cause issues. See crbug.com/737167. Disabled on standalones because they
     * don't have touchscreens, so the mouseSingleClickView causes issues.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testNativeNavigationAndInteraction() throws IllegalArgumentException {
        for (String url : NATIVE_URLS_OF_INTEREST) {
            mTestRule.loadUrl(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
            mTestRule.loadUrl(url, PAGE_LOAD_TIMEOUT_S);
            ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                    mTestRule.getActivity().getWindow().getDecorView().getRootView());
        }
        mTestRule.loadUrl(TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
    }

    /**
     * Tests that renderer crashes while in (fullscreen) 2D browsing don't exit VR.
     */
    @Test
    @MediumTest
    public void testRendererKilledInFullscreenStaysInVr()
            throws IllegalArgumentException, TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                TEST_PAGE_2D_URL, PAGE_LOAD_TIMEOUT_S);
        enterFullscreenOrFail(mVrBrowserTestFramework.getCurrentWebContents());

        final Tab tab = mTestRule.getActivity().getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> ChromeTabUtils.simulateRendererKilledForTesting(tab, true));

        mVrBrowserTestFramework.simulateRendererKilled();

        TestThreadUtils.runOnUiThreadBlocking(() -> tab.reload());
        ChromeTabUtils.waitForTabPageLoaded(tab, TEST_PAGE_2D_URL);
        ChromeTabUtils.waitForInteractable(tab);

        assertState(mVrBrowserTestFramework.getCurrentWebContents(), Page.PAGE_2D,
                PresentationMode.NON_PRESENTING, FullscreenMode.NON_FULLSCREENED);
    }

    /**
     * Tests that Incognito and non-Incognito modes maintain separate history stacks. Automation of
     * a manual test in https://crbug.com/861925.
     */
    @Test
    @MediumTest
    public void testIncognitoMaintainsSeparateHistoryStack() throws InterruptedException {
        // Test non-Incognito's forward/back.
        mTestRule.loadUrl(TEST_PAGE_2D_URL);
        mTestRule.loadUrl(TEST_PAGE_2D_2_URL);
        VrBrowserTransitionUtils.navigateBack();
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_2D_URL);
        VrBrowserTransitionUtils.navigateForward();
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_2D_2_URL);

        // Open up an Incognito tab.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.NEW_INCOGNITO_TAB, new PointF());
        NewTabPageTestUtils.waitForNtpLoaded(mTestRule.getActivity().getActivityTab());

        // Test Incognito's forward/back.
        // TODO(https://crbug.com/868506): Remove the waitForTabPageLoaded calls after the loadUrl
        // calls once the issue with Incognito loadUrl reporting page load too quickly is fixed.
        mTestRule.loadUrl(TEST_PAGE_WEBXR_2_URL);
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_WEBXR_2_URL);
        mTestRule.loadUrl(TEST_PAGE_WEBXR_URL);
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_WEBXR_URL);
        VrBrowserTransitionUtils.navigateBack();
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_WEBXR_2_URL);
        VrBrowserTransitionUtils.navigateForward();
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_WEBXR_URL);

        // Exit Incognito.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CLOSE_INCOGNITO_TABS, new PointF());
        CriteriaHelper.pollUiThread(() -> {
            return mTestRule.getWebContents().getVisibleUrl().equals(TEST_PAGE_2D_2_URL);
        }, "Did not successfully exit Incognito mode");

        // Ensure that non-Incognito's forward/back was unaffected by Incognito.
        VrBrowserTransitionUtils.navigateBack();
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_2D_URL);
        VrBrowserTransitionUtils.navigateForward();
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), TEST_PAGE_2D_2_URL);
    }

    /**
     * Tests that closing all Incognito tabs with no non-Incognito tabs open automatically opens
     * a new non-Incognito tab. Automation of a manual test in https://crbug.com/861925.
     */
    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM)
    public void testNewTabAutomaticallyOpenedWhenIncognitoClosed() throws InterruptedException {
        VrBrowserTransitionUtils.forceExitVr();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Close the tab that's automatically open at test start.
            TabModelSelector.from(mTestRule.getActivity().getActivityTab()).closeAllTabs();
            // Create an Incognito tab. Closing all tabs automatically goes to overview mode, but
            // appears to take some amount of time to do so. Instead of waiting until then and
            // creating through the menu item, just create an Incognito tab directly.
            mTestRule.getActivity().getTabCreator(true /* incognito */).launchNTP();
        });
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);

        // Close the Incognito tab and ensure a non-Incognito NTP is opened.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OVERFLOW_MENU, new PointF());
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.CLOSE_INCOGNITO_TABS, new PointF());
        CriteriaHelper.pollUiThread(() -> {
            return mTestRule.getActivity().getActivityTab().getNativePage() != null;
        }, "Closing Incognito tab did not load a native page");
        Assert.assertFalse("Created native page is in Incognito mode",
                mTestRule.getActivity().getActivityTab().isIncognito());
        Assert.assertTrue("Created native page is not a NTP",
                mTestRule.getActivity().getActivityTab().getNativePage() instanceof NewTabPage);
    }

    /**
     * Tests that inputting a URL into the URL bar results in a successful navigation.
     */
    @Test
    @MediumTest
    public void testUrlEntryTriggersNavigation() throws InterruptedException {
        testUrlEntryTriggersNavigationImpl();
    }

    /**
     * Tests that inputting a URL into the URL bar in Incognito Mode results in a successful
     * navigation.
     */
    @Test
    @MediumTest
    public void testUrlEntryTriggersNavigationIncognito() throws InterruptedException {
        mVrBrowserTestFramework.openIncognitoTab("about:blank");
        testUrlEntryTriggersNavigationImpl();
    }

    private void testUrlEntryTriggersNavigationImpl() throws InterruptedException {
        NativeUiUtils.enableMockedKeyboard();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // This is a roundabout solution for ensuring that the committing/pressing of enter actually
        // results in a navigation - internally, this is handled by grabbing the first suggestion
        // and navigating to its destination. So, we need to make sure we have suggestions before
        // pressing enter, otherwise nothing happens. If this ever stops working, e.g. due to always
        // having suggestions present, waiting for a number of frames before committing seemed
        // stable. Adding an operation to listen for suggestion changes would also work, but
        // would add some pretty niche test-only code to the native side.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://version/"); });
        NativeUiUtils.inputEnter();
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), "chrome://version/");
    }

    /**
     * Tests that clicking on a suggestion results in a successful navigation.
     */
    @Test
    @MediumTest
    public void testSuggestionClickTriggersNavigation() throws InterruptedException {
        testSuggestionClickTriggersNavigationImpl();
    }

    /**
     * Tests that clicking on a suggestion while in Incognito Mode results in a successful
     * navigation.
     */
    @Test
    @MediumTest
    public void testSuggestionClickTriggersNavigationIncognito() throws InterruptedException {
        mVrBrowserTestFramework.openIncognitoTab("about:blank");
        testSuggestionClickTriggersNavigationImpl();
    }

    private void testSuggestionClickTriggersNavigationImpl() throws InterruptedException {
        NativeUiUtils.enableMockedKeyboard();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.SUGGESTION_BOX, true /* visible */,
                () -> { NativeUiUtils.inputString("chrome://"); });
        // Click near the bottom of the suggestion box to get the last suggestion, which for
        // "chrome://" should be a valid chrome:// URL. The suggestion that triggers a search can
        // be in either the middle or top spot depending on whether the
        // OmniboxGroupSuggestionsBySearchVsUrl feature is enabled or not.
        NativeUiUtils.clickElement(UserFriendlyElementName.SUGGESTION_BOX, new PointF(0.0f, -0.4f));
        ChromeTabUtils.waitForTabPageLoaded(
                mTestRule.getActivity().getActivityTab(), (String) null);
        // We can't just wait for navigation to finish and then check because waitForTabPageLoaded
        // only supports either exact URL matching or no URL matching, and no URL matching results
        // in the URL still being about:blank when we check.
        CriteriaHelper.pollInstrumentationThread(() -> {
            return mTestRule.getActivity().getActivityTab().getUrl().startsWith("chrome://");
        });
    }
}
