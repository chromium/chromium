// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.ExternalNavigationParams;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for InterceptNavigationDelegate */
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
    private CallbackHelper mSubframeExternalProtocolCalled = new CallbackHelper();
    private GURL mSubframeRedirectTarget;

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
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mNavParamHistory.size(), Matchers.is(count));
                },
                timeout,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Before
    public void setUp() throws Exception {
        mActivity = sActivityTestRule.getActivity();
        final Tab tab = mActivity.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InterceptNavigationDelegateClientImpl client =
                            new InterceptNavigationDelegateClientImpl(tab);
                    InterceptNavigationDelegateImpl delegate =
                            new InterceptNavigationDelegateImpl(client) {
                                @Override
                                public boolean shouldIgnoreNavigation(
                                        NavigationHandle navigationHandle,
                                        GURL escapedUrl,
                                        boolean hiddenCrossFrame,
                                        boolean isSandboxedFrame) {
                                    mNavParamHistory.add(navigationHandle);
                                    return super.shouldIgnoreNavigation(
                                            navigationHandle,
                                            escapedUrl,
                                            hiddenCrossFrame,
                                            isSandboxedFrame);
                                }

                                @Override
                                public GURL handleSubframeExternalProtocol(
                                        GURL escapedUrl,
                                        @PageTransition int transition,
                                        boolean hasUserGesture,
                                        Origin initiatorOrigin) {
                                    mSubframeExternalProtocolCalled.notifyCalled();
                                    if (mSubframeRedirectTarget != null) {
                                        return mSubframeRedirectTarget;
                                    }
                                    return super.handleSubframeExternalProtocol(
                                            escapedUrl,
                                            transition,
                                            hasUserGesture,
                                            initiatorOrigin);
                                }
                            };
                    client.initializeWithDelegate(delegate);
                    delegate.setExternalNavigationHandler(new TestExternalNavigationHandler());
                    delegate.associateWithWebContents(tab.getWebContents());
                });
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
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
        waitTillExpectedCallsComplete(2, DEFAULT_MAX_TIME_TO_WAIT_IN_MS);

        mSubframeExternalProtocolCalled.waitForOnly();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.PRERENDER2)
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
