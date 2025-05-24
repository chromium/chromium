// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

/** A test suite for ContentView.getTitle(). */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class GetTitleTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String TITLE = "TITLE";

    private static final String GET_TITLE_TEST_PATH = "/get_title_test.html";
    private static final String GET_TITLE_TEST_EMPTY_PATH = "/get_title_test_empty.html";
    private static final String GET_TITLE_TEST_NO_TITLE_PATH = "/get_title_test_no_title.html";

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private static class PageInfo {
        public final String mTitle;
        public final String mUrl;

        public PageInfo(String title, String url) {
            mTitle = title;
            mUrl = url;
        }
    }

    public GetTitleTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
    }

    private static final String getHtml(String title) {
        StringBuilder html = new StringBuilder();
        html.append("<html><head>");
        if (title != null) {
            html.append("<title>" + title + "</title>");
        }
        html.append("</head><body>BODY</body></html>");
        return html.toString();
    }

    private String loadFromDataAndGetTitle(String html) throws Throwable {
        mActivityTestRule.loadDataSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), html, "text/html", false);
        return mActivityTestRule.getTitleOnUiThread(mAwContents);
    }

    private PageInfo loadFromUrlAndGetTitle(String html, String filename) throws Throwable {
        TestWebServer webServer = TestWebServer.start();
        try {
            final String url = webServer.setResponse(filename, html, null);
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            return new PageInfo(
                    mActivityTestRule.getTitleOnUiThread(mAwContents),
                    url.replaceAll("http:\\/\\/", ""));
        } finally {
            webServer.shutdown();
        }
    }

    /** When the data has title info, the page title is set to it. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Main"})
    public void testLoadDataGetTitle() throws Throwable {
        final String title = loadFromDataAndGetTitle(getHtml(TITLE));
        Assert.assertEquals("Title should be " + TITLE, TITLE, title);
    }

    /** When the data has empty title, the page title is set to the loaded content. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetTitleOnDataContainingEmptyTitle() throws Throwable {
        final String content = getHtml("");
        final String expectedTitle = "data:text/html," + content;
        final String title = loadFromDataAndGetTitle(content);
        Assert.assertEquals(
                "Title should be set to the loaded data:text/html content", expectedTitle, title);
    }

    /** When the data has no title, the page title is set to the loaded content. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetTitleOnDataContainingNoTitle() throws Throwable {
        final String content = getHtml(null);
        final String expectedTitle = "data:text/html," + content;
        final String title = loadFromDataAndGetTitle(content);
        Assert.assertEquals(
                "Title should be set to the data:text/html content", expectedTitle, title);
    }

    /** When url-file has the title info, the page title is set to it. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLoadUrlGetTitle() throws Throwable {
        final PageInfo info = loadFromUrlAndGetTitle(getHtml(TITLE), GET_TITLE_TEST_PATH);
        Assert.assertEquals("Title should be " + TITLE, TITLE, info.mTitle);
    }

    /**
     * When the loaded file has empty title, the page title is set to the url it loads from. It also
     * contains: hostName, portNumber information if it's part of the loaded URL.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetTitleOnLoadUrlFileContainingEmptyTitle() throws Throwable {
        final PageInfo info = loadFromUrlAndGetTitle(getHtml(""), GET_TITLE_TEST_EMPTY_PATH);
        Assert.assertEquals("Incorrect title :: ", info.mUrl, info.mTitle);
    }

    /**
     * When the loaded file has no title, the page title is set to the urk it loads from. It also
     * contains: hostName, portNumber information if it's part of the loaded URL.
     */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetTitleOnLoadUrlFileContainingNoTitle() throws Throwable {
        final PageInfo info = loadFromUrlAndGetTitle(getHtml(null), GET_TITLE_TEST_NO_TITLE_PATH);
        Assert.assertEquals("Incorrect title :: ", info.mUrl, info.mTitle);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetTitleSetFromJS() throws Throwable {
        final String expectedTitle = "Expected";
        final String page =
                "<html><head>"
                        + "<script>document.title=\""
                        + expectedTitle
                        + "\"</script>"
                        + "</head><body>"
                        + "</body></html>";
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        final String title = loadFromDataAndGetTitle(page);
        Assert.assertEquals("Incorrect title :: ", expectedTitle, title);
    }
}
