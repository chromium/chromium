// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertNotEquals;

import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.webkit.WebSettings;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwConsoleMessage;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;

/**
 * Verify that content loading blocks initiated by renderer can be detected
 * by the embedder via WebChromeClient.onConsoleMessage.
 */
@RunWith(AwJUnit4ClassRunner.class)
@CommandLineFlags.Add(ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1")
public class ConsoleMessagesForBlockedLoadsTest {
    public static final String SERVER_HOSTNAME = "example.test";

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private TestAwContentsClient.AddMessageToConsoleHelper mOnConsoleMessageHelper;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        mOnConsoleMessageHelper = mContentsClient.getAddMessageToConsoleHelper();
    }

    @After
    public void tearDown() {
        if (mWebServer != null) mWebServer.shutdown();
    }

    private void startWebServer() throws Exception {
        mWebServer = TestWebServer.start();
        mWebServer.setServerHost(SERVER_HOSTNAME);
    }

    private AwConsoleMessage getSingleErrorMessage() {
        AwConsoleMessage result = null;
        for (AwConsoleMessage m : mOnConsoleMessageHelper.getMessages()) {
            if (m.messageLevel() == AwConsoleMessage.MESSAGE_LEVEL_ERROR) {
                Assert.assertNull(result);
                result = m;
            }
        }
        Assert.assertNotNull(result);
        return result;
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testXFrameOptionsDenial() throws Throwable {
        startWebServer();
        final String iframeHtml = CommonResources.makeHtmlPageFrom("", "FAIL");
        List<Pair<String, String>> iframeHeaders = new ArrayList<Pair<String, String>>();
        iframeHeaders.add(Pair.create("x-frame-options", "DENY"));
        final String iframeUrl = mWebServer.setResponse("/iframe.html", iframeHtml, iframeHeaders);
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + iframeUrl + "' />");
        final String pageUrl = mWebServer.setResponse("/page.html", pageHtml, null);
        mOnConsoleMessageHelper.clearMessages();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        AwConsoleMessage errorMessage = getSingleErrorMessage();
        assertNotEquals(errorMessage.message().indexOf(iframeUrl), -1);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testMixedContentDenial() throws Throwable {
        startWebServer();
        TestWebServer httpsServer = null;
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        try {
            httpsServer = TestWebServer.startSsl();
            final String imageUrl = mWebServer.setResponseBase64(
                    "/insecure.png", CommonResources.FAVICON_DATA_BASE64, null);
            final String secureHtml = CommonResources.makeHtmlPageFrom(
                    "", "<img src='" + imageUrl + "' />");
            String secureUrl = httpsServer.setResponse("/secure.html", secureHtml, null);
            mOnConsoleMessageHelper.clearMessages();
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), secureUrl);
            AwConsoleMessage errorMessage = getSingleErrorMessage();
            assertNotEquals(errorMessage.message().indexOf(imageUrl), -1);
            assertNotEquals(errorMessage.message().indexOf(secureUrl), -1);
        } finally {
            if (httpsServer != null) {
                httpsServer.shutdown();
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCrossOriginDenial() throws Throwable {
        startWebServer();
        final String iframeXsl =
                "<?xml version='1.0' encoding='UTF-8'?>"
                + "<xsl:stylesheet version='1.0' xmlns:xsl='http://www.w3.org/1999/XSL/Transform'>"
                + "<xsl:template match='*'>"
                + "<html><body>FAIL</body></html>"
                + "</xsl:template>"
                + "</xsl:stylesheet>";
        final String iframeXslUrl = mWebServer.setResponse(
                "/iframe.xsl", iframeXsl, null).replace(SERVER_HOSTNAME, "127.0.0.1");
        final String iframeXml =
                "<?xml version='1.0' encoding='UTF-8'?>"
                + "<?xml-stylesheet type='text/xsl' href='" + iframeXslUrl + "'?>"
                + "<html xmlns='http://www.w3.org/1999/xhtml'>"
                + "<body>PASS</body></html>";
        final String iframeXmlUrl = mWebServer.setResponse("/iframe.xml", iframeXml, null);
        final String pageHtml = CommonResources.makeHtmlPageFrom(
                "", "<iframe src='" + iframeXmlUrl + "' />");
        final String pageUrl = mWebServer.setResponse("/page.html", pageHtml, null);
        mOnConsoleMessageHelper.clearMessages();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageUrl);
        AwConsoleMessage errorMessage = getSingleErrorMessage();
        assertNotEquals(errorMessage.message().indexOf(iframeXslUrl), -1);
        assertNotEquals(errorMessage.message().indexOf(iframeXmlUrl), -1);
    }
}
