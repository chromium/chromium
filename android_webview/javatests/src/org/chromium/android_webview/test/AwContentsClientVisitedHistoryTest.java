// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.TestAwContentsClient.DoUpdateVisitedHistoryHelper;
import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.util.TestWebServer;

/** Tests for AwContentsClient.getVisitedHistory and AwContents.doUpdateVisitedHistory callbacks. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientVisitedHistoryTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static class GetVisitedHistoryHelper extends CallbackHelper {
        private Callback<String[]> mCallback;
        private boolean mSaveCallback;

        public Callback<String[]> getCallback() {
            assert getCallCount() > 0;
            return mCallback;
        }

        public void setSaveCallback(boolean value) {
            mSaveCallback = value;
        }

        public void notifyCalled(Callback<String[]> callback) {
            if (mSaveCallback) {
                mCallback = callback;
            }
            notifyCalled();
        }
    }

    private static class VisitedHistoryTestAwContentsClient extends TestAwContentsClient {

        private GetVisitedHistoryHelper mGetVisitedHistoryHelper;

        public VisitedHistoryTestAwContentsClient() {
            mGetVisitedHistoryHelper = new GetVisitedHistoryHelper();
        }

        public GetVisitedHistoryHelper getGetVisitedHistoryHelper() {
            return mGetVisitedHistoryHelper;
        }

        @Override
        public void getVisitedHistory(Callback<String[]> callback) {
            getGetVisitedHistoryHelper().notifyCalled(callback);
        }
    }

    private VisitedHistoryTestAwContentsClient mContentsClient =
            new VisitedHistoryTestAwContentsClient();

    public AwContentsClientVisitedHistoryTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testUpdateVisitedHistoryCallback() throws Throwable {
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        // Load a page with an iframe to make sure the callback only happens for the main frame URL.
        final String path = "/testUpdateVisitedHistoryCallback.html";
        final String html = "<iframe src=\"about:blank\"></iframe>";

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse(path, html, null);
            final DoUpdateVisitedHistoryHelper doUpdateVisitedHistoryHelper =
                    mContentsClient.getDoUpdateVisitedHistoryHelper();
            int callCount = doUpdateVisitedHistoryHelper.getCallCount();
            mActivityTestRule.loadUrlAsync(awContents, pageUrl);
            doUpdateVisitedHistoryHelper.waitForCallback(callCount);
            Assert.assertEquals(pageUrl, doUpdateVisitedHistoryHelper.getUrl());
            Assert.assertEquals(false, doUpdateVisitedHistoryHelper.getIsReload());
            Assert.assertEquals(callCount + 1, doUpdateVisitedHistoryHelper.getCallCount());

            // Reload
            mActivityTestRule.loadUrlAsync(awContents, pageUrl);
            doUpdateVisitedHistoryHelper.waitForCallback(callCount + 1);
            Assert.assertEquals(pageUrl, doUpdateVisitedHistoryHelper.getUrl());
            Assert.assertEquals(true, doUpdateVisitedHistoryHelper.getIsReload());
            Assert.assertEquals(callCount + 2, doUpdateVisitedHistoryHelper.getCallCount());
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGetVisitedHistoryExerciseCodePath() throws Throwable {
        // Due to security/privacy restrictions around the :visited css property, it is not
        // possible test this end to end without using the flaky and brittle capturing picture of
        // the web page. So we are doing the next best thing, exercising all the code paths.
        final GetVisitedHistoryHelper visitedHistoryHelper =
                mContentsClient.getGetVisitedHistoryHelper();
        final int callCount = visitedHistoryHelper.getCallCount();
        visitedHistoryHelper.setSaveCallback(true);

        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();

        final String path = "/testGetVisitedHistoryExerciseCodePath.html";
        final String visitedLinks[] = {"http://foo.com", "http://bar.com", null};
        final String html = "<a src=\"http://foo.com\">foo</a><a src=\"http://bar.com\">bar</a>";

        TestWebServer webServer = TestWebServer.start();
        try {
            final String pageUrl = webServer.setResponse(path, html, null);
            mActivityTestRule.loadUrlSync(
                    awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
            visitedHistoryHelper.waitForCallback(callCount);
            Assert.assertNotNull(visitedHistoryHelper.getCallback());

            visitedHistoryHelper.getCallback().onResult(visitedLinks);
            visitedHistoryHelper.getCallback().onResult(null);

            mActivityTestRule.loadUrlSync(
                    awContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        } finally {
            webServer.shutdown();
        }
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testGetVisitedHistoryCallbackAfterDestroy() throws Throwable {
        GetVisitedHistoryHelper visitedHistoryHelper = mContentsClient.getGetVisitedHistoryHelper();
        visitedHistoryHelper.setSaveCallback(true);
        final int callCount = visitedHistoryHelper.getCallCount();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        AwContents awContents = testView.getAwContents();
        mActivityTestRule.loadUrlAsync(awContents, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        visitedHistoryHelper.waitForCallback(callCount);
        Assert.assertNotNull(visitedHistoryHelper.getCallback());

        mActivityTestRule.destroyAwContentsOnMainSync(awContents);
        visitedHistoryHelper.getCallback().onResult(new String[] {"abc.def"});
        visitedHistoryHelper.getCallback().onResult(null);
    }
}
