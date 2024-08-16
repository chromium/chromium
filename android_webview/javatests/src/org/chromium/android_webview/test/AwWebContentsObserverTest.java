// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwWebContentsObserver;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

/** Tests for the AwWebContentsObserver class. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwWebContentsObserverTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwWebContentsObserver mWebContentsObserver;

    private GURL mExampleURL;
    private GURL mExampleURLWithFragment;
    private GURL mSyncURL;
    private GURL mUnreachableWebDataUrl;

    public AwWebContentsObserverTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mUnreachableWebDataUrl = new GURL(AwContentsStatics.getUnreachableWebDataUrl());
        mExampleURL = new GURL("http://www.example.com/");
        mExampleURLWithFragment = new GURL("http://www.example.com/#anchor");
        mSyncURL = new GURL("http://example.org/");
        // AwWebContentsObserver constructor must be run on the UI thread.
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                mWebContentsObserver =
                                        new AwWebContentsObserver(
                                                mTestContainerView.getWebContents(),
                                                mTestContainerView.getAwContents(),
                                                mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinished() throws Throwable {
        GlobalRenderFrameHostId frameId = new GlobalRenderFrameHostId(-1, -1);
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoadInPrimaryMainFrame(
                frameId, mExampleURL, true, LifecycleState.ACTIVE);
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

        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoadInPrimaryMainFrame(
                frameId, mUnreachableWebDataUrl, false, LifecycleState.ACTIVE);
        mWebContentsObserver.didFinishLoadInPrimaryMainFrame(
                frameId, mSyncURL, true, LifecycleState.ACTIVE);
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
                frameId, mSyncURL, true, LifecycleState.ACTIVE);
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

        navigation.didFinish(
                gurl,
                isErrorPage,
                /* hasCommitted= */ true,
                isFragmentNavigation,
                /* isDownload= */ false,
                /* isValidSearchFormUrl= */ false,
                transition,
                /* errorCode= */ 0,
                /* httpStatusCode= */ 200,
                /* isExternalProtocol= */ false,
                /* isPdf= */ false,
                /* mimeType= */ "",
                /* isSaveableNavigation= */ false);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(navigation);
    }
}
