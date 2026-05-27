// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.net.test.util.TestWebServer;

import java.io.InputStream;
import java.net.URL;

/** Tests for the Favicon and TouchIcon related APIs. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientFaviconTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String FAVICON1_URL = "/favicon1.png";
    private static final String FAVICON1_PAGE_URL = "/favicon1.html";
    private static final String FAVICON1_PAGE_HTML =
            CommonResources.makeHtmlPageFrom(
                    "<link rel=\"icon\" href=\"" + FAVICON1_URL + "\" />", "Body");

    private static final String TOUCHICON_REL_LINK = "touch.png";
    private static final String TOUCHICON_REL_LINK_72 = "touch_72.png";
    private static final String TOUCHICON_REL_URL = "/" + TOUCHICON_REL_LINK;
    private static final String TOUCHICON_REL_URL_72 = "/" + TOUCHICON_REL_LINK_72;
    private static final String TOUCHICON_REL_PAGE_HTML =
            CommonResources.makeHtmlPageFrom(
                    "<link rel=\"apple-touch-icon\" href=\""
                            + TOUCHICON_REL_URL
                            + "\" />"
                            + "<link rel=\"apple-touch-icon\" sizes=\"72x72\" href=\""
                            + TOUCHICON_REL_URL_72
                            + "\" />",
                    "Body");

    // Maximum number of milliseconds within which a request to web server is made.
    private static final long MAX_REQUEST_WAITING_LIMIT_MS = scaleTimeout(500);

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    public AwContentsClientFaviconTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        AwContents.setShouldDownloadFavicons();
        mContentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    @Test
    @SmallTest
    public void testReceiveBasicFavicon() throws Throwable {
        int callCount = mContentsClient.getFaviconHelper().getCallCount();

        final String faviconUrl =
                mWebServer.setResponseBase64(
                        FAVICON1_URL,
                        CommonResources.FAVICON_DATA_BASE64,
                        CommonResources.getImagePngHeaders(true));
        final String pageUrl =
                mWebServer.setResponse(
                        FAVICON1_PAGE_URL,
                        FAVICON1_PAGE_HTML,
                        CommonResources.getTextHtmlHeaders(true));

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        mContentsClient.getFaviconHelper().waitForCallback(callCount);
        Assert.assertEquals(1, mWebServer.getRequestCount(FAVICON1_URL));
        Object originalFaviconSource = new URL(faviconUrl).getContent();
        Bitmap originalFavicon = BitmapFactory.decodeStream((InputStream) originalFaviconSource);
        Assert.assertNotNull(originalFavicon);
        Assert.assertNotNull(mContentsClient.getFaviconHelper().getIcon());
        Assert.assertTrue(mContentsClient.getFaviconHelper().getIcon().sameAs(originalFavicon));

        // Make sure the request counter for favicon is incremented when the page is loaded again
        // successfully.
        mActivityTestRule.loadUrlAsync(mAwContents, pageUrl);
        mContentsClient.getFaviconHelper().waitForCallback(callCount);
        Assert.assertEquals(2, mWebServer.getRequestCount(FAVICON1_URL));
    }

    @Test
    @SmallTest
    public void testDoNotMakeRequestForFaviconAfter404() throws Throwable {
        mWebServer.setResponseWithNotFoundStatus(FAVICON1_URL);
        final String pageUrl =
                mWebServer.setResponse(
                        FAVICON1_PAGE_URL,
                        FAVICON1_PAGE_HTML,
                        CommonResources.getTextHtmlHeaders(true));

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        AwActivityTestRule.pollInstrumentationThread(
                () -> mWebServer.getRequestCount(FAVICON1_URL) == 1);

        // Make sure the request counter for favicon is not incremented, since we already got 404.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        // If a request hasn't been done within this time period, we assume it won't be done.
        Thread.sleep(MAX_REQUEST_WAITING_LIMIT_MS);
        Assert.assertEquals(1, mWebServer.getRequestCount(FAVICON1_URL));
    }

    @Test
    @SmallTest
    public void testReceiveBasicTouchIconLinkRel() throws Throwable {
        int callCount = mContentsClient.getFaviconHelper().getCallCount();

        final String pageUrl =
                mWebServer.setResponse(
                        TOUCHICON_REL_URL,
                        TOUCHICON_REL_PAGE_HTML,
                        CommonResources.getTextHtmlHeaders(true));

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);

        mContentsClient.getTouchIconHelper().waitForCallback(callCount, 2);
        Assert.assertEquals(2, mContentsClient.getTouchIconHelper().getTouchIconsCount());
        Assert.assertFalse(
                mContentsClient
                        .getTouchIconHelper()
                        .hasTouchIcon(mWebServer.getBaseUrl() + TOUCHICON_REL_LINK));
        Assert.assertFalse(
                mContentsClient
                        .getTouchIconHelper()
                        .hasTouchIcon(mWebServer.getBaseUrl() + TOUCHICON_REL_LINK_72));
    }
}
