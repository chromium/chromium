// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwWebContentsObserver;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.ui.base.PageTransition;

/**
 * Tests for the AwWebContentsObserver class.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwWebContentsObserverTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwWebContentsObserver mWebContentsObserver;

    private static final String EXAMPLE_URL = "http://www.example.com/";
    private static final String EXAMPLE_URL_WITH_FRAGMENT = "http://www.example.com/#anchor";
    private static final String SYNC_URL = "http://example.org/";
    private String mUnreachableWebDataUrl;

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mUnreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
        // AwWebContentsObserver constructor must be run on the UI thread.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mWebContentsObserver =
                                   new AwWebContentsObserver(mTestContainerView.getWebContents(),
                                           mTestContainerView.getAwContents(), mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOnPageFinished() throws Throwable {
        int frameId = 0;
        boolean mainFrame = true;
        boolean subFrame = false;
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();

        int callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, EXAMPLE_URL, mainFrame);
        mWebContentsObserver.didStopLoading(EXAMPLE_URL);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should be called for main frame navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should be called for main frame navigations.",
                EXAMPLE_URL, onPageFinishedHelper.getUrl());

        // In order to check that callbacks are *not* firing, first we execute code
        // that shoudn't emit callbacks, then code that emits a callback, and check that we
        // have got only one callback, and that its URL is from the last call. Since
        // callbacks are serialized, that means we didn't have a callback for the first call.
        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, EXAMPLE_URL, subFrame);
        mWebContentsObserver.didFinishLoad(frameId, SYNC_URL, mainFrame);
        mWebContentsObserver.didStopLoading(SYNC_URL);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should only be called for the main frame.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should only be called for the main frame.", SYNC_URL,
                onPageFinishedHelper.getUrl());

        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishLoad(frameId, mUnreachableWebDataUrl, mainFrame);
        mWebContentsObserver.didFinishLoad(frameId, SYNC_URL, mainFrame);
        mWebContentsObserver.didStopLoading(SYNC_URL);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should not be called for the error url.", callCount + 1,
                onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should not be called for the error url.", SYNC_URL,
                onPageFinishedHelper.getUrl());

        String baseUrl = null;
        boolean isInMainFrame = true;
        boolean isErrorPage = false;
        boolean isSameDocument = true;
        boolean fragmentNavigation = true;
        boolean isRendererInitiated = true;
        int errorCode = 0;
        int httpStatusCode = 200;
        callCount = onPageFinishedHelper.getCallCount();
        simulateNavigation(EXAMPLE_URL, isInMainFrame, isErrorPage, !isSameDocument,
                !fragmentNavigation, !isRendererInitiated, PageTransition.TYPED);
        simulateNavigation(EXAMPLE_URL_WITH_FRAGMENT, isInMainFrame, isErrorPage, isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should be called for main frame fragment navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should be called for main frame fragment navigations.",
                EXAMPLE_URL_WITH_FRAGMENT, onPageFinishedHelper.getUrl());

        callCount = onPageFinishedHelper.getCallCount();
        simulateNavigation(EXAMPLE_URL, isInMainFrame, isErrorPage, !isSameDocument,
                !fragmentNavigation, !isRendererInitiated, PageTransition.TYPED);
        mWebContentsObserver.didFinishLoad(frameId, SYNC_URL, mainFrame);
        mWebContentsObserver.didStopLoading(SYNC_URL);
        onPageFinishedHelper.waitForCallback(callCount);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals(
                "onPageFinished should be called only for main frame fragment navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals(
                "onPageFinished should be called only for main frame fragment navigations.",
                SYNC_URL, onPageFinishedHelper.getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDidFinishNavigation() throws Throwable {
        String nullUrl = null;
        String baseUrl = null;
        boolean isInMainFrame = true;
        boolean isErrorPage = false;
        boolean isSameDocument = true;
        boolean fragmentNavigation = false;
        boolean isRendererInitiated = false;
        TestAwContentsClient.DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                mContentsClient.getDoUpdateVisitedHistoryHelper();

        int callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(nullUrl, isInMainFrame, !isErrorPage, !isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", nullUrl,
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(EXAMPLE_URL, isInMainFrame, isErrorPage, !isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", EXAMPLE_URL,
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(nullUrl, isInMainFrame, isErrorPage, !isSameDocument, fragmentNavigation,
                isRendererInitiated, PageTransition.TYPED);
        simulateNavigation(EXAMPLE_URL, !isInMainFrame, isErrorPage, !isSameDocument,
                fragmentNavigation, isRendererInitiated, PageTransition.TYPED);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should only be called for the main frame.",
                callCount + 1, doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should only be called for the main frame.",
                nullUrl, doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        simulateNavigation(EXAMPLE_URL, isInMainFrame, isErrorPage, isSameDocument,
                !fragmentNavigation, !isRendererInitiated, PageTransition.RELOAD);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for reloads.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for reloads.", EXAMPLE_URL,
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(true, doUpdateVisitedHistoryHelper.getIsReload());
    }

    private void simulateNavigation(String url, boolean isInMainFrame, boolean isErrorPage,
            boolean isSameDocument, boolean isFragmentNavigation, boolean isRendererInitiated,
            int transition) {
        NavigationHandle navigation = new NavigationHandle(0 /* navigationHandleProxy */, url,
                isInMainFrame, isSameDocument, isRendererInitiated);
        mWebContentsObserver.didStartNavigation(navigation);

        navigation.didFinish(url, isErrorPage, true /* hasCommitted */, isFragmentNavigation,
                false /* isDownload */, false /* isValidSearchFormUrl */, transition,
                0 /* errorCode*/, 200 /* httpStatusCode*/);
        mWebContentsObserver.didFinishNavigation(navigation);
    }
}
