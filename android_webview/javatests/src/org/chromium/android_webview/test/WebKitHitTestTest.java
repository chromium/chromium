// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.webkit.WebView.HitTestResult;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;
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
import org.chromium.android_webview.test.util.AwTestTouchUtils;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageCommitVisibleHelper;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.TimeUnit;

/** Test for getHitTestResult, requestFocusNodeHref, and requestImageRef methods */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class WebKitHitTestTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mTestView;
    private AwContents mAwContents;
    private TestWebServer mWebServer;

    private static final String HREF = "http://foo/";
    private static final String ANCHOR_TEXT = "anchor text";
    private int mServerResponseCount;

    public WebKitHitTestTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mContentsClient = new TestAwContentsClient();
        mTestView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestView.getAwContents();
        mWebServer = TestWebServer.start();
        final String imagePath = "/" + CommonResources.TEST_IMAGE_FILENAME;
        mWebServer.setResponseBase64(
                imagePath,
                CommonResources.FAVICON_DATA_BASE64,
                CommonResources.getImagePngHeaders(true));
    }

    @After
    public void tearDown() {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    private String setServerResponseAndLoad(String response) throws Throwable {
        // Use a different path each time to avoid flakes due to caching.
        String path = "/hittest" + mServerResponseCount++ + ".html";
        String url = mWebServer.setResponse(path, response, null);
        OnPageCommitVisibleHelper commitHelper = mContentsClient.getOnPageCommitVisibleHelper();
        int currentCallCount = commitHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        commitHelper.waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        return url;
    }

    private static String fullPageLink(String href, String anchorText) {
        return CommonResources.makeHtmlPageFrom(
                "",
                "<a class=\"full_view\" href=\""
                        + href
                        + "\" "
                        + "onclick=\"return false;\">"
                        + anchorText
                        + "</a>");
    }

    private void simulateTabDownUpOnUiThread() {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            long eventTime = SystemClock.uptimeMillis();
                            mAwContents
                                    .getWebContents()
                                    .getEventForwarder()
                                    .dispatchKeyEvent(
                                            new KeyEvent(
                                                    eventTime,
                                                    eventTime,
                                                    KeyEvent.ACTION_DOWN,
                                                    KeyEvent.KEYCODE_TAB,
                                                    0));
                            mAwContents
                                    .getWebContents()
                                    .getEventForwarder()
                                    .dispatchKeyEvent(
                                            new KeyEvent(
                                                    eventTime,
                                                    eventTime,
                                                    KeyEvent.ACTION_UP,
                                                    KeyEvent.KEYCODE_TAB,
                                                    0));
                        });
    }

    private void simulateInput(boolean byTouch) {
        // Send a touch click event if byTouch is true. Otherwise, send a TAB
        // key event to change the focused element of the page.
        if (byTouch) {
            AwTestTouchUtils.simulateTouchCenterOfView(mTestView);
        } else {
            simulateTabDownUpOnUiThread();
        }
    }

    private static boolean stringEquals(String a, String b) {
        return a == null ? b == null : a.equals(b);
    }

    private void pollForHitTestDataOnUiThread(final int expectedType, final String expectedExtra) {
        mActivityTestRule.pollUiThread(
                () -> {
                    AwContents.HitTestData data = mAwContents.getLastHitTestResult();
                    return expectedType == data.hitTestResultType
                            && stringEquals(expectedExtra, data.hitTestResultExtraData);
                });
    }

    private void pollForHrefAndImageSrcOnUiThread(
            final String expectedHref,
            final String expectedAnchorText,
            final String expectedImageSrc) {
        mActivityTestRule.pollUiThread(
                () -> {
                    AwContents.HitTestData data = mAwContents.getLastHitTestResult();
                    return stringEquals(expectedHref, data.href)
                            && stringEquals(expectedAnchorText, data.anchorText)
                            && stringEquals(expectedImageSrc, data.imgSrc);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Handler placeholderHandler = new Handler();
                    Message focusNodeHrefMsg = placeholderHandler.obtainMessage();
                    Message imageRefMsg = placeholderHandler.obtainMessage();

                    mAwContents.requestFocusNodeHref(focusNodeHrefMsg);
                    mAwContents.requestImageRef(imageRefMsg);

                    Assert.assertEquals(expectedHref, focusNodeHrefMsg.getData().getString("url"));
                    Assert.assertEquals(
                            expectedAnchorText, focusNodeHrefMsg.getData().getString("title"));
                    Assert.assertEquals(
                            expectedImageSrc, focusNodeHrefMsg.getData().getString("src"));
                    Assert.assertEquals(expectedImageSrc, imageRefMsg.getData().getString("url"));
                });
    }

    private void srcAnchorTypeTestBody(boolean byTouch) throws Throwable {
        String page = fullPageLink(HREF, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_ANCHOR_TYPE, HREF);
        pollForHrefAndImageSrcOnUiThread(HREF, ANCHOR_TEXT, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorType() throws Throwable {
        srcAnchorTypeTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeByFocus() throws Throwable {
        srcAnchorTypeTestBody(false);
    }

    private void blankHrefTestBody(boolean byTouch) throws Throwable {
        String page = fullPageLink("", ANCHOR_TEXT);
        String fullPath = setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_ANCHOR_TYPE, fullPath);
        pollForHrefAndImageSrcOnUiThread(fullPath, ANCHOR_TEXT, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeBlankHref() throws Throwable {
        blankHrefTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeBlankHrefByFocus() throws Throwable {
        blankHrefTestBody(false);
    }

    private void srcAnchorTypeRelativeUrlTestBody(boolean byTouch) throws Throwable {
        String relPath = "/foo.html";
        String fullPath = mWebServer.getResponseUrl(relPath);
        String page = fullPageLink(relPath, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_ANCHOR_TYPE, fullPath);
        pollForHrefAndImageSrcOnUiThread(fullPath, ANCHOR_TEXT, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeRelativeUrl() throws Throwable {
        srcAnchorTypeRelativeUrlTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcAnchorTypeRelativeUrlByFocus() throws Throwable {
        srcAnchorTypeRelativeUrlTestBody(false);
    }

    private void srcEmailTypeTestBody(boolean byTouch) throws Throwable {
        String email = "foo@bar.com";
        String prefix = "mailto:";
        String page = fullPageLink(prefix + email, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.EMAIL_TYPE, email);
        pollForHrefAndImageSrcOnUiThread(prefix + email, ANCHOR_TEXT, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcEmailType() throws Throwable {
        srcEmailTypeTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcEmailTypeByFocus() throws Throwable {
        srcEmailTypeTestBody(false);
    }

    private void srcGeoTypeTestBody(boolean byTouch) throws Throwable {
        String location = "Jilin";
        String prefix = "geo:0,0?q=";
        String page = fullPageLink(prefix + location, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.GEO_TYPE, location);
        pollForHrefAndImageSrcOnUiThread(prefix + location, ANCHOR_TEXT, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcGeoType() throws Throwable {
        srcGeoTypeTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcGeoTypeByFocus() throws Throwable {
        srcGeoTypeTestBody(false);
    }

    private void srcPhoneTypeTestBody(boolean byTouch) throws Throwable {
        String phone_num = "%2B1234567890";
        String expected_phone_num = "+1234567890";
        String prefix = "tel:";
        String page = fullPageLink("tel:" + phone_num, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.PHONE_TYPE, expected_phone_num);
        pollForHrefAndImageSrcOnUiThread(prefix + phone_num, ANCHOR_TEXT, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcPhoneType() throws Throwable {
        srcPhoneTypeTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcPhoneTypeByFocus() throws Throwable {
        srcPhoneTypeTestBody(false);
    }

    private void srcImgeAnchorTypeTestBody(boolean byTouch) throws Throwable {
        String fullImageSrc = "http://foo.bar/nonexistent.jpg";
        String page =
                CommonResources.makeHtmlPageFrom(
                        "",
                        "<a class=\"full_view\" href=\""
                                + HREF
                                + "\"onclick=\"return false;\"><img class=\"full_view\" src=\""
                                + fullImageSrc
                                + "\"></a>");
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_IMAGE_ANCHOR_TYPE, fullImageSrc);
        pollForHrefAndImageSrcOnUiThread(HREF, null, fullImageSrc);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorType() throws Throwable {
        srcImgeAnchorTypeTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorTypeByFocus() throws Throwable {
        srcImgeAnchorTypeTestBody(false);
    }

    private void srcImgeAnchorTypeRelativeUrlTestBody(boolean byTouch) throws Throwable {
        String relImageSrc = "/nonexistent.jpg";
        String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
        String relPath = "/foo.html";
        String fullPath = mWebServer.getResponseUrl(relPath);
        String page =
                CommonResources.makeHtmlPageFrom(
                        "",
                        "<a class=\"full_view\" href=\""
                                + relPath
                                + "\"onclick=\"return false;\"><img class=\"full_view\" src=\""
                                + relImageSrc
                                + "\"></a>");
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.SRC_IMAGE_ANCHOR_TYPE, fullImageSrc);
        pollForHrefAndImageSrcOnUiThread(fullPath, null, fullImageSrc);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorTypeRelativeUrl() throws Throwable {
        srcImgeAnchorTypeRelativeUrlTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testSrcImgeAnchorTypeRelativeUrlByFocus() throws Throwable {
        srcImgeAnchorTypeRelativeUrlTestBody(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testImgeType() throws Throwable {
        String relImageSrc = "/" + CommonResources.TEST_IMAGE_FILENAME;
        String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
        String page =
                CommonResources.makeHtmlPageFrom(
                        "", "<img class=\"full_view\" src=\"" + relImageSrc + "\">");
        setServerResponseAndLoad(page);
        AwTestTouchUtils.simulateTouchCenterOfView(mTestView);
        pollForHitTestDataOnUiThread(HitTestResult.IMAGE_TYPE, fullImageSrc);
        pollForHrefAndImageSrcOnUiThread(null, null, fullImageSrc);
    }

    private void editTextTypeTestBody(boolean byTouch) throws Throwable {
        String page =
                CommonResources.makeHtmlPageFrom(
                        "", "<form><input class=\"full_view\" type=\"text\" name=\"test\"></form>");
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHitTestDataOnUiThread(HitTestResult.EDIT_TEXT_TYPE, null);
        pollForHrefAndImageSrcOnUiThread(null, null, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testEditTextType() throws Throwable {
        editTextTypeTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testEditTextTypeByFocus() throws Throwable {
        editTextTypeTestBody(false);
    }

    public void unknownTypeJavascriptSchemeTestBody(boolean byTouch) throws Throwable {
        // Per documentation, javascript urls are special.
        String javascript = "javascript:alert('foo');";
        String page = fullPageLink(javascript, ANCHOR_TEXT);
        setServerResponseAndLoad(page);
        simulateInput(byTouch);
        pollForHrefAndImageSrcOnUiThread(javascript, ANCHOR_TEXT, null);
        pollForHitTestDataOnUiThread(HitTestResult.UNKNOWN_TYPE, null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnknownTypeJavascriptScheme() throws Throwable {
        unknownTypeJavascriptSchemeTestBody(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnknownTypeJavascriptSchemeByFocus() throws Throwable {
        unknownTypeJavascriptSchemeTestBody(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    @DisabledTest(message = "https://crbug.com/1172381")
    public void testUnknownTypeUnrecognizedNode() throws Throwable {
        // Since UNKNOWN_TYPE is the default, hit test another type first for
        // this test to be valid.
        testSrcAnchorType();

        final String title = "UNKNOWN_TYPE title";

        String page =
                CommonResources.makeHtmlPageFrom(
                        "<title>" + title + "</title>", "<div class=\"full_view\">div text</div>");
        setServerResponseAndLoad(page);
        AwTestTouchUtils.simulateTouchCenterOfView(mTestView);
        pollForHitTestDataOnUiThread(HitTestResult.UNKNOWN_TYPE, null);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView", "WebKitHitTest"})
    public void testUnfocusedNodeAndTouchRace() throws Throwable {
        // Test when the touch and focus paths racing with setting different
        // results.

        String relImageSrc = "/" + CommonResources.TEST_IMAGE_FILENAME;
        String fullImageSrc = mWebServer.getResponseUrl(relImageSrc);
        String html =
                CommonResources.makeHtmlPageFrom(
                        "<meta name=\"viewport\""
                            + " content=\"width=device-width,height=device-height\" /><style"
                            + " type=\"text/css\">.full_width { width:100%; position:absolute; }"
                            + "</style>",
                        "<form><input class=\"full_width\" style=\"height:25%;\" "
                                + "type=\"text\" name=\"test\"></form>"
                                + "<img class=\"full_width\" style=\"height:50%;top:25%;\" "
                                + "src=\""
                                + relImageSrc
                                + "\">");
        setServerResponseAndLoad(html);

        // Focus on input element and check the hit test results.
        simulateTabDownUpOnUiThread();
        pollForHitTestDataOnUiThread(HitTestResult.EDIT_TEXT_TYPE, null);
        pollForHrefAndImageSrcOnUiThread(null, null, null);

        // Touch image. Now the focus based hit test path will try to null out
        // the results and the touch based path will update with the result of
        // the image.
        AwTestTouchUtils.simulateTouchCenterOfView(mTestView);

        // Make sure the result of image sticks.
        for (int i = 0; i < 2; ++i) {
            Thread.sleep(500);
            pollForHitTestDataOnUiThread(HitTestResult.IMAGE_TYPE, fullImageSrc);
            pollForHrefAndImageSrcOnUiThread(null, null, fullImageSrc);
        }
    }
}
