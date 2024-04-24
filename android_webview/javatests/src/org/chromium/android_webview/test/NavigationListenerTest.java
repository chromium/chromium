// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Bundle;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedTitleHelper;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.WebResourceResponseInfo;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.net.test.util.TestWebServer;

import java.io.ByteArrayInputStream;
import java.net.URLEncoder;

/** Test suite for the special navigation listener that will be notified of navigation messages */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class NavigationListenerTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    @ClassRule public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    private static final String RESOURCE_PATH = "/android_webview/test/data";
    private static final String PAGE_A = RESOURCE_PATH + "/hello_world.html";
    private static final String PAGE_B = RESOURCE_PATH + "/safe.html";
    private static final String PAGE_WITH_IFRAME = RESOURCE_PATH + "/iframe_access.html";
    private static final String NAVIGATION_LISTENER_ALLOW_BFCACHE_NAME =
            "experimentalWebViewNavigationListenerAllowBFCache";
    private static final String NAVIGATION_LISTENER_DISABLE_BFCACHE_NAME =
            "experimentalWebViewNavigationListenerDisableBFCache";
    private static final String ENCODING = "UTF-8";

    private EmbeddedTestServer mTestServer;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebMessageListener mListener;

    public NavigationListenerTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        mListener = new TestWebMessageListener();
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setJavaScriptEnabled(true);
        mTestServer = sTestServerRule.getServer();
    }

    // Test that adding the special navigationListener will result in receiving
    // navigation messages for a variety of navigation cases:
    // 1) Regular navigation
    // 2) Reload
    // 3) Same-document navigation
    // 4) Same-document history navigation
    // 5) Failed navigation resulting in an error page.
    // TODO: Add tests for SSL error?
    @Test
    @LargeTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigationVariousCases() throws Throwable {
        // Add the special listener object which will receive navigation messages.
        addWebMessageListenerOnUiThread(
                mAwContents, NAVIGATION_LISTENER_ALLOW_BFCACHE_NAME, new String[] {"*"}, mListener);
        // The first message received should be the NAVIGATION_MESSAGE_OPTED_IN message.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "NAVIGATION_MESSAGE_OPTED_IN");
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);
        // The JavaScriptReplyProxy should be 1:1 with Page. Save the current proxy (which is the
        // for the initial empty document), so that we can check if the proxy changes after
        // navigations.
        JsReplyProxy page1ReplyProxy = data.mReplyProxy;

        // No actual navigationListener object is created on the JS side.
        Assert.assertFalse(
                hasJavaScriptObject(
                        NAVIGATION_LISTENER_ALLOW_BFCACHE_NAME,
                        mActivityTestRule,
                        mAwContents,
                        mContentsClient));

        // Navigation #1: Navigate to `url` to trigger navigation messages.
        final String url = loadUrlFromPath(PAGE_A);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page2ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page1ReplyProxy, page2ReplyProxy);

        // After the new page finishes loading, we get a PAGE_LOAD_END message.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // Navigation #2: Do a same-document navigation to `url2`.
        final String url2 = loadUrlFromPath(PAGE_A + "#foo");
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url2,
                /* isSameDocument */ true,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        // Since the Page stays the same, the JavaScriptReplyProxy also stays
        // the same.
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // No PAGE_LOAD_END for same-document navigations.

        // Navigation #3: Do a renderer-initiated reload.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "location.reload()");
        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // The notification should indicate that the navigation is a reload, and
        // uses a new JavaScriptReplyProxy.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url2,
                /* isSameDocument */ false,
                /* isReload */ true,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ true,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page3ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page3ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        // Navigation #4: Do a same-document history navigation.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "history.go(-1)");
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ true,
                /* isReload */ false,
                /* isHistory */ true,
                /* isErrorPage */ false,
                /* isPageInitiated */ true,
                /* committed */ true,
                /* statusCode */ 200);
        // The JsReplyProxy is reused after a same-document navigation.
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        // No PAGE_LOAD_END for same-document navigations.

        // Navigation #5: Do a navigation to a non-existent page, resulting in a 404 error.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "location.href = '404.html'; ");

        // Since this navigation creates a new Page (an error page), the
        // previous Page gets deleted.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                mAwContents.getUrl().getSpec(),
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ true,
                /* isPageInitiated */ true,
                /* committed */ true,
                /* statusCode */ 404);

        // A new JavaScriptReplyProxy should be used for the new Page.
        JsReplyProxy page4ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page4ReplyProxy);
        Assert.assertNotEquals(page3ReplyProxy, page4ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page4ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test navigation messages when navigation to a URL that results in 204 No Content.
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigation204() throws Throwable {
        // Add the special listener object which will receive navigation messages, and get the
        // JsReplyProxy for the initial empty document.
        JsReplyProxy page1ReplyProxy = setUpAndGetInitialProxy();

        TestWebServer webServer = TestWebServer.start();
        try {
            // Navigate to a URL that results in 204 No Content response. This navigation won't
            // commit.
            final String url204 = webServer.setResponseWithNoContentStatus("/page204.html");
            mActivityTestRule.loadUrlAsync(mAwContents, url204);

            // The navigation didn't commit but still dispatched a navigation message, which will
            // reuse the initial empty document's JsReplyProxy (since this navigation didn't create
            // a new page).
            TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
            assertNavigationData(
                    data,
                    url204,
                    /* isSameDocument */ false,
                    /* isReload */ false,
                    /* isHistory */ false,
                    /* isErrorPage */ false,
                    /* isPageInitiated */ false,
                    /* committed */ false,
                    /* statusCode */ 204);
            Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

            // No more messages as the navigation didn't commit.
            Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
        } finally {
            webServer.shutdown();
        }
    }

    // Test navigation messages when navigating to a page with an iframe.
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigationPageWithIframe() throws Throwable {
        // Add the special listener object which will receive navigation messages, and get the
        // JsReplyProxy for the initial empty document.
        JsReplyProxy page1ReplyProxy = setUpAndGetInitialProxy();

        // Navigate to a page that has an iframe. When the iframe is loaded, the title of the main
        // document will be set to the iframe's URL.
        final OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        final int titleCallCount = onReceivedTitleHelper.getCallCount();
        final String pageWithIframeURL = mTestServer.getURL(PAGE_WITH_IFRAME);
        final String iframeURL = mTestServer.getURL(PAGE_A);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithIframeURL);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                pageWithIframeURL,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page2ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page1ReplyProxy, page2ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // Check that the main document's title has been updated to the iframe's URL, indicating
        // that the iframe had finished loading.
        onReceivedTitleHelper.waitForCallback(titleCallCount);
        Assert.assertEquals(iframeURL, onReceivedTitleHelper.getTitle());

        // Navigation #2: Navigate to `url2`, to ensure that we don't receive any navigation message
        // for the iframe loaded by the previous page..
        final String url2 = loadUrlFromPath(PAGE_B);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // The notification should indicate that the navigation is
        // cross-document and uses a new JavaScriptReplyProxy.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url2,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page3ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page3ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test navigation messages when navigating to a URL that redirects.
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigationRedirect() throws Throwable {
        // Add the special listener object which will receive navigation messages, and get the
        // JsReplyProxy for the initial empty document.
        JsReplyProxy page1ReplyProxy = setUpAndGetInitialProxy();

        // Navigate to `redirectingURL`, which redirects to `redirectTargetURL`.
        final String redirectTargetURL = mTestServer.getURL(PAGE_A);
        final String redirectingURL =
                mTestServer.getURL(
                        "/server-redirect?" + URLEncoder.encode(redirectTargetURL, ENCODING));
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), redirectingURL);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                redirectTargetURL,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page2ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page1ReplyProxy, page2ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test navigation messages when navigating to about:blank.
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigationAboutBlank() throws Throwable {
        // Add the special listener object which will receive navigation messages, and get the
        // JsReplyProxy for the initial empty document.
        JsReplyProxy page1ReplyProxy = setUpAndGetInitialProxy();

        // Navigate to about:blank.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                "about:blank",
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page2ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page1ReplyProxy, page2ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // Navigate same-document to about:blank#foo.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "location.href = 'about:blank#foo';");

        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                "about:blank#foo",
                /* isSameDocument */ true,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ true,
                /* committed */ true,
                /* statusCode */ 200);
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test navigation messages when navigating with restoreState().
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigationRestoreState() throws Throwable {
        // Navigation #1: Set up the listener and navigate to `url`. This will create a new page and
        // an associated JsReplyProxy.
        final String url = mTestServer.getURL(PAGE_A);
        JsReplyProxy page2ReplyProxy =
                setUpAndNavigateToNewPage(url, /* listenerDisablesBFCache= */ false);

        // Navigation #2: Save and restore to a new AwContents, which should trigger a load to
        // `url` again.
        TestAwContentsClient newContentsClient = new TestAwContentsClient();
        AwTestContainerView newView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(newContentsClient);
        AwContents newContents = newView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(newContents);
        TestWebMessageListener newListener = new TestWebMessageListener();
        addWebMessageListenerOnUiThread(
                newContents,
                NAVIGATION_LISTENER_ALLOW_BFCACHE_NAME,
                new String[] {"*"},
                newListener);

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            Bundle bundle = new Bundle();
                            boolean result = mAwContents.saveState(bundle);
                            Assert.assertTrue(result);
                            result = newContents.restoreState(bundle);
                            Assert.assertTrue(result);
                        });
        // Since the navigation uses a new AwContents, we get a new opt-in message.
        TestWebMessageListener.Data data = newListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "NAVIGATION_MESSAGE_OPTED_IN");
        JsReplyProxy page3ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page3ReplyProxy);

        // The restore navigation should trigger navigation messages, and page deletion notification
        // for the initial empty document of the new AwContents.
        data = newListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        data = newListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ true,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page4ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page3ReplyProxy, page4ReplyProxy);

        data = newListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page4ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(newListener.hasNoMoreOnPostMessage());
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test navigation messages when navigating with loadDataWithBaseURL().
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigationLoadDataWithBaseURL() throws Throwable {
        // Add the special listener object which will receive navigation messages, and get the
        // JsReplyProxy for the initial empty document.
        JsReplyProxy page1ReplyProxy = setUpAndGetInitialProxy();

        // Navigate with loadDataWithBaseURL.
        final String html = "<html><body>foo</body></html>";
        final String baseUrl = "http://www.google.com";
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                false,
                baseUrl,
                null);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

        // Assert that the navigation messages correspond to this navigation.

        // Note that the URL in the navigation message will only contain the data: URL prefix
        // instead of the whole `html` content.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                "data:text/html;charset=utf-8;base64,",
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page2ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page1ReplyProxy, page2ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test navigation messages for navigations that get intercepted.
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigationIntercepted() throws Throwable {
        // Add the special listener object which will receive navigation messages, and get the
        // JsReplyProxy for the initial empty document.
        JsReplyProxy page1ReplyProxy = setUpAndGetInitialProxy();

        TestAwContentsClient.ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                mContentsClient.getShouldInterceptRequestHelper();
        shouldInterceptRequestHelper.setReturnValue(
                new WebResourceResponseInfo(
                        "text/html",
                        ENCODING,
                        new ByteArrayInputStream("foo".getBytes(ENCODING)),
                        200,
                        "OK",
                        /* responseHeaders= */ null));

        // Navigation #1: Navigate to `url` which will be intercepted to contain "foo".
        final String url = loadUrlFromPath(PAGE_A);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

        // The navigation message for the navigation to `url` should be received
        // after the opt-in message.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        JsReplyProxy page2ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page1ReplyProxy, page2ReplyProxy);

        // Navigation #2: Navigate to `url2`, which will be intercepted to result in an error page.
        shouldInterceptRequestHelper.setReturnValue(
                new WebResourceResponseInfo(
                        "text/html",
                        ENCODING,
                        new ByteArrayInputStream("".getBytes(ENCODING)),
                        500,
                        "Internal Server Error",
                        /* responseHeaders= */ null));
        final String url2 = loadUrlFromPath(PAGE_B);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // The notification should indicate that the navigation is
        // cross-document and uses a new JavaScriptReplyProxy.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url2,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ true,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 500);
        JsReplyProxy page3ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page3ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test the navigation messages for navigations that get overridden.
    @Test
    @MediumTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener"})
    public void testNavigationOverridden() throws Throwable {
        // Add the special listener object which will receive navigation messages, and get the
        // JsReplyProxy for the initial empty document.
        JsReplyProxy page1ReplyProxy = setUpAndGetInitialProxy();

        final TestAwContentsClient.ShouldOverrideUrlLoadingHelper shouldOverrideUrlLoadingHelper =
                mContentsClient.getShouldOverrideUrlLoadingHelper();
        try {
            // Set up the helper to override navigations to `url2` (and only `url2`).
            final String url2 = mTestServer.getURL(PAGE_B);
            shouldOverrideUrlLoadingHelper.setUrlToOverride(url2);
            shouldOverrideUrlLoadingHelper.setShouldOverrideUrlLoadingReturnValue(true);

            // Navigation #2: Trigger a renderer-initiated navigation to `url2`, which should get
            // overridden.
            int currentCallCount = shouldOverrideUrlLoadingHelper.getCallCount();
            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, "window.location.href = '" + url2 + "';");
            shouldOverrideUrlLoadingHelper.waitForCallback(currentCallCount);
            Assert.assertEquals(
                    shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(), url2);

            // No navigation messages will be received as the navigations above got overridden.
            Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

            // Navigation #3: Navigate to `url3` which is not initially overridden but will
            // redirect to `url2`, at which point it will get overridden.
            final String url3 =
                    mTestServer.getURL("/server-redirect?" + URLEncoder.encode(url2, ENCODING));
            currentCallCount += 1;
            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, "window.location.href = '" + url3 + "';");
            shouldOverrideUrlLoadingHelper.waitForCallback(currentCallCount, 2);
            Assert.assertEquals(
                    shouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl(), url2);

            // Different from the previous navigation, we'll get a NAVIGATION_COMPLETED message
            // indicating that this navigation didn't commit and was redirected to `url2`. This is
            // because this navigation got overridden during redirect (after the navigation
            // starts) instead of at the very beginning before the navigation officially starts.
            TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
            assertNavigationData(
                    data,
                    url2,
                    /* isSameDocument */ false,
                    /* isReload */ false,
                    /* isHistory */ false,
                    /* isErrorPage */ false,
                    /* isPageInitiated */ true,
                    /* committed */ false,
                    /* statusCode */ 301);
            Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

            Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        } finally {
            shouldOverrideUrlLoadingHelper.setShouldOverrideUrlLoadingReturnValue(false);
            shouldOverrideUrlLoadingHelper.setUrlToOverride(null);
        }
    }

    // Test navigation messages on history navigations with BFCache disabled.
    @Test
    @LargeTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({
        "enable-features=EnableNavigationListener",
        "disable-features=WebViewBackForwardCache"
    })
    public void testNavigationHistoryNavigationBFCacheDisabled() throws Throwable {
        // Navigation #1: Set up the listener and navigate to `url`. This will create a new page and
        // an associated JsReplyProxy.
        final String url = mTestServer.getURL(PAGE_A);
        JsReplyProxy page2ReplyProxy =
                setUpAndNavigateToNewPage(url, /* listenerDisablesBFCache= */ false);

        // Navigation #2: Navigate to `url2`.
        final String url2 = loadUrlFromPath(PAGE_B);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // The notification should indicate that the navigation is
        // cross-document and uses a new JavaScriptReplyProxy.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url2,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page3ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page3ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        // Navigation #3: Do a back navigation to the `url` Page.
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents.getWebContents(),
                onPageStartedHelper);

        // Since this navigation creates a new Page, the `url2` Page gets
        // deleted.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        // The history navigation creates a new Page and uses a new
        // JsReplyProxy.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ true,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page4ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page4ReplyProxy);
        Assert.assertNotEquals(page3ReplyProxy, page4ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page4ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test the navigation messages on history navigations with BFCache enabled.
    @Test
    @LargeTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener,WebViewBackForwardCache"})
    public void testNavigationHistoryNavigationBFCacheEnabled() throws Throwable {
        // Navigation #1: Set up the listener and navigate to `url`. This will create a new page and
        // an associated JsReplyProxy.
        final String url = mTestServer.getURL(PAGE_A);
        JsReplyProxy page2ReplyProxy =
                setUpAndNavigateToNewPage(url, /* listenerDisablesBFCache= */ false);

        // Navigation #2: Navigate to `url2`.
        final String url2 = loadUrlFromPath(PAGE_B);

        // The notification should indicate that the navigation is
        // cross-document and uses a new JavaScriptReplyProxy. Since the
        // previous page gets BFCached, we don't get a PAGE_DELETED for the
        // previous page.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url2,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page3ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page3ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        // Navigation #3: Do a back navigation to the `url` Page.
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents.getWebContents(),
                onPageStartedHelper);

        // The history navigation restores the `url` page from BFCache, so it
        // reuses the `page2ReplyProxy`.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ true,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // No PAGE_LOAD_END for BFCache restores, as the page content didn't get
        // re-loaded.

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test navigation messages on history navigations with BFCache enabled, but with the
    // listener that disables BFCache. No page will be BFCached, because of the listener.
    @Test
    @LargeTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener,WebViewBackForwardCache"})
    public void testNavigationHistoryNavigationBFCacheEnabled_ListenerDisablesBFCache()
            throws Throwable {
        // Navigation #1: Set up the listener and navigate to `url`. This will create a new page and
        // an associated JsReplyProxy.
        final String url = mTestServer.getURL(PAGE_A);
        JsReplyProxy page2ReplyProxy =
                setUpAndNavigateToNewPage(url, /* listenerDisablesBFCache= */ true);

        // Navigation #2: Navigate to `url2`.
        final String url2 = loadUrlFromPath(PAGE_B);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // The notification should indicate that the navigation is
        // cross-document and uses a new JavaScriptReplyProxy.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url2,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page3ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page3ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        // Navigation #3: Do a back navigation to the `url` Page.
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents.getWebContents(),
                onPageStartedHelper);

        // Since this navigation creates a new Page, the `url2` Page gets
        // deleted.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        // The history navigation creates a new Page and uses a new
        // JsReplyProxy.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ true,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page4ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page4ReplyProxy);
        Assert.assertNotEquals(page3ReplyProxy, page4ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page4ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    // Test the navigation messages on history navigations with BFCache enabled, but the page was
    // evicted from BFCache.
    @Test
    @LargeTest
    @Feature({"AndroidWebView", "NavigationListener"})
    @CommandLineFlags.Add({"enable-features=EnableNavigationListener,WebViewBackForwardCache"})
    public void testNavigationHistoryNavigationToEvictedPageBFCacheEnabled() throws Throwable {
        // Navigation #1: Set up the listener and navigate to `url`. This will create a new page and
        // an associated JsReplyProxy.
        final String url = mTestServer.getURL(PAGE_A);
        JsReplyProxy page2ReplyProxy =
                setUpAndNavigateToNewPage(url, /* listenerDisablesBFCache= */ false);

        // Navigation #2: Navigate to `url2`.
        final String url2 = loadUrlFromPath(PAGE_B);

        // The notification should indicate that the navigation is
        // cross-document and uses a new JavaScriptReplyProxy. Since the
        // previous page gets BFCached, we don't get a PAGE_DELETED for the
        // previous page.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url2,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page3ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page3ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page3ReplyProxy, data.mReplyProxy);

        // Add another WebMessageListener, which will evict all BFCached pages.
        addWebMessageListenerOnUiThread(
                mAwContents, "foo", new String[] {"*"}, new TestWebMessageListener());

        // The `url` Page gets deleted as it's evicted from BFCache.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);

        // Navigation #3: Do a back navigation to the `url` Page.
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents.getWebContents(),
                onPageStartedHelper);

        // No PAGE_DELETED for `url2`, as that page is BFCached.

        // Since the `url` page is already evicted, the history navigation didn't do a BFCache
        // restore, and instead creates a new Page and uses a new JsReplyProxy.
        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ true,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page4ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page2ReplyProxy, page4ReplyProxy);
        Assert.assertNotEquals(page3ReplyProxy, page4ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page4ReplyProxy, data.mReplyProxy);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    private String loadUrlFromPath(String path) throws Exception {
        final String url = mTestServer.getURL(path);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        return url;
    }

    private static void addWebMessageListenerOnUiThread(
            final AwContents awContents,
            final String jsObjectName,
            final String[] allowedOriginRules,
            final WebMessageListener listener)
            throws Exception {
        TestWebMessageListener.addWebMessageListenerOnUiThread(
                awContents, jsObjectName, allowedOriginRules, listener);
    }

    private static boolean hasJavaScriptObject(
            final String jsObjectName,
            final AwActivityTestRule rule,
            final AwContents awContents,
            final TestAwContentsClient contentsClient)
            throws Throwable {
        final String result =
                rule.executeJavaScriptAndWaitForResult(
                        awContents, contentsClient, "typeof " + jsObjectName + " !== 'undefined'");
        return result.equals("true");
    }

    private void assertNavigationMessageType(TestWebMessageListener.Data data, String type)
            throws Throwable {
        Assert.assertEquals(type, new JSONObject(data.getAsString()).getString("type"));
    }

    private void assertNavigationData(
            TestWebMessageListener.Data data,
            String url,
            boolean isSameDocument,
            boolean isReload,
            boolean isHistory,
            boolean isErrorPage,
            boolean isPageInitiated,
            boolean committed,
            int statusCode)
            throws Throwable {
        var dataObj = new JSONObject(data.getAsString());
        Assert.assertEquals("NAVIGATION_COMPLETED", dataObj.getString("type"));
        Assert.assertEquals(url, dataObj.getString("url"));
        Assert.assertEquals(isSameDocument, dataObj.getBoolean("isSameDocument"));
        Assert.assertEquals(isReload, dataObj.getBoolean("isReload"));
        Assert.assertEquals(isHistory, dataObj.getBoolean("isHistory"));
        Assert.assertEquals(isErrorPage, dataObj.getBoolean("isErrorPage"));
        Assert.assertEquals(isPageInitiated, dataObj.getBoolean("isPageInitiated"));
        Assert.assertEquals(committed, dataObj.getBoolean("committed"));
        Assert.assertEquals(statusCode, dataObj.getInt("statusCode"));
    }

    private JsReplyProxy setUpAndGetInitialProxy() throws Throwable {
        // Add the special listener object which will receive navigation messages.
        addWebMessageListenerOnUiThread(
                mAwContents, NAVIGATION_LISTENER_ALLOW_BFCACHE_NAME, new String[] {"*"}, mListener);
        // The first message received should be the NAVIGATION_MESSAGE_OPTED_IN message. This will
        // fire with the JSReplyProxy associated with the initial empty document, which we should
        // return.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "NAVIGATION_MESSAGE_OPTED_IN");
        return data.mReplyProxy;
    }

    private JsReplyProxy setUpAndNavigateToNewPage(String url, boolean listenerDisablesBFCache)
            throws Throwable {
        // Add the special listener object which will receive navigation messages.
        addWebMessageListenerOnUiThread(
                mAwContents,
                listenerDisablesBFCache
                        ? NAVIGATION_LISTENER_DISABLE_BFCACHE_NAME
                        : NAVIGATION_LISTENER_ALLOW_BFCACHE_NAME,
                new String[] {"*"},
                mListener);
        // The first message received should be the NAVIGATION_MESSAGE_OPTED_IN message. This will
        // fire with the JSReplyProxy associated with the initial empty document, which we should
        // return.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "NAVIGATION_MESSAGE_OPTED_IN");
        JsReplyProxy page1ReplyProxy = data.mReplyProxy;

        // Navigate to `url`.
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);

        // Since this navigation creates a new Page, the previous Page gets
        // deleted.
        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_DELETED");
        Assert.assertEquals(page1ReplyProxy, data.mReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationData(
                data,
                url,
                /* isSameDocument */ false,
                /* isReload */ false,
                /* isHistory */ false,
                /* isErrorPage */ false,
                /* isPageInitiated */ false,
                /* committed */ true,
                /* statusCode */ 200);
        JsReplyProxy page2ReplyProxy = data.mReplyProxy;
        Assert.assertNotEquals(page1ReplyProxy, page2ReplyProxy);

        data = mListener.waitForOnPostMessage();
        assertNavigationMessageType(data, "PAGE_LOAD_END");
        Assert.assertEquals(page2ReplyProxy, data.mReplyProxy);
        return page2ReplyProxy;
    }
}
