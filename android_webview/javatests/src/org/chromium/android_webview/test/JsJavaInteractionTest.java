// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.net.Uri;
import android.webkit.JavascriptInterface;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

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
import org.chromium.android_webview.ScriptHandler;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedTitleHelper;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.net.test.util.TestWebServer;

import java.nio.charset.StandardCharsets;
import java.util.Random;
import java.util.concurrent.LinkedBlockingQueue;

/** Test suite for JavaScript Java interaction. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class JsJavaInteractionTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    @ClassRule public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    private static final String RESOURCE_PATH = "/android_webview/test/data";
    private static final String POST_MESSAGE_SIMPLE_HTML =
            RESOURCE_PATH + "/post_message_simple.html";
    private static final String POST_MESSAGE_WITH_PORTS_HTML =
            RESOURCE_PATH + "/post_message_with_ports.html";
    private static final String POST_MESSAGE_REPEAT_HTML =
            RESOURCE_PATH + "/post_message_repeat.html";
    private static final String POST_MESSAGE_NULL_OR_UNDEFINED_HTML =
            RESOURCE_PATH + "/post_message_null_or_undefined.html";
    private static final String POST_MESSAGE_REPLY_HTML =
            RESOURCE_PATH + "/post_message_receives_reply.html";
    private static final String POST_MESSAGE_ARRAYBUFFER_REPLY_HTML =
            RESOURCE_PATH + "/post_message_array_buffer_reply.html";
    private static final String POST_MESSAGE_ARRAYBUFFER_TITLE_HTML =
            RESOURCE_PATH + "/post_message_array_buffer_title.html";
    private static final String FILE_URI = "file:///android_asset/asset_file.html";
    private static final String HELLO_WORLD_HTML = RESOURCE_PATH + "/hello_world.html";

    private static final String HELLO = "Hello";
    private static final String NEW_TITLE = "new_title";
    private static final String JS_OBJECT_NAME = "myObject";
    private static final String JS_OBJECT_NAME_2 = "myObject2";
    private static final String DATA_HTML = "<html><body>data</body></html>";
    private static final int MESSAGE_COUNT = 10000;

    private EmbeddedTestServer mTestServer;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestWebMessageListener mListener;

    public JsJavaInteractionTest(AwSettingsMutation param) {
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

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageSimple() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String url = loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mTopLevelOrigin);
        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessage_LoadData_MessageHasStringNullOrigin() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String html =
                "<html><head><script>myObject.postMessage('Hello');</script></head>"
                        + "<body></body></html>";

        // This uses loadDataAsync() which is equivalent to WebView#loadData(...).
        mActivityTestRule.loadHtmlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), html);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        // Note that the source origin is a non-null string of n, u, l, l.
        Assert.assertNotNull(data.mSourceOrigin);
        Assert.assertEquals("null", data.mSourceOrigin.toString());

        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageWithPorts() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String url = loadUrlFromPath(POST_MESSAGE_WITH_PORTS_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        final MessagePort[] ports = data.mPorts;
        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertEquals(1, ports.length);

        // JavaScript code in the page will change the title to NEW_TITLE if postMessage on
        // this port succeed.
        final OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        final int titleCallCount = onReceivedTitleHelper.getCallCount();
        ports[0].postMessage(new MessagePayload(NEW_TITLE), new MessagePort[0]);
        onReceivedTitleHelper.waitForCallback(titleCallCount);

        Assert.assertEquals(NEW_TITLE, onReceivedTitleHelper.getTitle());

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageRepeated() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String url = loadUrlFromPath(POST_MESSAGE_REPEAT_HTML);
        for (int i = 0; i < MESSAGE_COUNT; ++i) {
            TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
            assertUrlHasOrigin(url, data.mSourceOrigin);
            Assert.assertEquals(HELLO + ":" + i, data.getAsString());
        }

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessageFromIframeWorks() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String frameUrl = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        final String html = createCrossOriginAccessTestPageHtml(frameUrl);

        final String baseUrl = "http://www.google.com";
        // Load a cross origin iframe page.
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                false,
                baseUrl,
                null);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(baseUrl, data.mTopLevelOrigin);
        assertUrlHasOrigin(frameUrl, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertFalse(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAddWebMessageListenerAfterPageLoadWontAffectCurrentPage() throws Throwable {
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        // Add WebMessageListener after the page loaded won't affect the current page.
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Check that we don't have a JavaScript object named JS_OBJECT_NAME
        Assert.assertFalse(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // We shouldn't have executed postMessage on JS_OBJECT_NAME either.
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAddTheSameWebMessageListenerForDifferentJsObjectsWorks() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, new String[] {"*"}, mListener);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.getAsString());

        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, JS_OBJECT_NAME_2 + ".postMessage('" + HELLO + "');");

        TestWebMessageListener.Data data2 = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data2.getAsString());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testFragmentNavigationWontDoJsInjection() throws Throwable {
        String url = loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        // Add WebMessageListener after the page loaded won't affect the current page.
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Load with fragment url.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), url + "#fragment");

        // Check that we don't have a JavaScript object named JS_OBJECT_NAME
        Assert.assertFalse(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // We shouldn't have executed postMessage on JS_OBJECT_NAME either.
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAddWebMessageListenerAffectsRendererInitiatedNavigation() throws Throwable {
        // TODO(crbug.com/40630430): We'd either replace the following html file with a file
        // contains
        // no JavaScript code or add a test to ensure that evaluateJavascript() won't
        // over-trigger DidClearWindowObject.
        loadUrlFromPath(POST_MESSAGE_WITH_PORTS_HTML);

        // Add WebMessageListener after the page loaded.
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Check that we don't have a JavaScript object named JS_OBJECT_NAME
        Assert.assertFalse(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Navigate to a different web page from renderer and wait until the page loading finished.
        final String url = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        final OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        final int currentCallCount = onPageFinishedHelper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.evaluateJavaScriptForTests(
                                "window.location.href = '" + url + "';", null));
        onPageFinishedHelper.waitForCallback(currentCallCount);

        // We should expect one onPostMessage event.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAddWebMessageListenerWontAffectOtherAwContents() throws Throwable {
        // Create another AwContents object.
        final TestAwContentsClient awContentsClient = new TestAwContentsClient();
        final AwTestContainerView awTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(awContentsClient);
        final AwContents otherAwContents = awTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(otherAwContents);

        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String url = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        mActivityTestRule.loadUrlSync(
                otherAwContents, awContentsClient.getOnPageFinishedHelper(), url);

        // Verify that WebMessageListener was set successfually on mAwContents.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Verify that we don't have myObject injected to otherAwContents.
        Assert.assertFalse(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, otherAwContents, awContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAddWebMessageListenerAllowsCertainUrlWorksWithIframe() throws Throwable {
        final String frameUrl = mTestServer.getURL(POST_MESSAGE_SIMPLE_HTML);
        final String html = createCrossOriginAccessTestPageHtml(frameUrl);
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME, new String[] {parseOrigin(frameUrl)}, mListener);

        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                false,
                "http://www.google.com",
                null);

        // The iframe should have myObject injected.
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.getAsString());

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Verify that the main frame has no myObject injected.
        Assert.assertFalse(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testRemoveWebMessageListener_preventInjectionForNextPageLoad() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Load the the page.
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.getAsString());

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Remove WebMessageListener will disable injection for next page load.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        // Should have no myObject injected.
        Assert.assertFalse(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testRemoveWebMessageListener_cutJsJavaMappingImmediately() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // Load the the page.
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data.getAsString());

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Remove WebMessageListener will disable injection for next page load.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        // Should still have myObject.
        Assert.assertTrue(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // But posting message on myObject will be dropped.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, JS_OBJECT_NAME + ".postMessage('" + HELLO + "');");
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testRemoveWebMessageListener_removeWithNoAddWebMessageListener() throws Throwable {
        // Call removeWebMessageListener() without addWebMessageListener() shouldn't fail.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testRemoveWebMessageListener_removeBeforeLoadPage() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        Assert.assertFalse(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testRemoveWebMessageListener_extraRemove() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);
        // Extra removeWebMessageListener() does nothing.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME);

        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        Assert.assertFalse(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAllowedOriginsWorksForVariousBaseUrls() throws Throwable {
        // Set a typical rule.
        addWebMessageListenerOnUiThread(
                mAwContents,
                JS_OBJECT_NAME,
                new String[] {"https://www.example.com:443"},
                mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com:8080", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("http://www.example.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("http://example.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.google.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("file://etc", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("ftp://example.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl(null, JS_OBJECT_NAME));

        // Inject to all frames.
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, new String[] {"*"}, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com:8080", JS_OBJECT_NAME_2));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("http://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("http://example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.google.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("file://etc", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("ftp://example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl(null, JS_OBJECT_NAME_2));

        // WebView doesn't support ftp with loadUrl() but ftp scheme could happen with
        // loadDataWithBaseUrl().
        final String jsObjectName3 = JS_OBJECT_NAME + "3";
        addWebMessageListenerOnUiThread(
                mAwContents, jsObjectName3, new String[] {"ftp://"}, mListener);
        // ftp is a standard scheme, so the origin will be "ftp://example.com", however we don't
        // support host rule for ftp://, so it won't do the injection.
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("ftp://example.com", jsObjectName3));

        // file scheme.
        final String jsObjectName4 = JS_OBJECT_NAME + "4";
        addWebMessageListenerOnUiThread(
                mAwContents, jsObjectName4, new String[] {"file://"}, mListener);
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("file://etc", jsObjectName4));

        // Pass an URI instead of origin shouldn't work.
        final String jsObjectName5 = JS_OBJECT_NAME + "5";
        RuntimeException exception =
                Assert.assertThrows(
                        RuntimeException.class,
                        () ->
                                addWebMessageListenerOnUiThread(
                                        mAwContents,
                                        jsObjectName5,
                                        new String[] {"https://www.example.com/index.html"},
                                        mListener));
        // Should catch IllegalArgumentException in the end of the re-throw chain.
        Assert.assertTrue(exception.getCause() instanceof IllegalArgumentException);
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", jsObjectName5));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDontAllowAddWebMessageLitenerWithTheSameJsObjectName() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        RuntimeException exception =
                Assert.assertThrows(
                        RuntimeException.class,
                        () ->
                                addWebMessageListenerOnUiThread(
                                        mAwContents,
                                        JS_OBJECT_NAME,
                                        new String[] {"*"},
                                        new TestWebMessageListener()));
        Assert.assertTrue(exception.getCause() instanceof IllegalArgumentException);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAddWebMessageListener_SameOrigins() throws Throwable {
        final String[] allowedOriginRules =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, allowedOriginRules, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME));

        // Call addWebMessageListener() with the same set of origins.
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, allowedOriginRules, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME_2));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAddWebMessageListener_OverlappingSetOfOrigins() throws Throwable {
        final String[] allowedOriginRules1 =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME, allowedOriginRules1, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME));

        final String[] allowedOriginRules2 =
                new String[] {"https://www.example.com", "https://www.ok.com"};
        // Call addWebMessageListener with overlapping set of origins.
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, allowedOriginRules2, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME_2));

        // Remove the listener should remove the js object from the next navigation.
        removeWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME_2);

        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME_2));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testAddWebMessageListener_NonOverlappingSetOfOrigins() throws Throwable {
        final String[] allowedOriginRules1 =
                new String[] {"https://www.example.com", "https://www.allowed.com"};
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME, allowedOriginRules1, mListener);

        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME));
        Assert.assertTrue(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME));

        final String[] allowedOriginRules2 = new String[] {"https://www.ok.com"};
        // Call addWebMessageListener with non-overlapping set of origins.
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, allowedOriginRules2, mListener);

        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.example.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.allowed.com", JS_OBJECT_NAME_2));
        Assert.assertTrue(isJsObjectInjectedWhenLoadingUrl("https://www.ok.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(
                isJsObjectInjectedWhenLoadingUrl("https://www.noinjection.com", JS_OBJECT_NAME_2));
        Assert.assertFalse(isJsObjectInjectedWhenLoadingUrl("", JS_OBJECT_NAME_2));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testJsReplyProxyWorks() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        loadUrlFromPath(POST_MESSAGE_REPLY_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        // JavaScript code in the page will change the title to NEW_TITLE if postMessage to proxy
        // succeed.
        final OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        final int titleCallCount = onReceivedTitleHelper.getCallCount();
        data.mReplyProxy.postMessage(new MessagePayload(NEW_TITLE));
        onReceivedTitleHelper.waitForCallback(titleCallCount);

        Assert.assertEquals(NEW_TITLE, onReceivedTitleHelper.getTitle());

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostArrayBufferEncodeToString() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        loadUrlFromPath(POST_MESSAGE_ARRAYBUFFER_TITLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        final String messageStr = HELLO + "FromJava";

        final OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        final int titleCallCount = onReceivedTitleHelper.getCallCount();
        data.mReplyProxy.postMessage(
                new MessagePayload(messageStr.getBytes(StandardCharsets.UTF_8)));
        onReceivedTitleHelper.waitForCallback(titleCallCount);

        Assert.assertEquals(messageStr, onReceivedTitleHelper.getTitle());
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    private void verifyPostArrayBufferWorks(byte[] content) throws Exception {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        loadUrlFromPath(POST_MESSAGE_ARRAYBUFFER_REPLY_HTML);
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        data.mReplyProxy.postMessage(new MessagePayload(content));
        data = mListener.waitForOnPostMessage();
        Assert.assertArrayEquals(content, data.getAsArrayBuffer());
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostArrayBufferWorks() throws Throwable {
        final byte[] content = (HELLO + "FromJava").getBytes(StandardCharsets.UTF_8);
        verifyPostArrayBufferWorks(content);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostEmptyArrayBuffer() throws Throwable {
        final byte[] content = new byte[0];
        verifyPostArrayBufferWorks(content);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostLargeArrayBuffer() throws Throwable {
        final byte[] content = new byte[500 * 1000]; // 500 Kib
        new Random(42).nextBytes(content);
        verifyPostArrayBufferWorks(content);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostNullOrUndefinedShouldThrowExceptionWithArrayBufferFeature()
            throws Throwable {
        final byte[] content = (HELLO + "FromJava").getBytes(StandardCharsets.UTF_8);
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        loadUrlFromPath(POST_MESSAGE_NULL_OR_UNDEFINED_HTML);
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        data.mReplyProxy.postMessage(new MessagePayload(content));

        // Null
        data = mListener.waitForOnPostMessage();
        String errorString = data.getAsString();
        Assert.assertTrue(errorString.contains("Error"));

        // Undefined
        data = mListener.waitForOnPostMessage();
        errorString = data.getAsString();
        Assert.assertTrue(errorString.contains("Error"));
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testJsReplyProxyReplyToTheCorrectJsObject() throws Throwable {
        final TestWebMessageListener webMessageListener2 = new TestWebMessageListener();
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        addWebMessageListenerOnUiThread(
                mAwContents, JS_OBJECT_NAME_2, new String[] {"*"}, webMessageListener2);

        loadUrlFromPath(POST_MESSAGE_REPLY_HTML);

        // Listener for myObject.
        final String listener1 =
                "function (event) {"
                        + "  "
                        + JS_OBJECT_NAME
                        + ".postMessage('ack1' + event.data);"
                        + "}";

        // Listener for myObject2.
        final String listener2 =
                "function (event) {"
                        + "  "
                        + JS_OBJECT_NAME_2
                        + ".postMessage('ack2' + event.data);"
                        + "}";

        // Add two different js objects.
        addEventListener(
                listener1,
                "listener1",
                JS_OBJECT_NAME,
                mActivityTestRule,
                mAwContents,
                mContentsClient);
        addEventListener(
                listener2,
                "listener2",
                JS_OBJECT_NAME_2,
                mActivityTestRule,
                mAwContents,
                mContentsClient);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        final String message = "message";
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents,
                mContentsClient,
                JS_OBJECT_NAME_2 + ".postMessage('" + message + "');");
        TestWebMessageListener.Data data2 = webMessageListener2.waitForOnPostMessage();

        Assert.assertEquals(message, data2.getAsString());

        // Targeting myObject.
        data.mReplyProxy.postMessage(new MessagePayload(HELLO));
        // Targeting myObject2.
        data2.mReplyProxy.postMessage(new MessagePayload(HELLO));

        TestWebMessageListener.Data replyData1 = mListener.waitForOnPostMessage();
        TestWebMessageListener.Data replyData2 = webMessageListener2.waitForOnPostMessage();

        Assert.assertEquals("ack1" + HELLO, replyData1.getAsString());
        Assert.assertEquals("ack2" + HELLO, replyData2.getAsString());

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
        Assert.assertTrue(webMessageListener2.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testJsReplyProxyDropsMessageIfJsObjectIsGone() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        loadUrlFromPath(POST_MESSAGE_REPLY_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        JsReplyProxy proxy = data.mReplyProxy;

        // Load the same url again.
        loadUrlFromPath(POST_MESSAGE_REPLY_HTML);
        mListener.waitForOnPostMessage();

        // Use the previous JsReplyProxy to send message. It should drop the message.
        proxy.postMessage(new MessagePayload(NEW_TITLE));

        // Call evaluateJavascript to make sure the previous postMessage() call is reached to
        // renderer if it should, since these messages are in sequence.
        Assert.assertTrue(
                hasJavaScriptObject(
                        JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient));

        // Title shouldn't change.
        Assert.assertNotEquals(NEW_TITLE, mActivityTestRule.getTitleOnUiThread(mAwContents));

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testJsAddAndRemoveEventListener() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        JsReplyProxy proxy = mListener.waitForOnPostMessage().mReplyProxy;

        final String listener1 =
                "function (event) {"
                        + "  if (window.receivedCount1) {"
                        + "    window.receivedCount1++;"
                        + "  } else {"
                        + "    window.receivedCount1 = 1;"
                        + "  }"
                        + "  "
                        + JS_OBJECT_NAME
                        + ".postMessage('ack1:' + window.receivedCount1);"
                        + "}";

        final String listener2 =
                "function (event) {"
                        + "  if (window.receivedCount2) {"
                        + "    window.receivedCount2++;"
                        + "  } else {"
                        + "    window.receivedCount2 = 1;"
                        + "  }"
                        + "  "
                        + JS_OBJECT_NAME
                        + ".postMessage('ack2:' + window.receivedCount2);"
                        + "}";

        addEventListener(
                listener1,
                "listener1",
                JS_OBJECT_NAME,
                mActivityTestRule,
                mAwContents,
                mContentsClient);
        addEventListener(
                listener2,
                "listener2",
                JS_OBJECT_NAME,
                mActivityTestRule,
                mAwContents,
                mContentsClient);

        // Post message to test both listeners receive message.
        proxy.postMessage(new MessagePayload(HELLO));

        TestWebMessageListener.Data replyData1 = mListener.waitForOnPostMessage();
        TestWebMessageListener.Data replyData2 = mListener.waitForOnPostMessage();

        Assert.assertEquals("ack1:1", replyData1.getAsString());
        Assert.assertEquals("ack2:1", replyData2.getAsString());

        removeEventListener(
                "listener2", JS_OBJECT_NAME, mActivityTestRule, mAwContents, mContentsClient);

        // Post message again to test if remove works.
        proxy.postMessage(new MessagePayload(HELLO));

        // listener 1 should add message again.
        TestWebMessageListener.Data replyData3 = mListener.waitForOnPostMessage();
        Assert.assertEquals("ack1:2", replyData3.getAsString());

        // Should be no more messages.
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testJsObjectRemoveOnMessage() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        loadUrlFromPath(POST_MESSAGE_REPLY_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        // JavaScript code in the page will change the title to NEW_TITLE if postMessage to proxy
        // succeed.
        final OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        final int titleCallCount = onReceivedTitleHelper.getCallCount();
        data.mReplyProxy.postMessage(new MessagePayload(NEW_TITLE));
        onReceivedTitleHelper.waitForCallback(titleCallCount);

        Assert.assertEquals(NEW_TITLE, onReceivedTitleHelper.getTitle());

        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, JS_OBJECT_NAME + ".onmessage = undefined;");
        Assert.assertEquals(
                "null",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, JS_OBJECT_NAME + ".onmessage"));

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    private void verifyOnPostMessageOriginIsNull() throws Throwable {
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, JS_OBJECT_NAME + ".postMessage('Hello');");

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        Assert.assertEquals("null", data.mSourceOrigin.toString());

        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testFileSchemeUrl_setAllowFileAccessFromFile_true() throws Throwable {
        mAwContents.getSettings().setAllowFileAccessFromFileURLs(true);
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), FILE_URI);
        Assert.assertEquals(
                "\"file://\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "window.origin"));

        verifyOnPostMessageOriginIsNull();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    @SkipMutations(
            reason = "This test depends on AwSettings.setAllowUniversalAccessFromFileURLs(false)")
    public void testFileSchemeUrl_setAllowFileAccessFromFile_false() throws Throwable {
        // The default value is false on JELLY_BEAN and above, but we explicitly set this to
        // false to readability.
        mAwContents.getSettings().setAllowFileAccessFromFileURLs(false);
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), FILE_URI);
        Assert.assertEquals(
                "\"null\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "window.origin"));

        verifyOnPostMessageOriginIsNull();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testContentSchemeUrl_setAllowFileAccessFromFileURLs_true() throws Throwable {
        mAwContents.getSettings().setAllowContentAccess(true);
        mAwContents.getSettings().setAllowFileAccessFromFileURLs(true);
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                TestContentProvider.createContentUrl("content_access"));
        Assert.assertEquals(
                "\"content://\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "window.origin"));

        verifyOnPostMessageOriginIsNull();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    @SkipMutations(
            reason = "This test depends on AwSettings.setAllowUniversalAccessFromFileURLs(false)")
    public void testContentSchemeUrl_setAllowFileAccessFromFileURLs_false() throws Throwable {
        mAwContents.getSettings().setAllowContentAccess(true);
        // The default value is false on JELLY_BEAN and above, but we explicitly set this to
        // false to readability.
        mAwContents.getSettings().setAllowFileAccessFromFileURLs(false);
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                TestContentProvider.createContentUrl("content_access"));
        Assert.assertEquals(
                "\"null\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, "window.origin"));

        verifyOnPostMessageOriginIsNull();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWebMessageListenerForPopupWindow() throws Throwable {
        TestWebServer webServer = TestWebServer.start();

        final String popupPath = "/popup.html";
        final String parentPageHtml =
                CommonResources.makeHtmlPageFrom(
                        "",
                        "<script>"
                                + "function tryOpenWindow() {"
                                + "  var newWindow = window.open('"
                                + popupPath
                                + "');"
                                + "}</script>");

        final String popupPageHtml =
                CommonResources.makeHtmlPageFrom("<title>popup</title>", "This is a popup window");

        mActivityTestRule.triggerPopup(
                mAwContents,
                mContentsClient,
                webServer,
                parentPageHtml,
                popupPageHtml,
                popupPath,
                "tryOpenWindow()");
        AwActivityTestRule.PopupInfo popupInfo = mActivityTestRule.createPopupContents(mAwContents);
        TestAwContentsClient popupContentsClient = popupInfo.popupContentsClient;
        final AwContents popupContents = popupInfo.popupContents;

        // App adds WebMessageListener to the child WebView, e.g. in onCreateWindow().
        addWebMessageListenerOnUiThread(
                popupContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        mActivityTestRule.loadPopupContents(mAwContents, popupInfo, null);

        // Test if the listener was re-injected.
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                popupContents, popupContentsClient, JS_OBJECT_NAME + ".postMessage('Hello');");

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        webServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessage_JsObjectName_Number() throws Throwable {
        checkInjectAndAccessJsObjectNameAsWindowProperty("123");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessage_JsObjectName_Symbol() throws Throwable {
        checkInjectAndAccessJsObjectNameAsWindowProperty("*");
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testPostMessage_JsObjectName_Keyword() throws Throwable {
        checkInjectAndAccessJsObjectNameAsWindowProperty("var");
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_addJavascriptInterfaceShouldBeAvaliable()
            throws Throwable {
        final LinkedBlockingQueue<String> javascriptInterfaceQueue = new LinkedBlockingQueue<>();
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                mAwContents,
                new Object() {
                    @JavascriptInterface
                    public void send(String message) {
                        javascriptInterfaceQueue.add(message);
                    }
                },
                "javaBridge");
        addDocumentStartJavaScriptOnUiThread(
                mAwContents, "javaBridge.send('" + HELLO + "');", new String[] {"*"});

        loadUrlFromPath(HELLO_WORLD_HTML);

        String message = AwActivityTestRule.waitForNextQueueElement(javascriptInterfaceQueue);

        Assert.assertEquals(HELLO, message);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_jsObjectShouldBeAvaliable() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        addDocumentStartJavaScriptOnUiThread(
                mAwContents, JS_OBJECT_NAME + ".postMessage('" + HELLO + "');", new String[] {"*"});

        String url = loadUrlFromPath(HELLO_WORLD_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_runBeforeUserScript() throws Throwable {
        addDocumentStartJavaScriptOnUiThread(
                mAwContents,
                JS_OBJECT_NAME + ".postMessage('" + HELLO + "1');",
                new String[] {"*"});
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        // POST_MESSAGE_SIMPLE_HTML will post HELLO message.
        String url = loadUrlFromPath(POST_MESSAGE_SIMPLE_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO + "1", data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        TestWebMessageListener.Data data2 = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO, data2.getAsString());
        Assert.assertTrue(data2.mIsMainFrame);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_multipleScripts() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        addDocumentStartJavaScriptOnUiThread(
                mAwContents,
                JS_OBJECT_NAME + ".postMessage('" + HELLO + "0');",
                new String[] {"*"});
        addDocumentStartJavaScriptOnUiThread(
                mAwContents,
                JS_OBJECT_NAME + ".postMessage('" + HELLO + "1');",
                new String[] {"*"});

        String url = loadUrlFromPath(HELLO_WORLD_HTML);

        for (int i = 0; i < 2; ++i) {
            TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

            assertUrlHasOrigin(url, data.mSourceOrigin);
            Assert.assertEquals(HELLO + Integer.toString(i), data.getAsString());
            Assert.assertTrue(data.mIsMainFrame);
            Assert.assertEquals(0, data.mPorts.length);
        }

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_callAgainAfterPageLoad() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        addDocumentStartJavaScriptOnUiThread(
                mAwContents,
                JS_OBJECT_NAME + ".postMessage('" + HELLO + "0');",
                new String[] {"*"});

        String url = loadUrlFromPath(HELLO_WORLD_HTML);

        addDocumentStartJavaScriptOnUiThread(
                mAwContents,
                JS_OBJECT_NAME + ".postMessage('" + HELLO + "1');",
                new String[] {"*"});
        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO + "0", data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        // Load the page again.
        loadUrlFromPath(HELLO_WORLD_HTML);

        for (int i = 0; i < 2; ++i) {
            TestWebMessageListener.Data data2 = mListener.waitForOnPostMessage();

            Assert.assertEquals(HELLO + Integer.toString(i), data2.getAsString());
            Assert.assertTrue(data2.mIsMainFrame);
        }

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_allowedOriginsRulesWithDifferentBaseUrl()
            throws Throwable {
        // With a standard origin rule.
        final String testObjectName = "test";
        addDocumentStartJavaScriptOnUiThread(
                mAwContents,
                "let " + testObjectName + " = {};",
                new String[] {"https://www.example.com:443"});

        Assert.assertTrue(didScriptRunWhenLoading("https://www.example.com", testObjectName));
        Assert.assertFalse(didScriptRunWhenLoading("https://www.example.com:8080", testObjectName));
        Assert.assertFalse(didScriptRunWhenLoading("http://www.example.com", testObjectName));
        Assert.assertFalse(didScriptRunWhenLoading("http://example.com", testObjectName));
        Assert.assertFalse(didScriptRunWhenLoading("https://www.google.com", testObjectName));
        Assert.assertFalse(didScriptRunWhenLoading("file://etc", testObjectName));
        Assert.assertFalse(didScriptRunWhenLoading("ftp://example.com", testObjectName));
        Assert.assertFalse(didScriptRunWhenLoading(null, testObjectName));

        // Match all the origins.
        final String testObjectName2 = testObjectName + "2";
        addDocumentStartJavaScriptOnUiThread(
                mAwContents, "let " + testObjectName2 + " = {};", new String[] {"*"});

        Assert.assertTrue(didScriptRunWhenLoading("https://www.example.com", testObjectName2));
        Assert.assertTrue(didScriptRunWhenLoading("https://www.example.com:8080", testObjectName2));
        Assert.assertTrue(didScriptRunWhenLoading("http://www.example.com", testObjectName2));
        Assert.assertTrue(didScriptRunWhenLoading("http://example.com", testObjectName2));
        Assert.assertTrue(didScriptRunWhenLoading("https://www.google.com", testObjectName2));
        Assert.assertTrue(didScriptRunWhenLoading("file://etc", testObjectName2));
        Assert.assertTrue(didScriptRunWhenLoading("ftp://example.com", testObjectName2));
        Assert.assertTrue(didScriptRunWhenLoading(null, testObjectName2));
        // data: scheme could be matched with "*".
        final String html = "<html><body><div>data</div></body></html>";
        mActivityTestRule.loadHtmlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), html);
        Assert.assertTrue(
                hasJavaScriptObject(
                        testObjectName2, mActivityTestRule, mAwContents, mContentsClient));

        // Wrong origin rule.
        final String testObjectName5 = testObjectName + "5";
        RuntimeException exception =
                Assert.assertThrows(
                        RuntimeException.class,
                        () ->
                                addDocumentStartJavaScriptOnUiThread(
                                        mAwContents,
                                        "let " + testObjectName5 + " = {};",
                                        new String[] {"https://www.example.com/index.html"}));
        Assert.assertTrue(
                "The exception should be an IllegalArgumentException",
                exception.getCause() instanceof IllegalArgumentException);
        Assert.assertFalse(didScriptRunWhenLoading("https://www.example.com", testObjectName5));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_willRunInIframe() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        final String script =
                "if (window.location.origin !== 'http://www.google.com') {"
                        + "  "
                        + JS_OBJECT_NAME
                        + ".postMessage('"
                        + HELLO
                        + "');"
                        + "}";
        // Since we are matching both origins, the script will run in both iframe and main frame,
        // but it will send message in only iframe.
        addDocumentStartJavaScriptOnUiThread(mAwContents, script, new String[] {"*"});

        final String frameUrl = mTestServer.getURL(HELLO_WORLD_HTML);
        final String html = createCrossOriginAccessTestPageHtml(frameUrl);

        // Load a cross origin iframe page, the www.google.com page is the main frame, test server
        // page is the iframe.
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                false,
                "http://www.google.com",
                null);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(frameUrl, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertFalse(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_removeScript() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);
        ScriptHandler[] handlers = new ScriptHandler[2];
        for (int i = 0; i < 2; ++i) {
            final String script =
                    JS_OBJECT_NAME + ".postMessage('" + HELLO + Integer.toString(i) + "');";
            // Since we are matching both origins, the script will run in both iframe and main
            // frame, but it will send message in only iframe.
            handlers[i] =
                    addDocumentStartJavaScriptOnUiThread(mAwContents, script, new String[] {"*"});
        }

        final String url = loadUrlFromPath(HELLO_WORLD_HTML);

        for (int i = 0; i < 2; ++i) {
            TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

            assertUrlHasOrigin(url, data.mSourceOrigin);
            Assert.assertEquals(HELLO + Integer.toString(i), data.getAsString());
        }

        ThreadUtils.runOnUiThreadBlocking(() -> handlers[0].remove());
        // Load the page again.
        loadUrlFromPath(HELLO_WORLD_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();
        Assert.assertEquals(HELLO + "1", data.getAsString());

        ThreadUtils.runOnUiThreadBlocking(() -> handlers[1].remove());
        // Load the page again.
        loadUrlFromPath(HELLO_WORLD_HTML);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView", "JsJavaInteraction"})
    public void testDocumentStartJavaScript_doubleRemoveScript() throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, JS_OBJECT_NAME, new String[] {"*"}, mListener);

        final String script = JS_OBJECT_NAME + ".postMessage('" + HELLO + "');";
        ScriptHandler handler =
                addDocumentStartJavaScriptOnUiThread(mAwContents, script, new String[] {"*"});

        final String url = loadUrlFromPath(HELLO_WORLD_HTML);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        assertUrlHasOrigin(url, data.mSourceOrigin);
        Assert.assertEquals(HELLO, data.getAsString());

        // Remove twice, the second time should take no effect.
        ThreadUtils.runOnUiThreadBlocking(() -> handler.remove());
        ThreadUtils.runOnUiThreadBlocking(() -> handler.remove());
        // Load the page again.
        loadUrlFromPath(HELLO_WORLD_HTML);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());

        // Remove twice again, should have no effect.
        ThreadUtils.runOnUiThreadBlocking(() -> handler.remove());
        ThreadUtils.runOnUiThreadBlocking(() -> handler.remove());
        // Load the page again.
        loadUrlFromPath(HELLO_WORLD_HTML);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    private void checkInjectAndAccessJsObjectNameAsWindowProperty(String jsObjName)
            throws Throwable {
        addWebMessageListenerOnUiThread(mAwContents, jsObjName, new String[] {"*"}, mListener);

        String html =
                "<html><head><script>window['"
                        + jsObjName
                        + "'].postMessage('Hello');"
                        + "</script></head><body><div>postMessage</div></body></html>";
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                html,
                "text/html",
                false,
                "http://www.google.com",
                null);

        TestWebMessageListener.Data data = mListener.waitForOnPostMessage();

        Assert.assertEquals("http://www.google.com", data.mSourceOrigin.toString());
        Assert.assertEquals(HELLO, data.getAsString());
        Assert.assertTrue(data.mIsMainFrame);
        Assert.assertEquals(0, data.mPorts.length);

        Assert.assertTrue(mListener.hasNoMoreOnPostMessage());
    }

    private boolean isJsObjectInjectedWhenLoadingUrl(
            final String baseUrl, final String jsObjectName) throws Throwable {
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                DATA_HTML,
                "text/html",
                false,
                baseUrl,
                null);
        return hasJavaScriptObject(jsObjectName, mActivityTestRule, mAwContents, mContentsClient);
    }

    // The script is trying to set a global JavaScript object, so it is essentially the same
    // with isJsObjectInjectedWhenLoadingUrl(). Having a wrapper method to make it clear for
    // the context.
    private boolean didScriptRunWhenLoading(final String baseUrl, final String objectName)
            throws Throwable {
        return isJsObjectInjectedWhenLoadingUrl(baseUrl, objectName);
    }

    private String loadUrlFromPath(String path) throws Exception {
        final String url = mTestServer.getURL(path);
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        return url;
    }

    private static void assertUrlHasOrigin(final String url, final Uri origin) {
        Assert.assertEquals("The origin URI must not contain a path", "", origin.getPath());
        Assert.assertEquals("The origin URI must not contain any queries", null, origin.getQuery());
        Assert.assertEquals(
                "The origin URI must not contain a fragment", null, origin.getFragment());

        Uri uriFromServer = Uri.parse(url);
        Assert.assertEquals(uriFromServer.getScheme(), origin.getScheme());
        Assert.assertEquals(uriFromServer.getHost(), origin.getHost());
        Assert.assertEquals(uriFromServer.getPort(), origin.getPort());
    }

    private static String createCrossOriginAccessTestPageHtml(final String frameUrl) {
        return "<html>"
                + "<body><div>I have an iframe</ div>"
                + "  <iframe src ='"
                + frameUrl
                + "'></iframe>"
                + "</body></html>";
    }

    private static ScriptHandler addDocumentStartJavaScriptOnUiThread(
            final AwContents awContents, final String script, final String[] allowedOriginRules)
            throws Exception {
        AwActivityTestRule.checkJavaScriptEnabled(awContents);
        return ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.addDocumentStartJavaScript(script, allowedOriginRules));
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

    private static void removeWebMessageListenerOnUiThread(
            final AwContents awContents, final String jsObjectName) throws Exception {
        TestWebMessageListener.removeWebMessageListenerOnUiThread(awContents, jsObjectName);
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

    private static void addEventListener(
            final String func,
            final String funcName,
            String jsObjectName,
            final AwActivityTestRule rule,
            final AwContents awContents,
            final TestAwContentsClient contentsClient)
            throws Throwable {
        String code = "let " + funcName + " = " + func + ";";
        code += jsObjectName + ".addEventListener('message', " + funcName + ");";
        rule.executeJavaScriptAndWaitForResult(awContents, contentsClient, code);
    }

    private static void removeEventListener(
            final String funcName,
            final String jsObjectName,
            final AwActivityTestRule rule,
            final AwContents awContents,
            final TestAwContentsClient contentsClient)
            throws Throwable {
        String code = jsObjectName + ".removeEventListener('message', " + funcName + ")";
        rule.executeJavaScriptAndWaitForResult(awContents, contentsClient, code);
    }

    private static String parseOrigin(String url) {
        final Uri uri = Uri.parse(url);
        final StringBuilder sb = new StringBuilder();
        final String scheme = uri.getScheme();
        final int port = uri.getPort();

        if (!scheme.isEmpty()) {
            sb.append(scheme);
            sb.append("://");
        }
        sb.append(uri.getHost());
        if (port != -1) {
            sb.append(":");
            sb.append(port);
        }
        return sb.toString();
    }
}
