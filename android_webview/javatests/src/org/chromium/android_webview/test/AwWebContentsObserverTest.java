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
    private static final String ERROR_DESCRIPTION = "description";
    private String mUnreachableWebDataUrl;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mUnreachableWebDataUrl = AwContentsStatics.getUnreachableWebDataUrl();
        // AwWebContentsObserver constructor must be run on the UI thread.
        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                ()
                        -> mWebContentsObserver =
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
        boolean hasCommitted = true;
        boolean isSameDocument = true;
        boolean fragmentNavigation = true;
        boolean isRendererInitiated = true;
        boolean isDownload = false;
        int errorCode = 0;
        String errorDescription = "";
        int httpStatusCode = 200;
        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishNavigation(EXAMPLE_URL, isInMainFrame, isErrorPage,
                hasCommitted, !isSameDocument, !fragmentNavigation, !isRendererInitiated,
                isDownload, PageTransition.TYPED, errorCode, errorDescription, httpStatusCode);
        mWebContentsObserver.didFinishNavigation(EXAMPLE_URL_WITH_FRAGMENT, isInMainFrame,
                isErrorPage, hasCommitted, isSameDocument, fragmentNavigation, isRendererInitiated,
                isDownload, PageTransition.TYPED, errorCode, errorDescription, httpStatusCode);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertEquals("onPageFinished should be called for main frame fragment navigations.",
                callCount + 1, onPageFinishedHelper.getCallCount());
        Assert.assertEquals("onPageFinished should be called for main frame fragment navigations.",
                EXAMPLE_URL_WITH_FRAGMENT, onPageFinishedHelper.getUrl());

        callCount = onPageFinishedHelper.getCallCount();
        mWebContentsObserver.didFinishNavigation(EXAMPLE_URL, isInMainFrame, isErrorPage,
                hasCommitted, !isSameDocument, !fragmentNavigation, !isRendererInitiated,
                isDownload, PageTransition.TYPED, errorCode, errorDescription, httpStatusCode);
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
        boolean hasCommitted = true;
        boolean isSameDocument = true;
        boolean fragmentNavigation = false;
        boolean isRendererInitiated = false;
        boolean isDownload = false;
        int errorCode = 0;
        String errorDescription = "";
        int httpStatusCode = 200;
        TestAwContentsClient.DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                mContentsClient.getDoUpdateVisitedHistoryHelper();

        int callCount = doUpdateVisitedHistoryHelper.getCallCount();
        mWebContentsObserver.didFinishNavigation(nullUrl, isInMainFrame, !isErrorPage, hasCommitted,
                !isSameDocument, fragmentNavigation, isRendererInitiated, isDownload,
                PageTransition.TYPED, errorCode, errorDescription, httpStatusCode);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", nullUrl,
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        mWebContentsObserver.didFinishNavigation(EXAMPLE_URL, isInMainFrame, isErrorPage,
                hasCommitted, !isSameDocument, fragmentNavigation, isRendererInitiated, isDownload,
                PageTransition.TYPED, errorCode, errorDescription, httpStatusCode);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for any url.", EXAMPLE_URL,
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        mWebContentsObserver.didFinishNavigation(nullUrl, isInMainFrame, isErrorPage, hasCommitted,
                !isSameDocument, fragmentNavigation, isRendererInitiated, isDownload,
                PageTransition.TYPED, errorCode, errorDescription, httpStatusCode);
        mWebContentsObserver.didFinishNavigation(EXAMPLE_URL, !isInMainFrame, isErrorPage,
                hasCommitted, !isSameDocument, fragmentNavigation, isRendererInitiated, isDownload,
                PageTransition.TYPED, errorCode, errorDescription, httpStatusCode);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should only be called for the main frame.",
                callCount + 1, doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should only be called for the main frame.",
                nullUrl, doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());

        callCount = doUpdateVisitedHistoryHelper.getCallCount();
        mWebContentsObserver.didFinishNavigation(EXAMPLE_URL, isInMainFrame, isErrorPage,
                hasCommitted, isSameDocument, !fragmentNavigation, !isRendererInitiated, isDownload,
                PageTransition.RELOAD, errorCode, errorDescription, httpStatusCode);
        doUpdateVisitedHistoryHelper.waitForCallback(callCount);
        Assert.assertEquals("doUpdateVisitedHistory should be called for reloads.", callCount + 1,
                doUpdateVisitedHistoryHelper.getCallCount());
        Assert.assertEquals("doUpdateVisitedHistory should be called for reloads.", EXAMPLE_URL,
                doUpdateVisitedHistoryHelper.getUrl());
        Assert.assertEquals(true, doUpdateVisitedHistoryHelper.getIsReload());
    }
}
