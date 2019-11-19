// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_CHECK_INTERVAL_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;
import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE;

import android.graphics.PointF;
import android.support.test.filters.LargeTest;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.history.HistoryPage;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.browser.vr.util.NativeUiUtils;
import org.chromium.chrome.browser.vr.util.VrBrowserTransitionUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * End-to-end tests for Daydream controller input while in the VR browser.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=LogJsConsoleMessages"})
@Restriction(RESTRICTION_TYPE_VIEWER_DAYDREAM_OR_STANDALONE)
public class VrBrowserControllerInputTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    private VrBrowserTestFramework mVrBrowserTestFramework;

    @Before
    public void setUp() {
        // Ensure that all frame updates are delivered to the browser so we can monitor for
        // scroll changes.
        WebContentsUtils.reportAllFrameSubmissions(mVrTestRule.getWebContents(), true);

        mVrBrowserTestFramework = new VrBrowserTestFramework(mVrTestRule);
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
    }

    private void waitForPageToBeScrollable(final RenderCoordinates coord) {
        final View view = mVrTestRule.getActivity().getActivityTab().getContentView();
        CriteriaHelper.pollUiThread(() -> {
            return coord.getContentHeightPixInt() > view.getHeight()
                    && coord.getContentWidthPixInt() > view.getWidth();
        }, "Page did not become scrollable", POLL_TIMEOUT_LONG_MS, POLL_CHECK_INTERVAL_LONG_MS);
    }

    /**
     * Verifies that swiping up/down/left/right on the Daydream controller's
     * touchpad scrolls the webpage while in the VR browser.
     */
    @Test
    @MediumTest
    public void testControllerScrolling() throws InterruptedException, Exception {
        String url = VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_controller_scrolling");

        final AtomicReference<RenderCoordinates> coord = new AtomicReference<RenderCoordinates>();
        Runnable waitScrollable = () -> {
            coord.set(RenderCoordinates.fromWebContents(mVrTestRule.getWebContents()));
            waitForPageToBeScrollable(coord.get());
        };

        Callable<Integer> getYCoord = () -> {
            return coord.get().getScrollYPixInt();
        };
        Callable<Integer> getXCoord = () -> {
            return coord.get().getScrollXPixInt();
        };

        testControllerScrollingImpl(url, waitScrollable, getYCoord, getXCoord);
    }

    /**
     * Verifies that scrolling via the Daydream controller's touchpad works in cross-origin iframes
     * (file:// URLs appear to be always treated as different origins).
     * Automation of a manual test in https://crbug.com/862153.
     */
    @Test
    @MediumTest
    public void testControllerScrollingIframe() throws InterruptedException, Exception {
        String url = VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                "test_controller_scrolling_iframe_outer");

        Runnable waitScrollable = () -> {
            // We need to focus the iframe before we can start running JavaScript in it.
            mVrBrowserTestFramework.runJavaScriptOrFail(
                    "document.getElementById('fs_iframe').focus()", POLL_TIMEOUT_SHORT_MS);
            mVrBrowserTestFramework.pollJavaScriptBooleanInFrameOrFail(
                    "document.documentElement.scrollHeight > document.documentElement.clientHeight",
                    POLL_TIMEOUT_LONG_MS);
        };

        Callable<Integer> getYCoord = () -> {
            // Round necessary to prevent Integer from failing due to decimal points.
            return Integer.valueOf(mVrBrowserTestFramework.runJavaScriptInFrameOrFail(
                    "Math.round(document.documentElement.scrollTop)", POLL_TIMEOUT_SHORT_MS));
        };
        Callable<Integer> getXCoord = () -> {
            // Round necessary to prevent Integer from failing due to decimal points.
            return Integer.valueOf(mVrBrowserTestFramework.runJavaScriptInFrameOrFail(
                    "Math.round(document.documentElement.scrollLeft)", POLL_TIMEOUT_SHORT_MS));
        };

        testControllerScrollingImpl(url, waitScrollable, getYCoord, getXCoord);
    }

    private void waitForScrollQuiescence(final Callable<Integer> getCoord) {
        final AtomicInteger lastCoord = new AtomicInteger(-1);
        // Half-second poll period to be sure that the scroll has actually finished instead of
        // being stuck in flaky scroll jank or taking longer than usual to start.
        CriteriaHelper.pollInstrumentationThread(() -> {
            Integer curCoord = getCoord.call();
            if (curCoord.equals(lastCoord.get())) return true;
            lastCoord.set(curCoord);
            return false;
        }, "Did not reach scroll quiescence", POLL_TIMEOUT_LONG_MS, 500);
    }

    private void testControllerScrollingImpl(String url, Runnable waitScrollable,
            Callable<Integer> getYCoord, Callable<Integer> getXCoord)
            throws InterruptedException, Exception {
        mVrTestRule.loadUrl(url, PAGE_LOAD_TIMEOUT_S);
        waitScrollable.run();

        // Test that scrolling down works.
        int startScrollPoint = getYCoord.call().intValue();
        NativeUiUtils.scrollNonFling(NativeUiUtils.ScrollDirection.DOWN);
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_NON_FLING_SCROLL);
        waitForScrollQuiescence(getYCoord);
        int endScrollPoint = getYCoord.call().intValue();
        Assert.assertTrue("Controller failed to scroll down", startScrollPoint < endScrollPoint);

        // Test that scrolling up works.
        startScrollPoint = getYCoord.call().intValue();
        NativeUiUtils.scrollNonFling(NativeUiUtils.ScrollDirection.UP);
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_NON_FLING_SCROLL);
        waitForScrollQuiescence(getYCoord);
        endScrollPoint = getYCoord.call().intValue();
        Assert.assertTrue("Controller failed to scroll up", startScrollPoint > endScrollPoint);

        // Test that scrolling right works.
        startScrollPoint = getXCoord.call().intValue();
        NativeUiUtils.scrollNonFling(NativeUiUtils.ScrollDirection.RIGHT);
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_NON_FLING_SCROLL);
        waitForScrollQuiescence(getXCoord);
        endScrollPoint = getXCoord.call().intValue();
        Assert.assertTrue("Controller failed to scroll right", startScrollPoint < endScrollPoint);

        // Test that scrolling left works.
        startScrollPoint = endScrollPoint;
        NativeUiUtils.scrollNonFling(NativeUiUtils.ScrollDirection.LEFT);
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_NON_FLING_SCROLL);
        waitForScrollQuiescence(getXCoord);
        endScrollPoint = getXCoord.call().intValue();
        Assert.assertTrue("Controller failed to scroll left", startScrollPoint > endScrollPoint);
    }

    /**
     * Verifies that fling scrolling works on the Daydream controller's touchpad. This is done by
     * performing a fast non-fling scroll, checking how far it scrolled, and asserting that a fling
     * scroll of the same speed scrolls further.
     */
    @Test
    @LargeTest
    public void testControllerFlingScrolling() throws InterruptedException {
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_controller_scrolling"),
                PAGE_LOAD_TIMEOUT_S);
        final RenderCoordinates coord =
                RenderCoordinates.fromWebContents(mVrTestRule.getWebContents());
        waitForPageToBeScrollable(coord);

        Callable<Integer> getYCoord = () -> {
            return coord.getScrollYPixInt();
        };
        Callable<Integer> getXCoord = () -> {
            return coord.getScrollXPixInt();
        };

        // Scrolling can be inconsistent. So, try each direction up to 3 times to try to work around
        // flakiness.
        int numAttempts = 3;
        final int diffMultiplier = 2;

        int startPoint;
        int nonFlingEndpoint;
        int nonFlingDistance;
        boolean succeeded = false;

        // Test fling scrolling down.
        for (int i = 0; i < numAttempts; ++i) {
            startPoint = coord.getScrollYPixInt();
            // Perform a fast non-fling scroll and record how far it causes the page to scroll.
            NativeUiUtils.scrollNonFlingFast(NativeUiUtils.ScrollDirection.DOWN);
            waitForScrollQuiescence(getYCoord);
            nonFlingEndpoint = coord.getScrollYPixInt();
            nonFlingDistance = nonFlingEndpoint - startPoint;
            // Perform a fling scroll and check that it goes sufficiently further than the non-fling
            // scroll.
            NativeUiUtils.scrollFling(NativeUiUtils.ScrollDirection.DOWN);
            waitForScrollQuiescence(getYCoord);
            if (coord.getScrollYPixInt() - nonFlingEndpoint >= diffMultiplier * nonFlingDistance) {
                succeeded = true;
                break;
            }
            // Reset to the top of the page to try again.
            mVrBrowserTestFramework.runJavaScriptOrFail(
                    "window.scrollTo(0, 0)", POLL_TIMEOUT_SHORT_MS);
            waitForScrollQuiescence(getYCoord);
        }
        Assert.assertTrue(
                "Fling scroll down was unable to go sufficiently further than non-fling scroll",
                succeeded);
        succeeded = false;

        // Test fling scrolling up.
        for (int i = 0; i < numAttempts; ++i) {
            // Ensure we're at the bottom of the page.
            mVrBrowserTestFramework.runJavaScriptOrFail(
                    "window.scrollTo(0, document.documentElement.scrollHeight)",
                    POLL_TIMEOUT_SHORT_MS);
            waitForScrollQuiescence(getYCoord);
            startPoint = coord.getScrollYPixInt();
            // Perform the actual test.
            NativeUiUtils.scrollNonFlingFast(NativeUiUtils.ScrollDirection.UP);
            waitForScrollQuiescence(getYCoord);
            nonFlingEndpoint = coord.getScrollYPixInt();
            nonFlingDistance = startPoint - nonFlingEndpoint;
            NativeUiUtils.scrollFling(NativeUiUtils.ScrollDirection.UP);
            waitForScrollQuiescence(getYCoord);
            if (nonFlingEndpoint - coord.getScrollYPixInt() >= diffMultiplier * nonFlingDistance) {
                succeeded = true;
                break;
            }
        }
        Assert.assertTrue(
                "Fling scroll up was unable to go sufficiently further than non-fling scroll",
                succeeded);
        succeeded = false;

        // Test fling scrolling right.
        for (int i = 0; i < numAttempts; ++i) {
            startPoint = coord.getScrollXPixInt();
            NativeUiUtils.scrollNonFlingFast(NativeUiUtils.ScrollDirection.RIGHT);
            waitForScrollQuiescence(getXCoord);
            nonFlingEndpoint = coord.getScrollXPixInt();
            nonFlingDistance = nonFlingEndpoint - startPoint;
            NativeUiUtils.scrollFling(NativeUiUtils.ScrollDirection.RIGHT);
            waitForScrollQuiescence(getXCoord);
            if (coord.getScrollXPixInt() - nonFlingEndpoint >= diffMultiplier * nonFlingDistance) {
                succeeded = true;
                break;
            }
            // Reset to the left side to try again
            mVrBrowserTestFramework.runJavaScriptOrFail(
                    "window.scrollTo(0, 0)", POLL_TIMEOUT_SHORT_MS);
            waitForScrollQuiescence(getXCoord);
        }
        Assert.assertTrue(
                "Fling scroll right was unable to go sufficiently further than non-fling scroll",
                succeeded);
        succeeded = false;

        // Test fling scrolling left.
        for (int i = 0; i < numAttempts; ++i) {
            // Ensure we're on the right side of the page.
            mVrBrowserTestFramework.runJavaScriptOrFail(
                    "window.scrollTo(document.documentElement.scrollWidth, 0)",
                    POLL_TIMEOUT_SHORT_MS);
            waitForScrollQuiescence(getXCoord);
            startPoint = coord.getScrollXPixInt();
            // Perform the actual test.
            NativeUiUtils.scrollNonFlingFast(NativeUiUtils.ScrollDirection.LEFT);
            waitForScrollQuiescence(getXCoord);
            nonFlingEndpoint = coord.getScrollXPixInt();
            nonFlingDistance = startPoint - nonFlingEndpoint;
            NativeUiUtils.scrollFling(NativeUiUtils.ScrollDirection.LEFT);
            waitForScrollQuiescence(getXCoord);
            if (nonFlingEndpoint - coord.getScrollXPixInt() >= diffMultiplier * nonFlingDistance) {
                succeeded = true;
                break;
            }
        }
        Assert.assertTrue(
                "Fling scroll left was unable to go sufficiently further than non-fling scroll",
                succeeded);
    }

    /**
     * Verifies that controller clicks in the VR browser are properly registered on the webpage.
     * This is done by clicking on a link on the page and ensuring that it causes a navigation.
     */
    @Test
    @MediumTest
    public void testControllerClicksRegisterOnWebpage() {
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                                    "test_controller_clicks_register_on_webpage"),
                PAGE_LOAD_TIMEOUT_S);

        NativeUiUtils.clickElement(UserFriendlyElementName.CONTENT_QUAD, new PointF());
        ChromeTabUtils.waitForTabPageLoaded(mVrTestRule.getActivity().getActivityTab(),
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"));
    }

    /**
     * Verifies that controller clicks in the VR browser on cross-origin iframes are properly
     * registered. This is done by clicking on a link in the iframe and ensuring that it causes a
     * navigation.
     * Automation of a manual test in https://crbug.com/862153.
     */
    @Test
    @MediumTest
    public void testControllerClicksRegisterOnIframe() {
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_iframe_clicks_outer"));
        NativeUiUtils.clickElement(UserFriendlyElementName.CONTENT_QUAD, new PointF());
        // Wait until the iframe's current location matches the URL of the page that gets navigated
        // to on click.
        mVrBrowserTestFramework.pollJavaScriptBooleanInFrameOrFail("window.location.href == '"
                        + VrBrowserTestFramework.getFileUrlForHtmlTestFile(
                                  "test_iframe_clicks_inner_nav")
                        + "'",
                POLL_TIMEOUT_SHORT_MS);
    }

    /*
     * Verifies that swiping up/down on the Daydream controller's touchpad
     * scrolls a native page while in the VR browser.
     */
    @DisabledTest(message = "crbug.com/1005835")
    @Test
    @MediumTest
    public void testControllerScrollingNative() throws InterruptedException {
        VrBrowserTransitionUtils.forceEnterVrBrowserOrFail(POLL_TIMEOUT_LONG_MS);
        // Fill history with enough items to scroll
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_controller_scrolling"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("generic_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_webxr_page"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_webxr_input"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_webxr_consent"),
                PAGE_LOAD_TIMEOUT_S);
        mVrTestRule.loadUrl(VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_gamepad_button"),
                PAGE_LOAD_TIMEOUT_S);

        mVrTestRule.loadUrl(UrlConstants.HISTORY_URL, PAGE_LOAD_TIMEOUT_S);

        RecyclerView recyclerView =
                ((HistoryPage) (mVrTestRule.getActivity().getActivityTab().getNativePage()))
                        .getHistoryManagerForTesting()
                        .getRecyclerViewForTests();

        // Test that scrolling down works
        int startScrollPoint = recyclerView.computeVerticalScrollOffset();
        NativeUiUtils.scrollNonFling(NativeUiUtils.ScrollDirection.DOWN);
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_NON_FLING_SCROLL);
        int endScrollPoint = recyclerView.computeVerticalScrollOffset();
        Assert.assertTrue("Controller failed to scroll down", startScrollPoint < endScrollPoint);

        // Test that scrolling up works
        startScrollPoint = endScrollPoint;
        NativeUiUtils.scrollNonFling(NativeUiUtils.ScrollDirection.UP);
        NativeUiUtils.waitNumFrames(NativeUiUtils.NUM_FRAMES_NON_FLING_SCROLL);
        endScrollPoint = recyclerView.computeVerticalScrollOffset();
        Assert.assertTrue("Controller failed to scroll up", startScrollPoint > endScrollPoint);
    }

    /**
     * Verifies that pressing the Daydream controller's 'app' button causes the user to exit
     * fullscreen
     */
    @Test
    @MediumTest
    public void testAppButtonExitsFullscreen() throws TimeoutException {
        mVrBrowserTestFramework.loadUrlAndAwaitInitialization(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_navigation_2d_page"),
                PAGE_LOAD_TIMEOUT_S);
        // Enter fullscreen
        DOMUtils.clickNode(mVrBrowserTestFramework.getCurrentWebContents(), "fullscreen",
                false /* goThroughRootAndroidView */);
        mVrBrowserTestFramework.waitOnJavaScriptStep();
        Assert.assertTrue("Page did not enter fullscreen",
                DOMUtils.isFullscreen(mVrBrowserTestFramework.getCurrentWebContents()));

        NativeUiUtils.clickAppButton(UserFriendlyElementName.NONE, new PointF());
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> {
                    try {
                        return !DOMUtils.isFullscreen(
                                mVrBrowserTestFramework.getCurrentWebContents());
                    } catch (TimeoutException e) {
                        return false;
                    }
                },
                "Page did not exit fullscreen after app button was pressed", POLL_TIMEOUT_LONG_MS,
                POLL_CHECK_INTERVAL_LONG_MS);
        mVrBrowserTestFramework.assertNoJavaScriptErrors();
    }

    /**
     * Verifies that clicking and dragging down while at the top of the page triggers a page
     * refresh. Automation of a manual test case from https://crbug.com/861949.
     */
    @Test
    @MediumTest
    public void testDragRefresh() {
        mVrTestRule.loadUrl(
                VrBrowserTestFramework.getFileUrlForHtmlTestFile("test_controller_scrolling"),
                PAGE_LOAD_TIMEOUT_S);
        waitForPageToBeScrollable(RenderCoordinates.fromWebContents(mVrTestRule.getWebContents()));
        // The navigationStart time should change anytime we refresh, so save the value and compare
        // later.
        // Use a double because apparently returning Unix timestamps as floating point is a logical
        // thing for JavaScript to do and Long.valueOf is afraid of decimal points.
        String navStart = "window.performance.timing.navigationStart";
        final double navigationTimestamp =
                Double.valueOf(mVrBrowserTestFramework.runJavaScriptOrFail(
                                       navStart, POLL_TIMEOUT_SHORT_MS))
                        .doubleValue();

        // Click and drag from near the top center of the page to near the top bottom.
        // Use the NativeUiUtils approach to controller input since we shouldn't be missing anything
        // by bypassing VrCore for this test.
        NativeUiUtils.clickAndDragElement(UserFriendlyElementName.CONTENT_QUAD, new PointF(0, 0.4f),
                new PointF(0, -0.4f), 10 /* numInterpolatedSteps */);

        // Wait for the navigationStart time to be newer than our saved time.
        CriteriaHelper.pollInstrumentationThread(() -> {
            return Double.valueOf(mVrBrowserTestFramework.runJavaScriptOrFail(
                                          navStart, POLL_TIMEOUT_SHORT_MS))
                           .doubleValue()
                    > navigationTimestamp;
        }, "Dragging page down did not refresh page");
    }

    /**
     * Tests that pressing the app button on the Daydream controller exits omnibox text input mode.
     */
    @Test
    @MediumTest
    public void testAppButtonExitsOmniboxTextInput() throws InterruptedException {
        // We should always have the keyboard installed and up to date during automated testing, so
        // this isn't strictly required. However, it may prevent weird issues when running locally
        // if you don't have the keyboard installed for some reason.
        NativeUiUtils.enableMockedKeyboard();
        NativeUiUtils.clickElementAndWaitForUiQuiescence(UserFriendlyElementName.URL, new PointF());
        // This acts as an assert that we're actually in omnibox text input mode. If the omnibox
        // is not actually visible, we'll hit a DCHECK in the native code.
        NativeUiUtils.clickElementAndWaitForUiQuiescence(
                UserFriendlyElementName.OMNIBOX_TEXT_FIELD, new PointF());
        // Wait for the URL bar to re-appear, which we take as a signal that we've exited omnibox
        // text input mode.
        NativeUiUtils.performActionAndWaitForVisibilityStatus(
                UserFriendlyElementName.URL, true /* visible */, () -> {
                    NativeUiUtils.clickAppButton(UserFriendlyElementName.NONE, new PointF());
                });
    }
}
