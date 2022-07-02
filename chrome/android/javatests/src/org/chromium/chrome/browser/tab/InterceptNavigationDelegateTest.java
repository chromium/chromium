// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Tests for InterceptNavigationDelegate
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class InterceptNavigationDelegateTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String BASE_PAGE = "/chrome/test/data/navigation_interception/";
    private static final String NAVIGATION_FROM_TIMEOUT_PAGE =
            BASE_PAGE + "navigation_from_timer.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_PAGE =
            BASE_PAGE + "navigation_from_user_gesture.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_PAGE =
            BASE_PAGE + "navigation_from_xhr_callback.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE =
            BASE_PAGE + "navigation_from_xhr_callback_and_short_timeout.html";
    private static final String NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE =
            BASE_PAGE + "navigation_from_xhr_callback_and_long_timeout.html";
    private static final String NAVIGATION_FROM_IMAGE_ONLOAD_PAGE =
            BASE_PAGE + "navigation_from_image_onload.html";
    private static final String NAVIGATION_FROM_USER_GESTURE_IFRAME_PAGE =
            BASE_PAGE + "navigation_from_user_gesture_to_iframe_page.html";
    private static final String NAVIGATION_FROM_PRERENDERING_PAGE =
            BASE_PAGE + "navigation_from_prerender.html";

    private static final long DEFAULT_MAX_TIME_TO_WAIT_IN_MS = 3000;
    private static final long LONG_MAX_TIME_TO_WAIT_IN_MS = 20000;

    private ChromeActivity mActivity;
    private List<NavigationHandle> mNavParamHistory = new ArrayList<>();
    private List<ExternalNavigationParams> mExternalNavParamHistory = new ArrayList<>();
    private EmbeddedTestServer mTestServer;

    class TestExternalNavigationHandler extends ExternalNavigationHandler {
        public TestExternalNavigationHandler() {
            super(new ExternalNavigationDelegateImpl(mActivity.getActivityTab()));
        }

        @Override
        public OverrideUrlLoadingResult shouldOverrideUrlLoading(ExternalNavigationParams params) {
            mExternalNavParamHistory.add(params);
            return OverrideUrlLoadingResult.forNoOverride();
        }
    }

    private void waitTillExpectedCallsComplete(int count, long timeout) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mNavParamHistory.size(), Matchers.is(count));
        }, timeout, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Before
    public void setUp() throws Exception {
        mActivity = sActivityTestRule.getActivity();
        final Tab tab = mActivity.getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InterceptNavigationDelegateClientImpl client =
                    new InterceptNavigationDelegateClientImpl(tab);
            InterceptNavigationDelegateImpl delegate = new InterceptNavigationDelegateImpl(client) {
                @Override
                public boolean shouldIgnoreNavigation(NavigationHandle navigationHandle,
                        GURL escapedUrl, boolean applyUserGestureCarryover) {
                    mNavParamHistory.add(navigationHandle);
                    return super.shouldIgnoreNavigation(
                            navigationHandle, escapedUrl, applyUserGestureCarryover);
                }
            };
            client.initializeWithDelegate(delegate);
            delegate.setExternalNavigationHandler(new TestExternalNavigationHandler());
            delegate.associateWithWebContents(tab.getWebContents());
        });
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @SmallTest
    public void testNavigationFromTimer() {
        sActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_TIMEOUT_PAGE));
        Assert.assertEquals(1, mNavParamHistory.size());

        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        Assert.assertFalse(mNavParamHistory.get(1).hasUserGesture());
    }

    @Test
    @SmallTest
    public void testNavigationFromUserGesture() throws TimeoutException {
        sActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_PAGE));
        Assert.assertEquals(1, mNavParamHistory.size());

        TouchCommon.singleClickView(mActivity.getActivityTab().getView());
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        Assert.assertTrue(mNavParamHistory.get(1).hasUserGesture());
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallback() throws TimeoutException {
        sActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_PAGE));
        Assert.assertEquals(1, mNavParamHistory.size());

        TouchCommon.singleClickView(mActivity.getActivityTab().getView());
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);

        Assert.assertTrue(mNavParamHistory.get(1).hasUserGesture());
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackAndShortTimeout() throws TimeoutException {
        sActivityTestRule.loadUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_SHORT_TIMEOUT_PAGE));
        Assert.assertEquals(1, mNavParamHistory.size());

        TouchCommon.singleClickView(mActivity.getActivityTab().getView());
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);

        Assert.assertTrue(mNavParamHistory.get(1).hasUserGesture());
    }

    @Test
    @SmallTest
    public void testNavigationFromXHRCallbackAndLongTimeout() throws TimeoutException {
        sActivityTestRule.loadUrl(
                mTestServer.getURL(NAVIGATION_FROM_XHR_CALLBACK_AND_LONG_TIMEOUT_PAGE));
        Assert.assertEquals(1, mNavParamHistory.size());

        TouchCommon.singleClickView(mActivity.getActivityTab().getView());
        waitTillExpectedCallsComplete(2, LONG_MAX_TIME_TO_WAIT_IN_MS);
        Assert.assertFalse(mNavParamHistory.get(1).hasUserGesture());
    }

    @Test
    @SmallTest
    public void testNavigationFromImageOnLoad() throws TimeoutException {
        sActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_IMAGE_ONLOAD_PAGE));
        Assert.assertEquals(1, mNavParamHistory.size());

        TouchCommon.singleClickView(mActivity.getActivityTab().getView());
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);

        Assert.assertTrue(mNavParamHistory.get(1).hasUserGesture());
    }

    @Test
    @MediumTest
    public void testExternalAppIframeNavigation() throws TimeoutException {
        sActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_USER_GESTURE_IFRAME_PAGE));
        Assert.assertEquals(1, mNavParamHistory.size());

        TouchCommon.singleClickView(mActivity.getActivityTab().getView());
        waitTillExpectedCallsComplete(3, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        Assert.assertEquals(3, mExternalNavParamHistory.size());

        Assert.assertTrue(mNavParamHistory.get(2).isExternalProtocol());
        Assert.assertFalse(mNavParamHistory.get(2).isInPrimaryMainFrame());
        Assert.assertTrue(
                mExternalNavParamHistory.get(2).getRedirectHandler().shouldStayInApp(true, false));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.PRERENDER2})
    public void testExternalAppPrerenderingNavigation() throws TimeoutException {
        // Ensure that a prerendering main frame doesn't call into the delegate.
        sActivityTestRule.loadUrl(mTestServer.getURL(NAVIGATION_FROM_PRERENDERING_PAGE));
        Assert.assertEquals(1, mNavParamHistory.size());

        // The click will reload the page with a user gesture. The delegate
        // should still only hear about the navigation in the primary main
        // frame, not the prerendering one.
        TouchCommon.singleClickView(mActivity.getActivityTab().getView());
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);
        Assert.assertEquals(2, mNavParamHistory.size());
        Assert.assertEquals(2, mExternalNavParamHistory.size());
    }
}
