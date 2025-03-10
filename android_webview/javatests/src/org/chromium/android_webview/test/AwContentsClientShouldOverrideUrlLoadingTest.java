// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;
import static org.chromium.android_webview.test.AwActivityTestRule.WAIT_TIMEOUT_MS;

import android.annotation.SuppressLint;
import android.os.Build;
import android.util.Pair;

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
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.policy.AwPolicyProvider;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.android_webview.test.util.JSUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.policy.AbstractAppRestrictionsProvider;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.policy.test.PolicyData;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/** Tests for the WebViewClient.shouldOverrideUrlLoading() method. */
@DoNotBatch(reason = "This test class is historically prone to flakes.")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwContentsClientShouldOverrideUrlLoadingTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String DATA_URL = "data:text/html,<div/>";
    private static final String REDIRECT_TARGET_PATH = "/redirect_target.html";
    private static final String TITLE = "TITLE";
    private static final String TAG = "AwContentsClientShouldOverrideUrlLoadingTest";
    private static final String sEnterpriseAuthAppLinkPolicy =
            "com.android.browser:EnterpriseAuthenticationAppLinkPolicy";

    private TestWebServer mWebServer;
    private ShouldOverrideUrlLoadingClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;

    private TestAwContentsClient.ShouldOverrideUrlLoadingHelper mShouldOverrideUrlLoadingHelper;

    private static class TestDefaultContentsClient extends ShouldOverrideUrlLoadingClient {
        @Override
        public boolean hasWebViewClient() {
            return false;
        }
    }

    public AwContentsClientShouldOverrideUrlLoadingTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private static class ShouldOverrideUrlLoadingClient extends TestAwContentsClient {
        private final BlockingQueue<AwWebResourceRequest> mShouldOverrideUrlLoadingQueue =
                new LinkedBlockingQueue<>();
        private final BlockingQueue<String> mOnPageFinishedQueue = new LinkedBlockingQueue<>();

        @Override
        public boolean shouldOverrideUrlLoading(AwWebResourceRequest request) {
            boolean value = super.shouldOverrideUrlLoading(request);
            mShouldOverrideUrlLoadingQueue.offer(request);
            return value;
        }

        @Override
        public void onPageFinished(String url) {
            super.onPageFinished(url);
            mOnPageFinishedQueue.offer(url);
        }

        public AwWebResourceRequest waitForShouldOverrideUrlLoading() throws Exception {
            return AwActivityTestRule.waitForNextQueueElement(mShouldOverrideUrlLoadingQueue);
        }

        public String waitForOnPageFinished() throws Exception {
            return AwActivityTestRule.waitForNextQueueElement(mOnPageFinishedQueue);
        }
    }

    private void standardSetup() {
        setupWithProvidedContentsClient(new ShouldOverrideUrlLoadingClient());
        mShouldOverrideUrlLoadingHelper = mContentsClient.getShouldOverrideUrlLoadingHelper();
    }

    private void setupWithProvidedContentsClient(ShouldOverrideUrlLoadingClient contentsClient) {
        mContentsClient = contentsClient;
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    private void clickOnLinkUsingJs() {
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        JSUtils.clickOnLinkUsingJs(
                InstrumentationRegistry.getInstrumentation(),
                mAwContents,
                mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                "link");
    }

    // Since this value is read on the UI thread, it's simpler to set it there too.
    void setShouldOverrideUrlLoadingReturnValueOnUiThread(final boolean value) {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                mShouldOverrideUrlLoadingHelper
                                        .setShouldOverrideUrlLoadingReturnValue(value));
    }

    private String getTestPageCommonHeaders() {
        return "<title>" + TITLE + "</title> ";
    }

    private String makeHtmlPageFrom(String headers, String body) {
        return CommonResources.makeHtmlPageFrom(getTestPageCommonHeaders() + headers, body);
    }

    private String getHtmlForPageWithJsAssignLinkTo(String url) {
        return makeHtmlPageFrom(
                "",
                "<img onclick=\"location.href='"
                        + url
                        + "'\" class=\"big\" id=\"link\" /><p>Text</p>");
    }

    private String getHtmlForPageWithJsReplaceLinkTo(String url) {
        return makeHtmlPageFrom(
                "",
                "<img onclick=\"location.replace('"
                        + url
                        + "');\" class=\"big\" id=\"link\" /><p>Text</p>");
    }

    private String getHtmlForPageWithMetaRefreshRedirectTo(String url) {
        return makeHtmlPageFrom(
                "<meta http-equiv=\"refresh\" content=\"0;url=" + url + "\" />",
                "<div>Meta refresh redirect</div>");
    }

    @SuppressLint("DefaultLocale")
    private String getHtmlForPageWithJsRedirectTo(String url, String method, int timeout) {
        return makeHtmlPageFrom(
                ""
                        + "<script>"
                        + "function doRedirectAssign() {"
                        + "location.href = '"
                        + url
                        + "';"
                        + "} "
                        + "function doRedirectReplace() {"
                        + "location.replace('"
                        + url
                        + "');"
                        + "} "
                        + "</script>",
                String.format(
                        "<script>" + "setTimeout('doRedirect%s()', %d)" + "</script>",
                        method, timeout));
    }

    private String addPageToTestServer(String httpPath, String html) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        return mWebServer.setResponse(httpPath, html, headers);
    }

    private String createRedirectTargetPage() {
        return addPageToTestServer(
                REDIRECT_TARGET_PATH,
                makeHtmlPageFrom("", "<div>This is the end of the redirect chain</div>"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledOnLoadUrl() throws Throwable {
        standardSetup();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL),
                "text/html",
                false);

        Assert.assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledOnReload() throws Throwable {
        standardSetup();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL),
                "text/html",
                false);

        int callCountBeforeReload = mShouldOverrideUrlLoadingHelper.getCallCount();
        mActivityTestRule.reloadSync(mAwContents, mContentsClient.getOnPageFinishedHelper());
        Assert.assertEquals(callCountBeforeReload, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    private void waitForNavigationRunnableAndAssertTitleChanged(Runnable navigationRunnable)
            throws Exception {
        CallbackHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        final int callCount = onPageFinishedHelper.getCallCount();
        final String oldTitle = mActivityTestRule.getTitleOnUiThread(mAwContents);
        InstrumentationRegistry.getInstrumentation().runOnMainSync(navigationRunnable);
        onPageFinishedHelper.waitForCallback(callCount);
        Assert.assertFalse(oldTitle.equals(mActivityTestRule.getTitleOnUiThread(mAwContents)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledOnBackForwardNavigation() throws Throwable {
        standardSetup();
        final String[] pageTitles = new String[] {"page1", "page2", "page3"};

        for (String title : pageTitles) {
            mActivityTestRule.loadDataSync(
                    mAwContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    CommonResources.makeHtmlPageFrom("<title>" + title + "</title>", ""),
                    "text/html",
                    false);
        }
        Assert.assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(() -> mAwContents.goBack());
        Assert.assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(() -> mAwContents.goForward());
        Assert.assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(() -> mAwContents.goBackOrForward(-2));
        Assert.assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());

        waitForNavigationRunnableAndAssertTitleChanged(() -> mAwContents.goBackOrForward(1));
        Assert.assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCantBlockLoads() throws Throwable {
        standardSetup();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);

        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(getTestPageCommonHeaders(), DATA_URL),
                "text/html",
                false);

        Assert.assertEquals(TITLE, mActivityTestRule.getTitleOnUiThread(mAwContents));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledBeforeOnPageStarted() throws Throwable {
        standardSetup();
        OnPageStartedHelper onPageStartedHelper = mContentsClient.getOnPageStartedHelper();

        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL),
                "text/html",
                false);

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();
        final int onPageStartedCallCount = onPageStartedHelper.getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);
        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(onPageStartedCallCount, onPageStartedHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testDoesNotCauseOnReceivedError() throws Throwable {
        standardSetup();
        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        final int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();

        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL),
                "text/html",
                false);

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        setShouldOverrideUrlLoadingReturnValueOnUiThread(false);

        // After we load this URL we're certain that any in-flight callbacks for the previous
        // navigation have been delivered.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), DATA_URL);

        Assert.assertEquals(onReceivedErrorCount, onReceivedErrorHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForAnchorNavigations() throws Throwable {
        doTestNotCalledForAnchorNavigations(false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForAnchorNavigationsWithNonHierarchicalScheme() throws Throwable {
        doTestNotCalledForAnchorNavigations(true);
    }

    private void doTestNotCalledForAnchorNavigations(boolean useLoadData) throws Throwable {
        standardSetup();

        final String anchorLinkPath = "/anchor_link.html";
        final String anchorLinkUrl = mWebServer.getResponseUrl(anchorLinkPath);
        addPageToTestServer(
                anchorLinkPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(anchorLinkUrl + "#anchor"));

        if (useLoadData) {
            final String html =
                    CommonResources.makeHtmlPageWithSimpleLinkTo("#anchor").replace("#", "%23");
            // Loading the html via a data URI requires us to encode '#' symbols as '%23'.
            mActivityTestRule.loadDataSync(
                    mAwContents,
                    mContentsClient.getOnPageFinishedHelper(),
                    html,
                    "text/html",
                    false);
        } else {
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), anchorLinkUrl);
        }

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs();

        // After we load this URL we're certain that any in-flight callbacks for the previous
        // navigation have been delivered.
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        Assert.assertEquals(
                shouldOverrideUrlLoadingCallCount, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenLinkClicked() throws Throwable {
        standardSetup();

        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL),
                "text/html",
                false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        Assert.assertEquals(
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        Assert.assertTrue(mShouldOverrideUrlLoadingHelper.isOutermostMainFrame());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenTopLevelAboutBlankNavigation() throws Throwable {
        standardSetup();

        final String httpPath = "/page_with_about_blank_navigation";
        final String httpPathOnServer = mWebServer.getResponseUrl(httpPath);
        addPageToTestServer(
                httpPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), httpPathOnServer);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        Assert.assertEquals(
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenSelfLinkClicked() throws Throwable {
        standardSetup();

        final String httpPath = "/page_with_link_to_self.html";
        final String httpPathOnServer = mWebServer.getResponseUrl(httpPath);
        addPageToTestServer(
                httpPath, CommonResources.makeHtmlPageWithSimpleLinkTo(httpPathOnServer));

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), httpPathOnServer);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();

        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        Assert.assertEquals(
                httpPathOnServer, mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        Assert.assertTrue(mShouldOverrideUrlLoadingHelper.isOutermostMainFrame());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenNavigatingFromJavaScriptUsingAssign() throws Throwable {
        standardSetup();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String redirectTargetUrl = createRedirectTargetPage();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithJsAssignLinkTo(redirectTargetUrl),
                "text/html",
                false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledWhenNavigatingFromJavaScriptUsingReplace() throws Throwable {
        standardSetup();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        final String redirectTargetUrl = createRedirectTargetPage();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                getHtmlForPageWithJsReplaceLinkTo(redirectTargetUrl),
                "text/html",
                false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        // It's not a server-side redirect.
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        Assert.assertTrue(mShouldOverrideUrlLoadingHelper.isOutermostMainFrame());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testPassesCorrectUrl() throws Throwable {
        standardSetup();

        final String redirectTargetUrl = createRedirectTargetPage();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl),
                "text/html",
                false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        Assert.assertEquals(
                redirectTargetUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        // It's not a server-side redirect.
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        Assert.assertTrue(mShouldOverrideUrlLoadingHelper.isOutermostMainFrame());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCanIgnoreLoading() throws Throwable {
        standardSetup();

        final String redirectTargetUrl = createRedirectTargetPage();
        final String pageWithLinkToIgnorePath = "/page_with_link_to_ignore.html";
        final String pageWithLinkToIgnoreUrl =
                addPageToTestServer(
                        pageWithLinkToIgnorePath,
                        CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl));
        final String synchronizationPath = "/sync.html";
        final String synchronizationUrl =
                addPageToTestServer(
                        synchronizationPath,
                        CommonResources.makeHtmlPageWithSimpleLinkTo(redirectTargetUrl));

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithLinkToIgnoreUrl);

        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        // Some time around here true should be returned from the shouldOverrideUrlLoading
        // callback causing the navigation caused by calling clickOnLinkUsingJs to be ignored.
        // We validate this by checking which pages were loaded on the server.
        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);

        setShouldOverrideUrlLoadingReturnValueOnUiThread(false);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), synchronizationUrl);

        Assert.assertEquals(1, mWebServer.getRequestCount(pageWithLinkToIgnorePath));
        Assert.assertEquals(1, mWebServer.getRequestCount(synchronizationPath));
        Assert.assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledForUnsupportedSchemes() throws Throwable {
        standardSetup();
        final String unsupportedSchemeUrl = "foobar://resource/1";
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(unsupportedSchemeUrl),
                "text/html",
                false);

        int callCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();

        mShouldOverrideUrlLoadingHelper.waitForCallback(callCount);
        Assert.assertEquals(
                unsupportedSchemeUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        Assert.assertTrue(mShouldOverrideUrlLoadingHelper.isOutermostMainFrame());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForPostNavigations() throws Throwable {
        // The reason POST requests are excluded is BUG 155250.
        standardSetup();

        final String redirectTargetUrl = createRedirectTargetPage();
        final String postLinkUrl =
                addPageToTestServer(
                        "/page_with_post_link.html",
                        CommonResources.makeHtmlPageWithSimplePostFormTo(redirectTargetUrl));

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), postLinkUrl);

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();

        Assert.assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
        clickOnLinkUsingJs();

        // Wait for the target URL to be fetched from the server.
        AwActivityTestRule.pollInstrumentationThread(
                () -> mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1);

        // Since the targetURL was loaded from the test server it means all processing related
        // to dispatching a shouldOverrideUrlLoading callback had finished and checking the call
        // is stable.
        Assert.assertEquals(
                shouldOverrideUrlLoadingCallCount, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledFor302AfterPostNavigations() throws Throwable {
        // The reason POST requests are excluded is BUG 155250.
        standardSetup();

        final String redirectTargetUrl = createRedirectTargetPage();
        final String postToGetRedirectUrl = mWebServer.setRedirect("/302.html", redirectTargetUrl);
        final String postLinkUrl =
                addPageToTestServer(
                        "/page_with_post_link.html",
                        CommonResources.makeHtmlPageWithSimplePostFormTo(postToGetRedirectUrl));

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), postLinkUrl);

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);

        // Wait for the target URL to be fetched from the server.
        AwActivityTestRule.pollInstrumentationThread(
                () -> mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1);

        Assert.assertEquals(
                redirectTargetUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        Assert.assertTrue(mShouldOverrideUrlLoadingHelper.isRedirect());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        Assert.assertTrue(mShouldOverrideUrlLoadingHelper.isOutermostMainFrame());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testNotCalledForIframeHttpNavigations() throws Throwable {
        standardSetup();

        final String iframeRedirectTargetUrl = createRedirectTargetPage();
        final String iframeRedirectUrl =
                mWebServer.setRedirect("/302.html", iframeRedirectTargetUrl);
        final String pageWithIframeUrl =
                addPageToTestServer(
                        "/iframe_intercept.html",
                        makeHtmlPageFrom("", "<iframe src=\"" + iframeRedirectUrl + "\" />"));

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();

        Assert.assertEquals(0, mWebServer.getRequestCount(REDIRECT_TARGET_PATH));
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithIframeUrl);

        // Wait for the redirect target URL to be fetched from the server.
        AwActivityTestRule.pollInstrumentationThread(
                () -> mWebServer.getRequestCount(REDIRECT_TARGET_PATH) == 1);

        Assert.assertEquals(
                shouldOverrideUrlLoadingCallCount, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledForIframeUnsupportedSchemeNavigations() throws Throwable {
        standardSetup();

        final String unsupportedSchemeUrl = "foobar://resource/1";
        final String pageWithIframeUrl =
                addPageToTestServer(
                        "/iframe_intercept.html",
                        makeHtmlPageFrom("", "<iframe src=\"" + unsupportedSchemeUrl + "\" />"));

        final int shouldOverrideUrlLoadingCallCount =
                mShouldOverrideUrlLoadingHelper.getCallCount();

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithIframeUrl);

        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                unsupportedSchemeUrl,
                mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.isRedirect());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.hasUserGesture());
        Assert.assertFalse(mShouldOverrideUrlLoadingHelper.isOutermostMainFrame());
    }

    private void waitForRedirectsToFinish(String redirectUrl, String redirectTarget) {
        // Drain the onPageFinished callback queue until we get the final expected onPageFinished
        // callback.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    String url = mContentsClient.waitForOnPageFinished();
                    if (redirectUrl.equals(url)) {
                        // We will sometimes receive onPageFinished for the first page load (such as
                        // for "delayed" JavaScript redirects), but may not receive this for
                        // "immediate" JavaScript redirects or 302 server-side redirects.
                        return false;
                    }
                    if (redirectTarget.equals(url)) {
                        // This should be the last onPageFinished callback.
                        return true;
                    }
                    Assert.fail("Received an unexpected URL from onPageFinished: " + url);
                    return false; // This is unreached, but the compiler still needs a return value.
                },
                AwActivityTestRule.WAIT_TIMEOUT_MS,
                AwActivityTestRule.CHECK_INTERVAL);
    }

    /**
     * Worker method for the various redirect tests.
     *
     * <p>Calling this will first load the redirect URL built from redirectFilePath, query and
     * locationFilePath and assert that we get a override callback for the destination. The second
     * part of the test loads a page that contains a link which points at the redirect URL. We
     * expect two callbacks - one for the redirect link and another for the destination.
     */
    private void doTestCalledOnRedirect(
            String redirectUrl, String redirectTarget, boolean serverSideRedirect)
            throws Throwable {
        standardSetup();
        final String pageTitle = "doTestCalledOnRedirect page";
        final String pageWithLinkToRedirectUrl =
                addPageToTestServer(
                        "/page_with_link_to_redirect.html",
                        CommonResources.makeHtmlPageWithSimpleLinkTo(
                                "<title>" + pageTitle + "</title>", redirectUrl));
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        // There is a slight difference between navigations caused by calling load and navigations
        // caused by clicking on a link:
        //
        //  * when using load the navigation is treated as if it came from the URL bar (has the
        //    navigation type TYPED, doesn't have the has_user_gesture flag); thus the navigation
        //    itself is not reported via shouldOverrideUrlLoading, but then if it has caused a
        //    redirect, the redirect itself is reported;
        //
        //  * when clicking on a link the navigation has the LINK type and has_user_gesture depends
        //    on whether it was a real click done by the user, or has it been done by JS; on click,
        //    both the initial navigation and the redirect are reported via
        //    shouldOverrideUrlLoading.
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), redirectUrl);
        AwWebResourceRequest request = mContentsClient.waitForShouldOverrideUrlLoading();
        Assert.assertEquals(redirectTarget, request.url);
        Assert.assertEquals(serverSideRedirect, request.isRedirect);
        Assert.assertFalse(request.hasUserGesture);
        Assert.assertTrue(request.isOutermostMainFrame);
        waitForRedirectsToFinish(redirectUrl, redirectTarget);

        // Test clicking with JS, hasUserGesture must be false.
        int indirectLoadCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithLinkToRedirectUrl);
        // This assertion is redundant because loadUrlSync already waits for onPageFinished,
        // however we still need to call waitForOnPageFinished() in order to drain the queue.
        Assert.assertEquals(
                "Expected onPageFinished for pageWithLinkToRedirectUrl",
                pageWithLinkToRedirectUrl,
                mContentsClient.waitForOnPageFinished());
        Assert.assertEquals(
                "shouldOverrideUrlLoading should not be invoked during loadUrlSync",
                indirectLoadCallCount,
                mShouldOverrideUrlLoadingHelper.getCallCount());

        clickOnLinkUsingJs();

        request = mContentsClient.waitForShouldOverrideUrlLoading();
        Assert.assertEquals(redirectUrl, request.url);
        Assert.assertFalse(request.isRedirect);
        Assert.assertFalse(request.hasUserGesture);
        Assert.assertTrue(request.isOutermostMainFrame);

        request = mContentsClient.waitForShouldOverrideUrlLoading();
        Assert.assertEquals(redirectTarget, request.url);
        Assert.assertEquals(serverSideRedirect, request.isRedirect);
        Assert.assertFalse(request.hasUserGesture);
        Assert.assertTrue(request.isOutermostMainFrame);
        waitForRedirectsToFinish(redirectUrl, redirectTarget);

        indirectLoadCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithLinkToRedirectUrl);
        // This assertion is redundant because loadUrlSync already waits for onPageFinished,
        // however we still need to call waitForOnPageFinished() in order to drain the queue.
        Assert.assertEquals(
                "Expected onPageFinished for pageWithLinkToRedirectUrl",
                pageWithLinkToRedirectUrl,
                mContentsClient.waitForOnPageFinished());
        mActivityTestRule.pollUiThread(() -> mAwContents.getTitle().equals(pageTitle));
        Assert.assertEquals(
                "shouldOverrideUrlLoading should not be invoked during loadUrlSync",
                indirectLoadCallCount,
                mShouldOverrideUrlLoadingHelper.getCallCount());

        // Simulate touch, hasUserGesture must be true only on the first call.
        JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "link");

        request = mContentsClient.waitForShouldOverrideUrlLoading();
        Assert.assertEquals(redirectUrl, request.url);
        Assert.assertFalse(request.isRedirect);
        Assert.assertTrue(request.hasUserGesture);
        Assert.assertTrue(request.isOutermostMainFrame);

        request = mContentsClient.waitForShouldOverrideUrlLoading();
        Assert.assertEquals(redirectTarget, request.url);
        Assert.assertEquals(serverSideRedirect, request.isRedirect);
        Assert.assertFalse(request.hasUserGesture);
        Assert.assertTrue(request.isOutermostMainFrame);
        waitForRedirectsToFinish(redirectUrl, redirectTarget);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOn302Redirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl = mWebServer.setRedirect("/302.html", redirectTargetUrl);
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnMetaRefreshRedirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl =
                addPageToTestServer(
                        "/meta_refresh.html",
                        getHtmlForPageWithMetaRefreshRedirectTo(redirectTargetUrl));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationImmediateAssignRedirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl =
                addPageToTestServer(
                        "/js_immediate_assign.html",
                        getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Assign", 0));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationImmediateReplaceRedirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl =
                addPageToTestServer(
                        "/js_immediate_replace.html",
                        getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Replace", 0));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationDelayedAssignRedirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl =
                addPageToTestServer(
                        "/js_delayed_assign.html",
                        getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Assign", 100));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testCalledOnJavaScriptLocationDelayedReplaceRedirect() throws Throwable {
        final String redirectTargetUrl = createRedirectTargetPage();
        final String redirectUrl =
                addPageToTestServer(
                        "/js_delayed_replace.html",
                        getHtmlForPageWithJsRedirectTo(redirectTargetUrl, "Replace", 100));
        doTestCalledOnRedirect(redirectUrl, redirectTargetUrl, false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testDoubleNavigateDoesNotSuppressInitialNavigate() throws Throwable {
        final String jsUrl = "javascript:try{console.log('processed js loadUrl');}catch(e){};";
        standardSetup();

        // Do a double navigagtion, the second being an effective no-op, in quick succession (i.e.
        // without yielding the main thread inbetween).
        int currentCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mAwContents.loadUrl(
                                    LoadUrlParams.createLoadDataParams(
                                            CommonResources.makeHtmlPageWithSimpleLinkTo(DATA_URL),
                                            "text/html",
                                            false));
                            mAwContents.loadUrl(new LoadUrlParams(jsUrl));
                        });
        mContentsClient
                .getOnPageFinishedHelper()
                .waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

        Assert.assertEquals(0, mShouldOverrideUrlLoadingHelper.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCallDestroyInCallback() throws Throwable {
        class DestroyInCallbackClient extends ShouldOverrideUrlLoadingClient {
            @Override
            public boolean shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest request) {
                mAwContents.destroy();
                return super.shouldOverrideUrlLoading(request);
            }
        }

        setupWithProvidedContentsClient(new DestroyInCallbackClient());
        mShouldOverrideUrlLoadingHelper = mContentsClient.getShouldOverrideUrlLoadingHelper();

        OnReceivedErrorHelper onReceivedErrorHelper = mContentsClient.getOnReceivedErrorHelper();
        int onReceivedErrorCount = onReceivedErrorHelper.getCallCount();

        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo(
                        ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL),
                "text/html",
                false);

        int shouldOverrideUrlLoadingCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);

        mActivityTestRule.pollUiThread(() -> AwContents.getNativeInstanceCount() == 0);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Navigation"})
    public void testReloadingUrlDoesNotBreakBackForwardList() throws Throwable {
        class ReloadInCallbackClient extends ShouldOverrideUrlLoadingClient {
            @Override
            public boolean shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest request) {
                super.shouldOverrideUrlLoading(request);
                mAwContents.loadUrl(request.url);
                return true;
            }
        }

        setupWithProvidedContentsClient(new ReloadInCallbackClient());
        mShouldOverrideUrlLoadingHelper = mContentsClient.getShouldOverrideUrlLoadingHelper();
        int shouldOverrideUrlLoadingCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();

        final String linkUrl =
                addPageToTestServer("/foo.html", "<html><body>hello world</body></html>");
        final String html = CommonResources.makeHtmlPageWithSimpleLinkTo(linkUrl);
        final String firstUrl = addPageToTestServer("/first.html", html);
        CallbackHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, firstUrl);

        int pageFinishedCount = onPageFinishedHelper.getCallCount();
        clickOnLinkUsingJs();
        onPageFinishedHelper.waitForCallback(pageFinishedCount);
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        Assert.assertEquals(
                linkUrl, mShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());

        Assert.assertEquals(new GURL(linkUrl), mAwContents.getUrl());
        Assert.assertTrue("Should have a navigation history", mAwContents.canGoBack());
        NavigationHistory navHistory = mAwContents.getNavigationHistory();
        Assert.assertEquals(2, navHistory.getEntryCount());
        Assert.assertEquals(1, navHistory.getCurrentEntryIndex());
        Assert.assertEquals(linkUrl, navHistory.getEntryAtIndex(1).getUrl().getSpec());

        pageFinishedCount = onPageFinishedHelper.getCallCount();
        shouldOverrideUrlLoadingCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mAwContents.goBack());
        onPageFinishedHelper.waitForCallback(pageFinishedCount);
        Assert.assertEquals(
                "Should not invoke shouldOverrideUrlLoading() for history navigation",
                shouldOverrideUrlLoadingCallCount,
                mShouldOverrideUrlLoadingHelper.getCallCount());

        Assert.assertFalse("Should not be able to navigate backward", mAwContents.canGoBack());
        Assert.assertEquals(new GURL(firstUrl), mAwContents.getUrl());
        navHistory = mAwContents.getNavigationHistory();
        Assert.assertEquals(2, navHistory.getEntryCount());
        Assert.assertEquals(0, navHistory.getCurrentEntryIndex());
        Assert.assertEquals(firstUrl, navHistory.getEntryAtIndex(0).getUrl().getSpec());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCallStopAndLoadJsInCallback() throws Throwable {
        final String globalJsVar = "window.testCallStopAndLoadJsInCallback";
        class StopInCallbackClient extends ShouldOverrideUrlLoadingClient {
            @Override
            public boolean shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest request) {
                mAwContents.stopLoading();
                mAwContents.loadUrl("javascript:" + globalJsVar + "= 1;");
                return super.shouldOverrideUrlLoading(request);
            }
        }

        setupWithProvidedContentsClient(new StopInCallbackClient());
        mShouldOverrideUrlLoadingHelper = mContentsClient.getShouldOverrideUrlLoadingHelper();

        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo("http://foo.com"),
                "text/html",
                false);

        int shouldOverrideUrlLoadingCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String actual =
                                JSUtils.executeJavaScriptAndWaitForResult(
                                        InstrumentationRegistry.getInstrumentation(), mAwContents,
                                        mContentsClient.getOnEvaluateJavaScriptResultHelper(),
                                                globalJsVar);
                        Criteria.checkThat(actual, Matchers.is("1"));
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCallLoadInCallback() throws Throwable {
        final String httpPath = "/page_with_about_blank_navigation";
        final String httpPathOnServer = mWebServer.getResponseUrl(httpPath);
        addPageToTestServer(
                httpPath,
                CommonResources.makeHtmlPageWithSimpleLinkTo(
                        getTestPageCommonHeaders(), ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));
        class StopInCallbackClient extends ShouldOverrideUrlLoadingClient {
            @Override
            public boolean shouldOverrideUrlLoading(AwContentsClient.AwWebResourceRequest request) {
                mAwContents.loadUrl(httpPathOnServer);
                return super.shouldOverrideUrlLoading(request);
            }
        }
        setupWithProvidedContentsClient(new StopInCallbackClient());
        mShouldOverrideUrlLoadingHelper = mContentsClient.getShouldOverrideUrlLoadingHelper();
        mActivityTestRule.loadDataSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                CommonResources.makeHtmlPageWithSimpleLinkTo("http://foo.com"),
                "text/html",
                false);
        int shouldOverrideUrlLoadingCallCount = mShouldOverrideUrlLoadingHelper.getCallCount();
        int onPageFinishedCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        setShouldOverrideUrlLoadingReturnValueOnUiThread(true);
        clickOnLinkUsingJs();
        mShouldOverrideUrlLoadingHelper.waitForCallback(shouldOverrideUrlLoadingCallCount);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(onPageFinishedCallCount);

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                mActivityTestRule.getTitleOnUiThread(mAwContents),
                                Matchers.is(TITLE));
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullContentsClientWithServerRedirect() throws Throwable {
        try {
            // The test will fire real intents through the test activity.
            // Need to temporarily suppress startActivity otherwise there will be a
            // handler selection window and the test can't dismiss that.
            mActivityTestRule.getActivity().setIgnoreStartActivity(true);
            final String testUrl =
                    mWebServer.setResponse(
                            "/" + CommonResources.ABOUT_FILENAME,
                            CommonResources.ABOUT_HTML,
                            CommonResources.getTextHtmlHeaders(true));
            setupWithProvidedContentsClient(new TestDefaultContentsClient());
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), testUrl);

            Assert.assertNull(mActivityTestRule.getActivity().getLastSentIntent());

            // Now the server will redirect path1 to path2. Path2 will load ABOUT_HTML.
            // AwContents should create an intent for the server initiated redirection.
            final String path1 = "/from.html";
            final String path2 = "/to.html";
            final String fromUrl = mWebServer.setRedirect(path1, path2);
            final String toUrl =
                    mWebServer.setResponse(
                            path2,
                            CommonResources.ABOUT_HTML,
                            CommonResources.getTextHtmlHeaders(true));
            mActivityTestRule.loadUrlAsync(mAwContents, fromUrl);

            mActivityTestRule.pollUiThread(
                    () -> mActivityTestRule.getActivity().getLastSentIntent() != null);
            Assert.assertEquals(
                    toUrl,
                    mActivityTestRule.getActivity().getLastSentIntent().getData().toString());
        } finally {
            mActivityTestRule.getActivity().setIgnoreStartActivity(false);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullContentsClientOpenAboutUrlInWebView() throws Throwable {
        try {
            // If there's a bug in WebView, this may fire real intents through the test activity.
            // Need to temporarily suppress startActivity otherwise there will be a
            // handler selection window and the test can't dismiss that.
            mActivityTestRule.getActivity().setIgnoreStartActivity(true);
            setupWithProvidedContentsClient(new TestDefaultContentsClient());
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String pageTitle = "Click Title";
            final String htmlWithLink =
                    "<html><title>"
                            + pageTitle
                            + "</title>"
                            + "<body><a id='link' href='"
                            + ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL
                            + "'>Click this!</a></body></html>";
            final String urlWithLink =
                    mWebServer.setResponse(
                            "/html_with_link.html",
                            htmlWithLink,
                            CommonResources.getTextHtmlHeaders(true));

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), urlWithLink);

            // Clicking on an about:blank link should always navigate to the page directly
            int currentCallCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
            JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "link");
            mContentsClient
                    .getOnPageFinishedHelper()
                    .waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);

            Assert.assertEquals(
                    new GURL(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL), mAwContents.getUrl());
        } finally {
            mActivityTestRule.getActivity().setIgnoreStartActivity(false);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNullContentsClientOpenLink() throws Throwable {
        try {
            // The test will fire real intents through the test activity.
            // Need to temporarily suppress startActivity otherwise there will be a
            // handler selection window and the test can't dismiss that.
            mActivityTestRule.getActivity().setIgnoreStartActivity(true);
            final String testUrl =
                    mWebServer.setResponse(
                            "/" + CommonResources.ABOUT_FILENAME,
                            CommonResources.ABOUT_HTML,
                            CommonResources.getTextHtmlHeaders(true));
            setupWithProvidedContentsClient(new TestDefaultContentsClient());
            AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
            final String pageTitle = "Click Title";
            final String htmlWithLink =
                    "<html><title>"
                            + pageTitle
                            + "</title>"
                            + "<body><a id='link' href='"
                            + testUrl
                            + "'>Click this!</a></body></html>";
            final String urlWithLink =
                    mWebServer.setResponse(
                            "/html_with_link.html",
                            htmlWithLink,
                            CommonResources.getTextHtmlHeaders(true));

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), urlWithLink);
            // Executing JS code that tries to navigate somewhere should not create an intent.
            Assert.assertEquals(
                    "\"" + testUrl + "\"",
                    JSUtils.executeJavaScriptAndWaitForResult(
                            InstrumentationRegistry.getInstrumentation(),
                            mAwContents,
                            new OnEvaluateJavaScriptResultHelper(),
                            "document.location.href='" + testUrl + "'"));
            Assert.assertNull(mActivityTestRule.getActivity().getLastSentIntent());

            // Clicking on a link should create an intent.
            JSUtils.clickNodeWithUserGesture(mAwContents.getWebContents(), "link");
            mActivityTestRule.pollUiThread(
                    () -> mActivityTestRule.getActivity().getLastSentIntent() != null);
            Assert.assertEquals(
                    testUrl,
                    mActivityTestRule.getActivity().getLastSentIntent().getData().toString());
        } finally {
            mActivityTestRule.getActivity().setIgnoreStartActivity(false);
        }
    }

    private void setAppLinkPolicy(final AwPolicyProvider testProvider, String url) {
        final PolicyData[] policies = {
            new PolicyData.Str(sEnterpriseAuthAppLinkPolicy, "[{ \"url\": \"" + url + "\"}]")
        };

        AbstractAppRestrictionsProvider.setTestRestrictions(
                PolicyData.asBundle(Arrays.asList(policies)));

        ThreadUtils.runOnUiThreadBlocking(() -> testProvider.refresh());

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @Feature({"AndroidWebView"})
    public void testForAuthenticationUrlIntentSent() throws Throwable {
        try {
            standardSetup();
            mActivityTestRule.getActivity().setIgnoreStartActivity(true);
            AwSettings contentSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);

            final AwPolicyProvider testProvider =
                    new AwPolicyProvider(mActivityTestRule.getActivity().getApplicationContext());
            ThreadUtils.runOnUiThreadBlocking(
                    () -> CombinedPolicyProvider.get().registerProvider(testProvider));

            final String authenticationUrl =
                    addPageToTestServer(
                            "/redirect" + REDIRECT_TARGET_PATH,
                            makeHtmlPageFrom(
                                    "", "<div>This is the end of the redirect chain</div>"));
            final String loginUrl = mWebServer.setRedirect("/login.html", authenticationUrl);
            // Set the policy for authentication url.
            setAppLinkPolicy(testProvider, authenticationUrl);

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), loginUrl);

            mActivityTestRule.pollUiThread(
                    () -> mActivityTestRule.getActivity().getLastSentIntent() != null);
            Assert.assertEquals(
                    authenticationUrl,
                    mActivityTestRule.getActivity().getLastSentIntent().getData().toString());
        } finally {
            mActivityTestRule.getActivity().setIgnoreStartActivity(false);
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWithoutPolicyForAuthenticationUrlIntentNotSent() throws Throwable {
        try {
            standardSetup();
            mActivityTestRule.getActivity().setIgnoreStartActivity(true);
            AwSettings contentSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);
            final AwPolicyProvider testProvider =
                    new AwPolicyProvider(mActivityTestRule.getActivity().getApplicationContext());
            ThreadUtils.runOnUiThreadBlocking(
                    () -> CombinedPolicyProvider.get().registerProvider(testProvider));
            final String authenticationUrl =
                    addPageToTestServer(
                            "/redirect" + REDIRECT_TARGET_PATH,
                            makeHtmlPageFrom(
                                    "", "<div>This is the end of the redirect chain</div>"));
            final String loginUrl = mWebServer.setRedirect("/login.html", authenticationUrl);

            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), loginUrl);

            Assert.assertNull(mActivityTestRule.getActivity().getLastSentIntent());
        } finally {
            mActivityTestRule.getActivity().setIgnoreStartActivity(false);
        }
    }

    // Verify popups can open about:blank but no shouldoverrideurloading is received for about:blank
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWindowOpenAboutBlankInPopup() throws Throwable {
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper popupShouldOverrideUrlLoadingHelper =
                createPopUp("about:blank", /* waitForTitle= */ true);
        // Popup is just created, so testing against 0 is true.
        Assert.assertEquals(0, popupShouldOverrideUrlLoadingHelper.getCallCount());
    }

    // Verify popups can open custom scheme and shouldoverrideurlloading is called.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWindowOpenCustomSchemeUrlInPopup() throws Throwable {
        final String popupPath = "foo://bar";
        verifyShouldOverrideUrlLoadingInPopup(popupPath);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWindowOpenHttpUrlInPopup() throws Throwable {
        final String popupPath = "http://example.com/";
        verifyShouldOverrideUrlLoadingInPopup(popupPath);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testWindowOpenHttpUrlInPopupAddsTrailingSlash() throws Throwable {
        final String popupPath = "http://example.com";
        verifyShouldOverrideUrlLoadingInPopup(popupPath, popupPath + "/");
    }

    private static final String BAD_SCHEME = "badscheme://";

    // AwContentsClient handling an invalid network scheme
    private static class BadSchemeClient extends ShouldOverrideUrlLoadingClient {
        CountDownLatch mLatch = new CountDownLatch(1);

        @Override
        public boolean shouldOverrideUrlLoading(AwWebResourceRequest request) {
            if (request.url.startsWith(BAD_SCHEME)) {
                mLatch.countDown();
                return true;
            }
            return false;
        }

        @Override
        public void onReceivedError(AwWebResourceRequest request, AwWebResourceError error) {
            super.onReceivedError(request, error);
            throw new RuntimeException("we should not receive an error code! " + request.url);
        }

        public void waitForLatch() {
            try {
                Assert.assertTrue(mLatch.await(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCalledOnServerRedirectInvalidScheme() throws Throwable {
        BadSchemeClient client = new BadSchemeClient();
        setupWithProvidedContentsClient(client);

        final String path1 = "/from.html";
        final String path2 = BAD_SCHEME + "to.html";
        final String fromUrl = mWebServer.setRedirect(path1, path2);
        final String toUrl =
                mWebServer.setResponse(
                        path2,
                        CommonResources.ABOUT_HTML,
                        CommonResources.getTextHtmlHeaders(true));
        mActivityTestRule.loadUrlAsync(mAwContents, fromUrl);
        client.waitForLatch();
        // Wait for an arbitrary amount of time to ensure onReceivedError is never called.
        Thread.sleep(SCALED_WAIT_TIMEOUT_MS / 3);
    }

    private void verifyShouldOverrideUrlLoadingInPopup(String popupPath) throws Throwable {
        verifyShouldOverrideUrlLoadingInPopup(popupPath, popupPath);
    }

    private void verifyShouldOverrideUrlLoadingInPopup(
            String popupPath, String expectedPathInShouldOVerrideUrlLoading) throws Throwable {
        TestAwContentsClient.ShouldOverrideUrlLoadingHelper popupShouldOverrideUrlLoadingHelper =
                createPopUp(popupPath, /* waitForTitle= */ false);
        Assert.assertEquals(
                expectedPathInShouldOVerrideUrlLoading,
                popupShouldOverrideUrlLoadingHelper.getShouldOverrideUrlLoadingUrl());
        Assert.assertEquals(false, popupShouldOverrideUrlLoadingHelper.isRedirect());
        Assert.assertFalse(popupShouldOverrideUrlLoadingHelper.hasUserGesture());
        Assert.assertTrue(popupShouldOverrideUrlLoadingHelper.isOutermostMainFrame());
    }

    private TestAwContentsClient.ShouldOverrideUrlLoadingHelper createPopUp(
            String popupPath, boolean waitForTitle) throws Throwable {
        standardSetup();
        final String parentPageHtml =
                CommonResources.makeHtmlPageFrom(
                        "",
                        "<script>"
                                + "function tryOpenWindow() {"
                                + "  var newWindow = window.open('"
                                + popupPath
                                + "');"
                                + "}</script>");
        mActivityTestRule.triggerPopup(
                mAwContents,
                mContentsClient,
                mWebServer,
                parentPageHtml,
                null,
                null,
                "tryOpenWindow()");

        final TestAwContentsClient popupContentsClient = new TestAwContentsClient();
        final AwTestContainerView popupContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(popupContentsClient);
        final AwContents popupContents = popupContainerView.getAwContents();

        TestAwContentsClient.ShouldOverrideUrlLoadingHelper popupShouldOverrideUrlLoadingHelper =
                popupContentsClient.getShouldOverrideUrlLoadingHelper();
        int currentCallCount = popupShouldOverrideUrlLoadingHelper.getCallCount();

        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mAwContents.supplyContentsForPopup(popupContents));

        if (waitForTitle) {
            // Wait for popup to be loaded for about:blank. Turned out that in about:blank
            // navigation to open a popup, both WebviewClient and WebChromeClient callbacks such as
            // OnPageFinished, OnReceivedTitle, onPageStarted, are not called. However,
            // title changes.
            pollTitleAs("about:blank", popupContents);
        } else {
            popupContentsClient
                    .getOnPageFinishedHelper()
                    .waitForCallback(currentCallCount, 1, WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        }
        return popupShouldOverrideUrlLoadingHelper;
    }

    private void pollTitleAs(final String title, final AwContents awContents) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> title.equals(mActivityTestRule.getTitleOnUiThread(awContents)));
    }
}
