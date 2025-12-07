// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.json.JSONArray;
import org.json.JSONObject;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwPage;
import org.chromium.android_webview.AwWebContentsObserver;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.time.Duration;
import java.time.temporal.ChronoUnit;

/** Tests for the AwWebContentsObserver class. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwWebContentsObserverTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private TestAwNavigationListener mNavigationListener;
    private AwTestContainerView mTestContainerView;
    private AwWebContentsObserver mWebContentsObserver;
    private TestWebServer mWebServer;

    private GURL mExampleURL;
    private GURL mExampleURLWithFragment;
    private GURL mSyncURL;
    private GURL mUnreachableWebDataUrl;

    private static final String JS_OBJECT_NAME = "testListener";
    private static final String WEB_PERFORMANCE_METRICS_HTML =
            """
                <html>
                <head>
                <title>Hello, World!</title>
                <script>
                        const observer = new PerformanceObserver((list) => {
                                testListener.postMessage(JSON.stringify(list.getEntries()));
                        });
                        observer.observe({entryTypes: ["paint"]});
                </script>
                </head>
                <body>
                Hello, World!
                </body>
                </html>
            """;

    public AwWebContentsObserverTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mNavigationListener = new TestAwNavigationListener();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mTestContainerView.getAwContents().getNavigationClient().addListener(mNavigationListener);
        mWebContentsObserver =
                mTestContainerView.getAwContents().getWebContentsObserverForTesting();
        mWebServer = TestWebServer.start();
        mUnreachableWebDataUrl = new GURL(AwContentsStatics.getUnreachableWebDataUrl());
        mExampleURL = new GURL("http://www.example.com/");
        mExampleURLWithFragment = new GURL("http://www.example.com/#anchor");
        mSyncURL = new GURL("http://example.org/");
    }

    @After
    public void tearDown() {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinished() throws Throwable {
        GlobalRenderFrameHostId frameId = new GlobalRenderFrameHostId(-1, -1);
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int callCount = onPageFinishedHelper.getCallCount();
        Page page = Page.createForTesting();
        mWebContentsObserver.didFinishLoadInPrimaryMainFrame(
                page, frameId, mExampleURL, true, LifecycleState.ACTIVE);
        mWebContentsObserver.didStopLoading(mExampleURL, true);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "onPageFinished should be called for main frame navigations.",
                callCount + 1,
                onPageFinishedHelper.getCallCount());
        Assert.assertEquals(
                "onPageFinished should be called for main frame navigations.",
                mExampleURL.getSpec(),
                onPageFinishedHelper.getUrl());
        // Check that onPageLoadEventFired() is called with the correct page.
        AwPage awPageWithLoadEventFired = mNavigationListener.getLastPageWithLoadEventFired();
        Assert.assertNotNull(awPageWithLoadEventFired);
        Assert.assertEquals(page, awPageWithLoadEventFired.getInternalPageForTesting());

        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoadInPrimaryMainFrame(
                Page.createForTesting(),
                frameId,
                mUnreachableWebDataUrl,
                false,
                LifecycleState.ACTIVE);
        mWebContentsObserver.didFinishLoadInPrimaryMainFrame(
                Page.createForTesting(), frameId, mSyncURL, true, LifecycleState.ACTIVE);
        mWebContentsObserver.didStopLoading(mSyncURL, true);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "onPageFinished should not be called for the error url.",
                callCount + 1,
                onPageFinishedHelper.getCallCount());
        Assert.assertEquals(
                "onPageFinished should not be called for the error url.",
                mSyncURL.getSpec(),
                onPageFinishedHelper.getUrl());

        boolean isErrorPage = false;
        boolean isSameDocument = true;
        boolean fragmentNavigation = true;
        boolean isRendererInitiated = true;
        callCount = onPageFinishedHelper.getCallCount();
        simulateNavigation(
                mExampleURL,
                isErrorPage,
                !isSameDocument,
                !fragmentNavigation,
                !isRendererInitiated,
                PageTransition.TYPED);
        simulateNavigation(
                mExampleURLWithFragment,
                isErrorPage,
                isSameDocument,
                fragmentNavigation,
                isRendererInitiated,
                PageTransition.TYPED);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "onPageFinished should be called for main frame fragment navigations.",
                callCount + 1,
                onPageFinishedHelper.getCallCount());
        Assert.assertEquals(
                "onPageFinished should be called for main frame fragment navigations.",
                mExampleURLWithFragment.getSpec(),
                onPageFinishedHelper.getUrl());

        callCount = onPageFinishedHelper.getCallCount();
        simulateNavigation(
                mExampleURL,
                isErrorPage,
                !isSameDocument,
                !fragmentNavigation,
                !isRendererInitiated,
                PageTransition.TYPED);
        mWebContentsObserver.didFinishLoadInPrimaryMainFrame(
                Page.createForTesting(), frameId, mSyncURL, true, LifecycleState.ACTIVE);
        mWebContentsObserver.didStopLoading(mSyncURL, true);
        onPageFinishedHelper.waitForCallback(callCount);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "onPageFinished should be called only for main frame fragment navigations.",
                callCount + 1,
                onPageFinishedHelper.getCallCount());
        Assert.assertEquals(
                "onPageFinished should be called only for main frame fragment navigations.",
                mSyncURL.getSpec(),
                onPageFinishedHelper.getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDidFinishNavigation() throws Throwable {
        GURL emptyUrl = GURL.emptyGURL();
        boolean isErrorPage = false;
        boolean isSameDocument = true;
        boolean fragmentNavigation = false;
        boolean isRendererInitiated = false;
        TestAwContentsClient.DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                mContentsClient.getDoUpdateVisitedHistoryHelper();

        int callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(
                emptyUrl,
                !isErrorPage,
                !isSameDocument,
                fragmentNavigation,
                isRendererInitiated,
                PageTransition.TYPED);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "doUpdateVisitedHistory should be called for any url.",
                callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals(
                "doUpdateVisitedHistory should be called for any url.",
                emptyUrl.getSpec(),
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(
                mExampleURL,
                isErrorPage,
                !isSameDocument,
                fragmentNavigation,
                isRendererInitiated,
                PageTransition.TYPED);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "doUpdateVisitedHistory should be called for any url.",
                callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals(
                "doUpdateVisitedHistory should be called for any url.",
                mExampleURL.getSpec(),
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(
                mExampleURL,
                isErrorPage,
                isSameDocument,
                !fragmentNavigation,
                !isRendererInitiated,
                PageTransition.RELOAD);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "doUpdateVisitedHistory should be called for reloads.",
                callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals(
                "doUpdateVisitedHistory should be called for reloads.",
                mExampleURL.getSpec(),
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(true, doUpdateVisitedHistoryHelper.getIsReload());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFirstContentfulPaint() throws Throwable {
        mActivityTestRule
                .getAwSettingsOnUiThread(mTestContainerView.getAwContents())
                .setJavaScriptEnabled(true);

        TestWebMessageListener listener = new TestWebMessageListener();
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                mTestContainerView.getAwContents(), JS_OBJECT_NAME, new String[] {"*"}, listener);

        String testPage =
                mWebServer.setResponse(
                        "/web_performance_metrics.html", WEB_PERFORMANCE_METRICS_HTML, null);

        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                testPage);

        // Wait for paint event to occur and js fcp load time to be returned via postmessage
        TestWebMessageListener.Data data = listener.waitForOnPostMessage();

        // Note: js value is in milliseconds, navigation client value is in microseconds
        JSONObject jsFCPTimeData = new JSONArray(data.getAsString()).getJSONObject(1);
        Duration jsFCP = Duration.ofMillis((long) jsFCPTimeData.getDouble("startTime"));
        Long navigationFCPTime = mNavigationListener.getLastFirstContentfulPaintLoadTime();
        Assert.assertNotNull(navigationFCPTime);
        Duration navigationFCP = Duration.of(navigationFCPTime, ChronoUnit.MICROS);

        // Note: The two time values may differ slightly. This is primarily due to
        // coarsening for security reasons. We check here for a difference of 5 milliseconds
        // as at a minimum we need to account for paint timing coarsening to the next multiple of
        // 4 milliseconds, or coarser, when cross-origin isolated capability is false.
        // See: https://w3c.github.io/paint-timing/#mark-paint-timing
        // and https://developer.mozilla.org/en-US/docs/Web/API/DOMHighResTimeStamp
        // and
        // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/paint/timing/paint_timing.cc
        Assert.assertTrue(jsFCP.minus(navigationFCP).abs().toMillis() < 5);
    }

    private void simulateNavigation(
            GURL gurl,
            boolean isErrorPage,
            boolean isSameDocument,
            boolean isFragmentNavigation,
            boolean isRendererInitiated,
            int transition) {
        NavigationHandle navigation =
                NavigationHandle.createForTesting(
                        gurl,
                        /* isInPrimaryMainFrame= */ true,
                        isSameDocument,
                        isRendererInitiated,
                        transition,
                        /* hasUserGesture= */ false,
                        /* isReload= */ false);
        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(navigation);

        // Check that onNavigationStarted() is called correctly.
        AwNavigation awNavigationStart = mNavigationListener.getLastStartedNavigation();
        Assert.assertNotNull(awNavigationStart);
        Assert.assertEquals(
                "onNavigationStarted should have the intended URL",
                gurl.getSpec(),
                awNavigationStart.getUrl());
        Assert.assertEquals(
                "onNavigationStarted should have the intended isSameDocument",
                isSameDocument,
                awNavigationStart.isSameDocument());
        Assert.assertEquals(
                "onNavigationStarted should have the intended wasInitiatedByPage",
                isRendererInitiated,
                awNavigationStart.wasInitiatedByPage());
        Assert.assertFalse(
                "onNavigationStarted should have the intended isReload",
                awNavigationStart.isReload());
        Assert.assertFalse(
                "onNavigationStarted should have a false didCommit", awNavigationStart.didCommit());
        Assert.assertFalse(
                "onNavigationStarted should have a false didCommitErrorPage",
                awNavigationStart.didCommitErrorPage());
        Assert.assertNull("onNavigationStarted should have null page", awNavigationStart.getPage());

        @Nullable Page page = Page.createForTesting();
        navigation.didFinish(
                gurl,
                isErrorPage,
                /* hasCommitted= */ true,
                isFragmentNavigation,
                /* isDownload= */ false,
                /* isValidSearchFormUrl= */ false,
                transition,
                /* errorCode= */ 0,
                /* httpStatuscode= */ 200,
                /* isExternalProtocol= */ false,
                /* isPdf= */ false,
                /* mimeType= */ "",
                page);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(navigation);

        // Check that onNavigationCompleted() is called correctly.
        AwNavigation awNavigationComplete = mNavigationListener.getLastCompletedNavigation();
        Assert.assertNotNull(awNavigationComplete);
        Assert.assertEquals(
                "The AwNavigation passed at start & complete should be the same",
                awNavigationStart,
                awNavigationComplete);
        Assert.assertEquals(
                "onNavigationCompleted should have the same URL.",
                gurl.getSpec(),
                awNavigationComplete.getUrl());
        Assert.assertEquals(
                "onNavigationCompleted should have the intended isSameDocument",
                isSameDocument,
                awNavigationComplete.isSameDocument());
        Assert.assertEquals(
                "onNavigationCompleted should have the intended wasInitiatedByPage",
                isRendererInitiated,
                awNavigationComplete.wasInitiatedByPage());
        Assert.assertFalse(
                "onNavigationCompleted should have the intended isReload",
                awNavigationComplete.isReload());
        Assert.assertTrue(
                "onNavigationCompleted should have the intended didCommit",
                awNavigationComplete.didCommit());
        Assert.assertEquals(
                "onNavigationCompleted should have the intended error page status",
                isErrorPage,
                awNavigationComplete.didCommitErrorPage());
        Assert.assertEquals(
                "The page passed in didFinish should equal the one in AwNavigation",
                page,
                awNavigationComplete.getPage().getInternalPageForTesting());

        // onNavigationRedirected should not be called.
        Assert.assertNull(mNavigationListener.getLastRedirectedNavigation());
    }
}
