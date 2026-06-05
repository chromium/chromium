// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwPage;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

/** Tests for AwNavigation APIs. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwNavigationTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private TestAwNavigationListener mNavigationListener;
    private CallbackHelper mCallbackHelper;
    private AwTestContainerView mTestContainerView;
    private TestWebServer mWebServer;

    public AwNavigationTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mCallbackHelper = new CallbackHelper();
        mNavigationListener = new TestAwNavigationListener(mCallbackHelper);
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mTestContainerView.getAwContents().getNavigationClient().addListener(mNavigationListener);
        mWebServer = TestWebServer.start();
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
    public void testBasicNavigationProperties() throws Throwable {
        final String url =
                mWebServer.setResponse("/page.html", "<html><body>Hello</body></html>", null);

        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);

        AwNavigation navigation = mNavigationListener.getLastCompletedNavigation();
        Assert.assertNotNull(navigation);
        Assert.assertEquals(url, navigation.getUrl());
        Assert.assertFalse(navigation.wasInitiatedByPage());
        Assert.assertFalse(navigation.isSameDocument());
        Assert.assertFalse(navigation.isReload());
        Assert.assertFalse(navigation.isHistory());
        Assert.assertFalse(navigation.isRestore());
        Assert.assertFalse(navigation.isBack());
        Assert.assertFalse(navigation.isForward());
        Assert.assertTrue(navigation.didCommit());
        Assert.assertFalse(navigation.didCommitErrorPage());
        Assert.assertEquals(200, navigation.getStatusCode());
        Assert.assertNull(navigation.getWebResourceError());
        Assert.assertNotNull(navigation.getPage());
        Assert.assertEquals(url, navigation.getPage().getUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRendererInitiatedNavigation() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mTestContainerView.getAwContents());

        final String page1Url =
                mWebServer.setResponse("/page1.html", "<html><body>Page 1</body></html>", null);
        final String page2Url =
                mWebServer.setResponse("/page2.html", "<html><body>Page 2</body></html>", null);

        // Load Page 1
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                page1Url);

        // Navigate to Page 2 via JS
        int currentCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(),
                mContentsClient,
                "location.href = '" + page2Url + "';");
        mContentsClient.getOnPageFinishedHelper().waitForCallback(currentCallCount);

        AwNavigation navigation = mNavigationListener.getLastCompletedNavigation();
        Assert.assertNotNull(navigation);
        Assert.assertEquals(page2Url, navigation.getUrl());
        Assert.assertTrue(navigation.wasInitiatedByPage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSameDocumentNavigation() throws Throwable {
        final String url =
                mWebServer.setResponse("/page.html", "<html><body>Hello</body></html>", null);
        final String fragmentUrl = url + "#fragment";

        // Load initial page
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
        AwPage initialPage = mNavigationListener.getLastCompletedNavigation().getPage();
        Assert.assertNotNull(initialPage);

        // Navigate to fragment
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                fragmentUrl);

        AwNavigation navigation = mNavigationListener.getLastCompletedNavigation();
        Assert.assertNotNull(navigation);
        Assert.assertEquals(fragmentUrl, navigation.getUrl());
        Assert.assertTrue(navigation.isSameDocument());
        Assert.assertTrue(navigation.didCommit());
        Assert.assertSame(initialPage, navigation.getPage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testReload() throws Throwable {
        final String url =
                mWebServer.setResponse("/page.html", "<html><body>Hello</body></html>", null);

        // Load initial page
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);

        // Reload
        mActivityTestRule.reloadSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper());

        AwNavigation navigation = mNavigationListener.getLastCompletedNavigation();
        Assert.assertNotNull(navigation);
        Assert.assertEquals(url, navigation.getUrl());
        Assert.assertTrue(navigation.isReload());
        Assert.assertFalse(navigation.isHistory());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @DisabledTest(message = "https://crbug.com/520371218")
    public void testHistoryNavigation() throws Throwable {
        final String url1 =
                mWebServer.setResponse("/page1.html", "<html><body>Page 1</body></html>", null);
        final String url2 =
                mWebServer.setResponse("/page2.html", "<html><body>Page 2</body></html>", null);

        // Load Page 1
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                url1);

        // Load Page 2
        mActivityTestRule.loadUrlSync(
                mTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                url2);

        // Go Back
        int currentCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mTestContainerView.getAwContents().goBack());
        mContentsClient.getOnPageFinishedHelper().waitForCallback(currentCallCount);

        AwNavigation backNavigation = mNavigationListener.getLastCompletedNavigation();
        Assert.assertNotNull(backNavigation);
        Assert.assertEquals(url1, backNavigation.getUrl());
        Assert.assertTrue(backNavigation.isHistory());
        Assert.assertTrue(backNavigation.isBack());
        Assert.assertFalse(backNavigation.isForward());

        // Go Forward
        currentCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> mTestContainerView.getAwContents().goForward());
        mContentsClient.getOnPageFinishedHelper().waitForCallback(currentCallCount);

        AwNavigation forwardNavigation = mNavigationListener.getLastCompletedNavigation();
        Assert.assertNotNull(forwardNavigation);
        Assert.assertEquals(url2, forwardNavigation.getUrl());
        Assert.assertTrue(forwardNavigation.isHistory());
        Assert.assertFalse(forwardNavigation.isBack());
        Assert.assertTrue(forwardNavigation.isForward());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testErrorPageNavigation() throws Throwable {
        final String badUrl = "http://fake.domain.test/a.html";

        // Load bad URL
        mActivityTestRule.loadUrlAsync(mTestContainerView.getAwContents(), badUrl);

        // Wait for navigation to complete using polling to avoid race conditions with
        // onPageFinished
        AwActivityTestRule.pollInstrumentationThread(
                () -> mNavigationListener.getLastCompletedNavigation() != null);

        AwNavigation navigation = mNavigationListener.getLastCompletedNavigation();
        Assert.assertNotNull(navigation);
        Assert.assertEquals(badUrl, navigation.getUrl());
        Assert.assertTrue(navigation.didCommit()); // Error pages do commit in Chromium
        Assert.assertTrue(navigation.didCommitErrorPage());
        Assert.assertNotNull(navigation.getWebResourceError());
    }
}
