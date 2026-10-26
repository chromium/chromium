// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.graphics.PointF;
import android.os.Bundle;
import android.view.View;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** End-to-end instrumentation tests for Custom Tabs real-time engagement signals. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "Tests CCT startup, activity lifecycle, and window interactions.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Device(DeviceFormFactor.DESKTOP) // https://crbug.com/578060232
public class CustomTabEngagementSignalsTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final class GestureStateListenerImpl extends GestureStateListener {
        public final CallbackHelper scrollEndCallbackHelper = new CallbackHelper();

        @Override
        public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
            scrollEndCallbackHelper.notifyCalled();
        }
    }

    private static final String TEST_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<!DOCTYPE html><html><head>"
                            + "<meta name='viewport' content='width=device-width,"
                            + " initial-scale=1.0'>"
                            + "<style>body { margin: 0; height: 150svh; "
                            + "background: linear-gradient(blue, red); }</style></head>"
                            + "<body><p>Engagement Signals Test</p></body></html>");
    private static final int TIMEOUT_SECONDS = 30;

    private final PayloadCallbackHelper<Boolean> mScrollDirectionCallbackHelper =
            new PayloadCallbackHelper<>();
    private final PayloadCallbackHelper<Integer> mScrollPercentageCallbackHelper =
            new PayloadCallbackHelper<>();
    private final PayloadCallbackHelper<Boolean> mSessionEndedCallbackHelper =
            new PayloadCallbackHelper<>();
    private final GestureStateListenerImpl mGestureStateListener = new GestureStateListenerImpl();
    private boolean mInitialUsageAndCrashReportingPermitted;

    @Before
    public void setUp() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrivacyPreferencesManagerImpl privacyPreferencesManager =
                            PrivacyPreferencesManagerImpl.getInstance();
                    mInitialUsageAndCrashReportingPermitted =
                            privacyPreferencesManager.isUsageAndCrashReportingPermittedByUser();
                    privacyPreferencesManager.setUsageAndCrashReporting(true);
                });
        CustomTabsSession session =
                CustomTabsTestUtils.bindWithCallback(new CustomTabsCallback()).session;
        assertTrue(session.isEngagementSignalsApiAvailable(Bundle.EMPTY));
        EngagementSignalsCallback callback =
                new EngagementSignalsCallback() {
                    @Override
                    public void onVerticalScrollEvent(boolean isDirectionUp, Bundle extras) {
                        mScrollDirectionCallbackHelper.notifyCalled(isDirectionUp);
                    }

                    @Override
                    public void onGreatestScrollPercentageIncreased(
                            int scrollPercentage, Bundle extras) {
                        mScrollPercentageCallbackHelper.notifyCalled(scrollPercentage);
                    }

                    @Override
                    public void onSessionEnded(boolean didUserInteract, Bundle extras) {
                        mSessionEndedCallbackHelper.notifyCalled(didUserInteract);
                    }
                };
        assertTrue(session.setEngagementSignalsCallback(callback, Bundle.EMPTY));

        Intent intent =
                new CustomTabsIntent.Builder(session).setUrlBarHidingEnabled(false).build().intent;
        mCustomTabActivityTestRule.prepareUrlIntent(intent, TEST_URL);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        WebContents webContents = mCustomTabActivityTestRule.getWebContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    GestureListenerManager.fromWebContents(webContents)
                            .addListener(mGestureStateListener);
                });
        ChromeTabUtils.waitForInteractable(mCustomTabActivityTestRule.getActivityTab());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            RenderCoordinates.fromWebContents(webContents)
                                    .getMaxVerticalScrollPixInt(),
                            Matchers.greaterThan(0));
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();
                    if (activity != null && activity.getActivityTab() != null) {
                        WebContents webContents = activity.getActivityTab().getWebContents();
                        if (webContents != null && !webContents.isDestroyed()) {
                            GestureListenerManager.fromWebContents(webContents)
                                    .removeListener(mGestureStateListener);
                        }
                    }
                    PrivacyPreferencesManagerImpl.getInstance()
                            .setUsageAndCrashReporting(mInitialUsageAndCrashReportingPermitted);
                });
        CustomTabsTestUtils.cleanupSessions();
    }

    /**
     * Returns the starting screen coordinates for a vertical drag on the visible {@link
     * WebContents} area (below top browser controls): 75% down the visible height when dragging up
     * (scrolling down), and 25% down the visible height when dragging down (scrolling up).
     */
    private PointF getDragStartOnScreen(boolean isDragUp) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    View containerView =
                            mCustomTabActivityTestRule
                                    .getWebContents()
                                    .getViewAndroidDelegate()
                                    .getContainerView();
                    int[] location = new int[2];
                    containerView.getLocationOnScreen(location);
                    int topControlsHeight =
                            mCustomTabActivityTestRule
                                    .getActivity()
                                    .getBrowserControlsManager()
                                    .getTopControlsHeight();
                    int visibleHeight = containerView.getHeight() - topControlsHeight;
                    float yFraction = isDragUp ? 0.75f : 0.25f;
                    return new PointF(
                            location[0] + containerView.getWidth() * 0.5f,
                            location[1] + topControlsHeight + visibleHeight * yFraction);
                });
    }

    /** Returns the maximum vertical scroll range of the page in physical pixels. */
    private int getMaxScrollPix() {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        RenderCoordinates.fromWebContents(
                                        mCustomTabActivityTestRule.getWebContents())
                                .getMaxVerticalScrollPixInt());
    }

    /**
     * Performs a vertical drag by the given amount in physical pixels without generating a fling,
     * starting at 75% of the visible viewport height when dragging up (scrolling down) or 25% when
     * dragging down (scrolling up).
     *
     * <p>Wall-clock pacing is required because simulated-time drags dispatch every {@code
     * ACTION_MOVE} back-to-back and the compositor's scroll resampler drops the trailing delta,
     * under-scrolling the page.
     *
     * @param signedSwipeAmount A signed value that indicates the vertical distance from the touch
     *     down to the touch up location (negative drags finger up / scrolls page down; positive
     *     drags finger down / scrolls page up).
     */
    private void performDrag(int signedSwipeAmount) throws TimeoutException {
        PointF start = getDragStartOnScreen(signedSwipeAmount < 0);
        int scrollEndCount = mGestureStateListener.scrollEndCallbackHelper.getCallCount();
        TouchCommon.performWallClockDrag(
                mCustomTabActivityTestRule.getActivity(),
                start.x,
                start.x,
                start.y,
                start.y + signedSwipeAmount,
                /* duration= */ 250,
                /* dispatchIntervalMs= */ 16,
                /* preventFling= */ true);
        mGestureStateListener.scrollEndCallbackHelper.waitForCallback(
                scrollEndCount, 1, TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /** Returns the scroll direction reported at the given callback index. */
    private boolean getScrollDirection(int index) {
        return mScrollDirectionCallbackHelper.getPayloadByIndexBlocking(
                index, TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /** Returns the scroll percentage reported at the given callback index. */
    private int getScrollPercentage(int index) {
        int percent =
                mScrollPercentageCallbackHelper.getPayloadByIndexBlocking(
                        index, TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertEquals("Reported a value that is not a multiple of 5!", 0, percent % 5);
        return percent;
    }

    private void waitForScrollPercentage(Matcher<Integer> matcher, int previousCallCount) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    int callCount = mScrollPercentageCallbackHelper.getCallCount();
                    Criteria.checkThat(callCount, Matchers.greaterThan(previousCallCount));
                    Criteria.checkThat(getScrollPercentage(callCount - 1), matcher);
                });
    }

    @Test
    @LargeTest
    public void testScrollDirectionSignal() throws TimeoutException {
        int maxScrollPx = getMaxScrollPix();

        int directionCallCount = mScrollDirectionCallbackHelper.getCallCount();
        performDrag(-Math.round(maxScrollPx * 0.6f));
        assertFalse(getScrollDirection(directionCallCount));

        directionCallCount = mScrollDirectionCallbackHelper.getCallCount();
        performDrag(Math.round(maxScrollPx * 0.3f));
        assertTrue(getScrollDirection(directionCallCount));
    }

    @Test
    @LargeTest
    public void testJavascriptScrollDoesNotSendSignals() throws TimeoutException {
        WebContents webContents = mCustomTabActivityTestRule.getWebContents();
        final int scrollDirectionCallCount = mScrollDirectionCallbackHelper.getCallCount();
        final int scrollPercentageCallCount = mScrollPercentageCallbackHelper.getCallCount();

        // Scroll down then up using JavaScript.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, "window.scrollTo(0, document.body.scrollHeight / 2);");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, "window.scrollTo(0, 0);");

        assertEquals(
                "Scroll direction signals shouldn't be sent for JS events.",
                scrollDirectionCallCount,
                mScrollDirectionCallbackHelper.getCallCount());
        assertEquals(
                "Scroll percentage signals shouldn't be sent for JS events.",
                scrollPercentageCallCount,
                mScrollPercentageCallbackHelper.getCallCount());
    }

    @Test
    @LargeTest
    public void testScrollPercentageSignal() throws TimeoutException {
        final int scrollPercentageCallCount = mScrollPercentageCallbackHelper.getCallCount();
        // Because the page is 150svh, maxScrollPx is 0.5x the visible viewport height.
        // Dragging by 0.7x of maxScrollPx (0.35x of the visible viewport height) from 75% to 40%
        // of the visible viewport scrolls > 50% (and < 100%) on the first drag and reaches 100% on
        // the second drag.
        int maxScrollPx = getMaxScrollPix();
        int dragDownAmount = -Math.round(maxScrollPx * 0.7f);

        // Scroll down at least 50%.
        performDrag(dragDownAmount);
        waitForScrollPercentage(Matchers.greaterThanOrEqualTo(50), scrollPercentageCallCount);

        // Scroll down a 2nd time to reach the end of the page (100%).
        int callCountBeforeSecondScroll = mScrollPercentageCallbackHelper.getCallCount();
        performDrag(dragDownAmount);
        waitForScrollPercentage(Matchers.is(100), callCountBeforeSecondScroll);

        // Scroll back up and verify no new scroll percentage signal is sent.
        int callCountAfterSecondScroll = mScrollPercentageCallbackHelper.getCallCount();
        int directionCallCount = mScrollDirectionCallbackHelper.getCallCount();
        performDrag(Math.round(maxScrollPx * 0.3f));
        assertTrue(getScrollDirection(directionCallCount));
        assertEquals(
                "Scroll percentage signal shouldn't be sent after scrolling back up.",
                callCountAfterSecondScroll,
                mScrollPercentageCallbackHelper.getCallCount());
    }

    @Test
    @LargeTest
    public void testSessionEndedSignal() throws TimeoutException {
        final int sessionEndedCallCount = mSessionEndedCallbackHelper.getCallCount();
        int maxScrollPx = getMaxScrollPix();

        // Interact with the page by scrolling down.
        int directionCallCount = mScrollDirectionCallbackHelper.getCallCount();
        performDrag(-Math.round(maxScrollPx * 0.6f));
        assertFalse(getScrollDirection(directionCallCount));

        // Close the Custom Tab via the close button.
        ClickUtils.clickButton(
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.close_button));
        boolean didUserInteract =
                mSessionEndedCallbackHelper.getPayloadByIndexBlocking(
                        sessionEndedCallCount, TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue(didUserInteract);
    }
}
