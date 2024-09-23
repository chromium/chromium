// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.graphics.Rect;
import android.net.Uri;
import android.webkit.JavascriptInterface;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.test.AwActivityTestRule.PopupInfo;
import org.chromium.android_webview.test.TestAwContentsClient.ShouldInterceptRequestHelper;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;

import java.util.List;
import java.util.Locale;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Tests for pop up window flow. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class PopupWindowTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mParentContentsClient;
    private AwTestContainerView mParentContainerView;
    private AwContents mParentContents;
    private TestWebServer mWebServer;

    private static final String POPUP_TITLE = "Popup Window";

    public PopupWindowTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mParentContentsClient = new TestAwContentsClient();
        mParentContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mParentContentsClient);
        mParentContents = mParentContainerView.getAwContents();
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
    public void testPopupWindow() throws Throwable {
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
                CommonResources.makeHtmlPageFrom(
                        "<title>" + POPUP_TITLE + "</title>", "This is a popup window");

        mActivityTestRule.triggerPopup(
                mParentContents,
                mParentContentsClient,
                mWebServer,
                parentPageHtml,
                popupPageHtml,
                popupPath,
                "tryOpenWindow()");
        AwContents popupContents =
                mActivityTestRule.connectPendingPopup(mParentContents).popupContents;
        Assert.assertEquals(POPUP_TITLE, mActivityTestRule.getTitleOnUiThread(popupContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testJavascriptInterfaceForPopupWindow() throws Throwable {
        // android.webkit.cts.WebViewTest#testJavascriptInterfaceForClientPopup
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
                CommonResources.makeHtmlPageFrom(
                        "<title>" + POPUP_TITLE + "</title>", "This is a popup window");

        mActivityTestRule.triggerPopup(
                mParentContents,
                mParentContentsClient,
                mWebServer,
                parentPageHtml,
                popupPageHtml,
                popupPath,
                "tryOpenWindow()");
        PopupInfo popupInfo = mActivityTestRule.createPopupContents(mParentContents);
        TestAwContentsClient popupContentsClient = popupInfo.popupContentsClient;
        final AwContents popupContents = popupInfo.popupContents;

        class TestJavaScriptInterface {
            @JavascriptInterface
            public int test() {
                return 42;
            }
        }
        final TestJavaScriptInterface obj = new TestJavaScriptInterface();

        AwActivityTestRule.addJavascriptInterfaceOnUiThread(popupContents, obj, "interface");

        mActivityTestRule.loadPopupContents(mParentContents, popupInfo, null);

        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    String ans =
                            mActivityTestRule.executeJavaScriptAndWaitForResult(
                                    popupContents, popupContentsClient, "interface.test()");

                    return ans.equals("42");
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDefaultUserAgentsInParentAndChildWindows() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mParentContents).setJavaScriptEnabled(true);
        mActivityTestRule.loadUrlSync(
                mParentContents, mParentContentsClient.getOnPageFinishedHelper(), "about:blank");
        String parentUserAgent =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mParentContents, mParentContentsClient, "navigator.userAgent");

        final String popupPath = "/popup.html";
        final String myUserAgentString = "myUserAgent";
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
                "<html><head><script>"
                        + "document.title = navigator.userAgent;"
                        + "</script><body><div id='a'></div></body></html>";

        mActivityTestRule.triggerPopup(
                mParentContents,
                mParentContentsClient,
                mWebServer,
                parentPageHtml,
                popupPageHtml,
                popupPath,
                "tryOpenWindow()");

        PopupInfo popupInfo = mActivityTestRule.createPopupContents(mParentContents);
        TestAwContentsClient popupContentsClient = popupInfo.popupContentsClient;
        final AwContents popupContents = popupInfo.popupContents;

        mActivityTestRule.loadPopupContents(mParentContents, popupInfo, null);

        final String childUserAgent =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        popupContents, popupContentsClient, "navigator.userAgent");

        Assert.assertEquals(parentUserAgent, childUserAgent);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOverrideUserAgentInOnCreateWindow() throws Throwable {
        final String popupPath = "/popup.html";
        final String myUserAgentString = "myUserAgent";
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
                "<html><head><script>"
                        + "document.title = navigator.userAgent;"
                        + "</script><body><div id='a'></div></body></html>";

        mActivityTestRule.triggerPopup(
                mParentContents,
                mParentContentsClient,
                mWebServer,
                parentPageHtml,
                popupPageHtml,
                popupPath,
                "tryOpenWindow()");

        PopupInfo popupInfo = mActivityTestRule.createPopupContents(mParentContents);
        TestAwContentsClient popupContentsClient = popupInfo.popupContentsClient;
        final AwContents popupContents = popupInfo.popupContents;

        // Override the user agent string for the popup window.
        mActivityTestRule.loadPopupContents(
                mParentContents,
                popupInfo,
                new AwActivityTestRule.OnCreateWindowHandler() {
                    @Override
                    public boolean onCreateWindow(AwContents awContents) {
                        awContents.getSettings().setUserAgentString(myUserAgentString);
                        return true;
                    }
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                mActivityTestRule.getTitleOnUiThread(popupContents),
                                Matchers.is(myUserAgentString));
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSynthesizedOnPageFinishedCalledOnceAfterDomModificationDuringNavigation()
            throws Throwable {
        final String popupPath = "/popup.html";
        final String parentPageHtml =
                CommonResources.makeHtmlPageFrom(
                        "",
                        "<script>"
                                + "function tryOpenWindow() {"
                                + "  window.popupWindow = window.open('"
                                + popupPath
                                + "');}function modifyDomOfPopup() { "
                                + " window.popupWindow.document.body.innerHTML = 'Hello from the"
                                + " parent!';}</script>");

        mActivityTestRule.triggerPopup(
                mParentContents,
                mParentContentsClient,
                mWebServer,
                parentPageHtml,
                "<html></html>",
                popupPath,
                "tryOpenWindow()");
        PopupInfo popupInfo = mActivityTestRule.createPopupContents(mParentContents);
        TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                popupInfo.popupContentsClient.getOnPageFinishedHelper();
        ShouldInterceptRequestHelper shouldInterceptRequestHelper =
                popupInfo.popupContentsClient.getShouldInterceptRequestHelper();
        int onPageFinishedCount = onPageFinishedHelper.getCallCount();
        int shouldInterceptRequestCount = shouldInterceptRequestHelper.getCallCount();
        // Modify DOM before navigation gets committed. Once it gets committed, then
        // DidAccessInitialDocument does not get triggered.
        popupInfo
                .popupContentsClient
                .getShouldInterceptRequestHelper()
                .runDuringFirstTimeCallback(
                        () -> {
                            ThreadUtils.assertOnBackgroundThread();
                            try {
                                // Ensures that we modify DOM before navigation gets committed.
                                mActivityTestRule.executeJavaScriptAndWaitForResult(
                                        mParentContents,
                                        mParentContentsClient,
                                        "modifyDomOfPopup()");
                            } catch (Exception e) {
                                throw new RuntimeException(e);
                            }
                        });
        mActivityTestRule.loadPopupContents(mParentContents, popupInfo, null);
        shouldInterceptRequestHelper.waitForCallback(shouldInterceptRequestCount);
        // Modifying DOM in the middle while loading a popup window - this causes navigation state
        // change from NavigationControllerImpl::DidAccessInitialMainDocument() eventually calling
        // AwWebContentsDelegateAdapter#navigationStateChanged(), resulting in an additional
        // onPageFinished() callback. Also, the navigation eventually will commit and trigger an
        // onPageFinished() call. However, no additional synthesized onPageFinished() calls from
        // the commits are expected as we only synthesize an onPageFinished() call at most once.
        // See also https://crbug.com/458569 and b/19325392 for context.
        onPageFinishedHelper.waitForCallback(onPageFinishedCount, 2);
        List<String> urlList = onPageFinishedHelper.getUrlList();

        // This is the URL that gets shown to the user (instead of the pending navigation's URL)
        // because the parent changed DOM of the popup window.
        Assert.assertEquals("about:blank", urlList.get(onPageFinishedCount));
        // Note that in this test we do not stop the navigation and we still navigate to the page
        // that we wanted. The loaded page does not have the changed DOM. This is slightly different
        // from the original workflow in b/19325392 as there is no good hook to stop navigation and
        // trigger DidAccessInitialDocument at the same time.
        Assert.assertTrue(urlList.get(onPageFinishedCount + 1).endsWith(popupPath));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPopupWindowTextHandle() throws Throwable {
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
                CommonResources.makeHtmlPageFrom(
                        "<title>" + POPUP_TITLE + "</title>",
                        "<span id=\"plain_text\" class=\"full_view\">This is a popup"
                                + " window.</span>");

        mActivityTestRule.triggerPopup(
                mParentContents,
                mParentContentsClient,
                mWebServer,
                parentPageHtml,
                popupPageHtml,
                popupPath,
                "tryOpenWindow()");
        PopupInfo popupInfo = mActivityTestRule.connectPendingPopup(mParentContents);
        final AwContents popupContents = popupInfo.popupContents;
        TestAwContentsClient popupContentsClient = popupInfo.popupContentsClient;
        Assert.assertEquals(POPUP_TITLE, mActivityTestRule.getTitleOnUiThread(popupContents));

        AwActivityTestRule.enableJavaScriptOnUiThread(popupContents);

        // Now long press on some texts and see if the text handles show up.
        DOMUtils.longPressNode(popupContents.getWebContents(), "plain_text");
        SelectionPopupController controller =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                SelectionPopupController.fromWebContents(
                                        popupContents.getWebContents()));
        assertWaitForSelectActionBarStatus(true, controller);
        Assert.assertTrue(ThreadUtils.runOnUiThreadBlocking(() -> controller.hasSelection()));

        // Now hide the select action bar. This should hide the text handles and
        // clear the selection.
        hideSelectActionMode(controller);

        assertWaitForSelectActionBarStatus(false, controller);
        String jsGetSelection = "window.getSelection().toString()";
        // Test window.getSelection() returns empty string "" literally.
        Assert.assertEquals(
                "\"\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        popupContents, popupContentsClient, jsGetSelection));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPopupWindowHasUserGestureForUserInitiated() throws Throwable {
        runPopupUserGestureTest(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPopupWindowHasUserGestureForUserInitiatedNoOpener() throws Throwable {
        runPopupUserGestureTest(false);
    }

    private void runPopupUserGestureTest(boolean hasOpener) throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mParentContents.getSettings().setJavaScriptEnabled(true);
                    mParentContents.getSettings().setSupportMultipleWindows(true);
                    mParentContents.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
                });

        final String body =
                String.format(
                        Locale.US,
                        "<a href=\"popup.html\" id=\"link\" %s target=\"_blank\">example.com</a>",
                        hasOpener ? "" : "rel=\"noopener noreferrer\"");
        final String mainHtml = CommonResources.makeHtmlPageFrom("", body);
        final String openerUrl = mWebServer.setResponse("/popupOpener.html", mainHtml, null);
        final String popupUrl =
                mWebServer.setResponse(
                        "/popup.html",
                        CommonResources.makeHtmlPageFrom(
                                "<title>" + POPUP_TITLE + "</title>", "This is a popup window"),
                        null);

        mParentContentsClient.getOnCreateWindowHelper().setReturnValue(true);
        mActivityTestRule.loadUrlSync(
                mParentContents, mParentContentsClient.getOnPageFinishedHelper(), openerUrl);

        TestAwContentsClient.OnCreateWindowHelper onCreateWindowHelper =
                mParentContentsClient.getOnCreateWindowHelper();
        int currentCallCount = onCreateWindowHelper.getCallCount();
        JSUtils.clickNodeWithUserGesture(mParentContents.getWebContents(), "link");
        onCreateWindowHelper.waitForCallback(
                currentCallCount, 1, AwActivityTestRule.WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        Assert.assertTrue(onCreateWindowHelper.getIsUserGesture());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testPopupWindowNoUserGestureForJsInitiated() throws Throwable {
        final String popupPath = "/popup.html";
        final String openerPageHtml =
                CommonResources.makeHtmlPageFrom(
                        "",
                        "<script>"
                                + "function tryOpenWindow() {"
                                + "  var newWindow = window.open('"
                                + popupPath
                                + "');"
                                + "}</script>");

        final String popupPageHtml =
                CommonResources.makeHtmlPageFrom(
                        "<title>" + POPUP_TITLE + "</title>", "This is a popup window");

        mActivityTestRule.triggerPopup(
                mParentContents,
                mParentContentsClient,
                mWebServer,
                openerPageHtml,
                popupPageHtml,
                popupPath,
                "tryOpenWindow()");
        TestAwContentsClient.OnCreateWindowHelper onCreateWindowHelper =
                mParentContentsClient.getOnCreateWindowHelper();
        Assert.assertFalse(onCreateWindowHelper.getIsUserGesture());
    }

    private static class TestWebMessageListener implements WebMessageListener {
        private LinkedBlockingQueue<Data> mQueue = new LinkedBlockingQueue<>();

        public static class Data {
            public String mMessage;
            public boolean mIsMainFrame;
            public JsReplyProxy mReplyProxy;

            public Data(String message, boolean isMainFrame, JsReplyProxy replyProxy) {
                mMessage = message;
                mIsMainFrame = isMainFrame;
                mReplyProxy = replyProxy;
            }
        }

        @Override
        public void onPostMessage(
                MessagePayload payload,
                Uri topLevelOrigin,
                Uri sourceOrigin,
                boolean isMainFrame,
                JsReplyProxy replyProxy,
                MessagePort[] ports) {
            mQueue.add(new Data(payload.getAsString(), isMainFrame, replyProxy));
        }

        public Data waitForOnPostMessage() throws Exception {
            return AwActivityTestRule.waitForNextQueueElement(mQueue);
        }
    }

    // Regression test for crbug.com/1083819.
    //
    // The setup of this test is to have an iframe inside of a main frame, give the iframe user
    // gesture, then window.open() on javascript: scheme. We are verifying that the
    // JavaScript code isn't running in the main frame's context when
    // getJavaScriptCanOpenWindowsAutomatically() is false.
    //
    // There are several steps in this test:
    // 1. Load the web page (main.html), which has an cross-origin iframe (iframe.html).
    // 2. main frame send a message to browser to establish the connection.
    // 3. iframe send a message to browser to estiablish the connection and notify the location of
    //    the |iframe_link| element.
    // 4. Click the iframe_link element to give user gesture.
    // 5. Browser waits until receives "clicked" message.
    // 6. Browser asks the iframe to call window.open() and waits for the "done" message.
    // 7. Browser asks the main frame to check if an element was injected and waits the result.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @SkipMutations(
            reason = "This test depends on a combination of AwSettings, see crbug.com/1494038")
    public void testSingleWindowModeJsInjection() throws Throwable {
        // Choose a free port which is different from |mWebServer| so they have different origins.
        TestWebServer crossOriginWebServer = TestWebServer.startAdditional();

        final String windowOpenJavaScript =
                "javascript:{"
                        + "  let elem = document.createElement('p');"
                        + "  elem.setAttribute('id', 'inject');"
                        + "  document.body.append(elem);"
                        + "}";
        final String iframeHtml =
                "<html><head>"
                        + "<script>"
                        + "  myObject.onmessage = function(e) {"
                        + "    window.open(\""
                        + windowOpenJavaScript
                        + "\");    myObject.postMessage('done');  };  window.onload = function() { "
                        + "   let link = document.getElementById('iframe_link');    let rect ="
                        + " link.getBoundingClientRect();    let message = Math.round(rect.left) +"
                        + " ';' + Math.round(rect.top) + ';';    message += Math.round(rect.right)"
                        + " + ';' + Math.round(rect.bottom);    myObject.postMessage(message);  };"
                        + "</script></head><body>  <div>I am iframe.</div>  <a href='#'"
                        + " id='iframe_link' onclick='myObject.postMessage(\"clicked\");'>   iframe"
                        + " link  </a></body></html>";
        final String iframeHtmlPath =
                crossOriginWebServer.setResponse("/iframe.html", iframeHtml, null);
        final String mainHtml =
                "<html><head><script>"
                        + "  myObject.onmessage = function(e) {"
                        + "    let elem = document.getElementById('inject');"
                        + "    if (elem) { myObject.postMessage('failed'); }"
                        + "    else { myObject.postMessage('succeed'); }"
                        + "  };"
                        + "  myObject.postMessage('init');"
                        + "</script></head><body>"
                        + "<iframe src='"
                        + iframeHtmlPath
                        + "'></iframe>"
                        + "<div>I am main frame</div>"
                        + "</body></html>";
        final String mainHtmlPath = mWebServer.setResponse("/main.html", mainHtml, null);

        TestWebMessageListener webMessageListener = new TestWebMessageListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mParentContents.getSettings().setJavaScriptEnabled(true);
                    // |false| is the default setting for setSupportMultipleWindows(), we explicitly
                    // set it to |false| here for better readability.
                    mParentContents.getSettings().setSupportMultipleWindows(false);
                    mParentContents.getSettings().setJavaScriptCanOpenWindowsAutomatically(true);
                    mParentContents.addWebMessageListener(
                            "myObject", new String[] {"*"}, webMessageListener);
                });

        // Step 1.
        mActivityTestRule.loadUrlSync(
                mParentContents, mParentContentsClient.getOnPageFinishedHelper(), mainHtmlPath);

        // Step 2 and 3, the sequence doesn't matter.
        JsReplyProxy mainFrameReplyProxy = null;
        JsReplyProxy iframeReplyProxy = null;
        Rect rect = null;
        for (int i = 0; i < 2; ++i) {
            TestWebMessageListener.Data data = webMessageListener.waitForOnPostMessage();
            if (data.mIsMainFrame) {
                // The connection between browser and main frame established.
                Assert.assertEquals("init", data.mMessage);
                mainFrameReplyProxy = data.mReplyProxy;
            } else {
                // The connection between browser and iframe established.
                iframeReplyProxy = data.mReplyProxy;
                // iframe_link location.
                String[] c = data.mMessage.split(";");
                rect =
                        new Rect(
                                Integer.parseInt(c[0]),
                                Integer.parseInt(c[1]),
                                Integer.parseInt(c[2]),
                                Integer.parseInt(c[3]));
            }
        }

        Assert.assertNotNull("rect should not be null", rect);
        Assert.assertNotNull("mainFrameReplyProxy should not be null.", mainFrameReplyProxy);
        Assert.assertNotNull("iframeReplyProxy should not be null.", iframeReplyProxy);

        // Wait for the page to finish rendering entirely before
        // attempting to click the iframe_link We need this because we're using the DOMUtils Long
        // term we plan to switch to JSUtils to avoid this
        // https://crbug.com/1334843
        mParentContentsClient.getOnPageCommitVisibleHelper().waitForOnly();

        // Step 4. Click iframe_link to give user gesture.
        DOMUtils.clickRect(mParentContents.getWebContents(), rect);

        // Step 5. Waits until the element got clicked.
        TestWebMessageListener.Data clicked = webMessageListener.waitForOnPostMessage();
        Assert.assertEquals("clicked", clicked.mMessage);

        // Step 6. Send an arbitrary message to call window.open on javascript: URI.
        iframeReplyProxy.postMessage(new MessagePayload("hello"));
        TestWebMessageListener.Data data = webMessageListener.waitForOnPostMessage();
        Assert.assertEquals("done", data.mMessage);

        // Step 7. Send an arbitrary message to trigger the check. Main frame will check if there is
        // an injected element by running |windowOpenJavaScript|.
        mainFrameReplyProxy.postMessage(new MessagePayload("hello"));

        // If |succeed| received, then there was no injection.
        TestWebMessageListener.Data data2 = webMessageListener.waitForOnPostMessage();
        Assert.assertEquals("succeed", data2.mMessage);

        // Cleanup the test web server.
        crossOriginWebServer.shutdown();
    }

    // Copied from imeTest.java.
    private void assertWaitForSelectActionBarStatus(
            boolean show, final SelectionPopupController controller) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(controller.isSelectActionBarShowing(), Matchers.is(show));
                });
    }

    private void hideSelectActionMode(final SelectionPopupController controller) {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> controller.destroySelectActionMode());
    }
}
