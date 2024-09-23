// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.AwActivityTestRule.SCALED_WAIT_TIMEOUT_MS;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.test.AwActivityTestRule.PopupInfo;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Navigation history tests. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class NavigationHistoryTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String PAGE_1_PATH = "/page1.html";
    private static final String PAGE_1_TITLE = "Page 1 Title";
    private static final String PAGE_2_PATH = "/page2.html";
    private static final String PAGE_2_TITLE = "Page 2 Title";
    private static final String PAGE_WITH_HASHTAG_REDIRECT_TITLE = "Page with hashtag";
    private static final String PAGE_WITH_SAME_DOCUMENT = "/page3.html";
    private static final String NOTIFY_LOADED_SCRIPT =
            "<script>window.addEventListener('pageshow',(e)=>{awFullyLoadedFuture.done()});</script>";

    private TestWebServer mWebServer;
    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private TestPageLoadedNotifier mLoadedNotifier;
    private String mLoadedFutureName = "awFullyLoadedFuture";

    public NavigationHistoryTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        AwContents.setShouldDownloadFavicons();
        mContentsClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mWebServer = TestWebServer.start();
        mLoadedNotifier = new TestPageLoadedNotifier();
        mLoadedNotifier.setFuture(SettableFuture.create());
        AwActivityTestRule.addJavascriptInterfaceOnUiThread(
                mAwContents, mLoadedNotifier, mLoadedFutureName);
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private NavigationHistory getNavigationHistory(final AwContents awContents) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> awContents.getNavigationController().getNavigationHistory());
    }

    private void checkHistoryItem(
            NavigationEntry item,
            String url,
            String originalUrl,
            String title,
            boolean faviconNull) {
        Assert.assertEquals(url, item.getUrl().getSpec());
        Assert.assertEquals(originalUrl, item.getOriginalUrl().getSpec());
        Assert.assertEquals(title, item.getTitle());
        if (faviconNull) {
            Assert.assertNull(item.getFavicon());
        } else {
            Assert.assertNotNull(item.getFavicon());
        }
    }

    private String addPage1ToServer(TestWebServer webServer) {
        return mWebServer.setResponse(
                PAGE_1_PATH,
                CommonResources.makeHtmlPageFrom(
                        "<title>" + PAGE_1_TITLE + "</title>",
                        NOTIFY_LOADED_SCRIPT + "<div>This is test page 1.</div>"),
                CommonResources.getTextHtmlHeaders(false));
    }

    private String addPage2ToServer(TestWebServer webServer) {
        return mWebServer.setResponse(
                PAGE_2_PATH,
                CommonResources.makeHtmlPageFrom(
                        "<title>" + PAGE_2_TITLE + "</title>",
                        NOTIFY_LOADED_SCRIPT + "<div>This is test page 2.</div>"),
                CommonResources.getTextHtmlHeaders(false));
    }

    private String addPageWithHashTagRedirectToServer(TestWebServer webServer) {
        return mWebServer.setResponse(
                PAGE_2_PATH,
                CommonResources.makeHtmlPageFrom(
                        "<title>" + PAGE_WITH_HASHTAG_REDIRECT_TITLE + "</title>",
                        NOTIFY_LOADED_SCRIPT
                                + "<iframe onLoad=\"location.replace(location.href + '#tag');\""
                                + " />"),
                CommonResources.getTextHtmlHeaders(false));
    }

    private String addPageWithSameDocumentToServer(TestWebServer webServer) {
        return mWebServer.setResponse(
                PAGE_WITH_SAME_DOCUMENT,
                CommonResources.makeHtmlPageFrom(
                        "<script>history.pushState(null, null, '/history.html');</script>",
                        NOTIFY_LOADED_SCRIPT + "<div>This is test page with samedocument.</div>"),
                CommonResources.getTextHtmlHeaders(false));
    }

    private void navigateBack() throws Throwable {
        if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_BACK_FORWARD_CACHE)) {
            SettableFuture<Boolean> pageFullyLoadedFuture = SettableFuture.create();
            mLoadedNotifier.setFuture(pageFullyLoadedFuture);
            HistoryUtils.goBackSync(
                    InstrumentationRegistry.getInstrumentation(),
                    mAwContents.getWebContents(),
                    mContentsClient.getOnPageStartedHelper());
            // Wait for the page to be fully loaded
            Assert.assertTrue(
                    pageFullyLoadedFuture.get(SCALED_WAIT_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } else {
            HistoryUtils.goBackSync(
                    InstrumentationRegistry.getInstrumentation(),
                    mAwContents.getWebContents(),
                    mContentsClient.getOnPageFinishedHelper());
        }
    }

    @Test
    @SmallTest
    public void testNavigateOneUrl() throws Throwable {
        NavigationHistory history = getNavigationHistory(mAwContents);
        Assert.assertEquals(1, history.getEntryCount());

        final String pageWithHashTagRedirectUrl = addPageWithHashTagRedirectToServer(mWebServer);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithHashTagRedirectUrl);

        history = getNavigationHistory(mAwContents);
        checkHistoryItem(
                history.getEntryAtIndex(0),
                pageWithHashTagRedirectUrl + "#tag",
                pageWithHashTagRedirectUrl,
                PAGE_WITH_HASHTAG_REDIRECT_TITLE,
                true);

        Assert.assertEquals(0, history.getCurrentEntryIndex());
    }

    @Test
    @SmallTest
    public void testNavigateBackForwardWithIntervention() throws Throwable {
        NavigationHistory history = getNavigationHistory(mAwContents);
        Assert.assertEquals(1, history.getEntryCount());

        final String page1Url = addPage1ToServer(mWebServer);
        final String pageWithSameDocumentUrl = addPageWithSameDocumentToServer(mWebServer);
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), page1Url);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), pageWithSameDocumentUrl);

        history = getNavigationHistory(mAwContents);
        Assert.assertEquals(3, history.getEntryCount());
        Assert.assertTrue(mAwContents.canGoBackOrForward(-1));
        Assert.assertFalse(mAwContents.canGoBackOrForward(-2));

        navigateBack();

        Assert.assertTrue(mAwContents.canGoBackOrForward(1));
        Assert.assertFalse(mAwContents.canGoBackOrForward(2));
    }

    @Test
    @SmallTest
    public void testNavigateTwoUrls() throws Throwable {
        NavigationHistory list = getNavigationHistory(mAwContents);
        Assert.assertEquals(1, list.getEntryCount());

        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        final String page1Url = addPage1ToServer(mWebServer);
        final String page2Url = addPage2ToServer(mWebServer);

        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, page1Url);
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, page2Url);

        list = getNavigationHistory(mAwContents);

        // Make sure there is a new entry entry the list
        Assert.assertEquals(2, list.getEntryCount());

        // Make sure the first entry is still okay
        checkHistoryItem(list.getEntryAtIndex(0), page1Url, page1Url, PAGE_1_TITLE, true);

        // Make sure the second entry was added properly
        checkHistoryItem(list.getEntryAtIndex(1), page2Url, page2Url, PAGE_2_TITLE, true);

        Assert.assertEquals(1, list.getCurrentEntryIndex());
    }

    @Test
    @SmallTest
    public void testNavigateTwoUrlsAndBack() throws Throwable {
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        NavigationHistory list = getNavigationHistory(mAwContents);
        Assert.assertEquals(1, list.getEntryCount());

        final String page1Url = addPage1ToServer(mWebServer);
        final String page2Url = addPage2ToServer(mWebServer);

        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, page1Url);
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, page2Url);

        navigateBack();
        list = getNavigationHistory(mAwContents);

        // Make sure the first entry is still okay
        checkHistoryItem(list.getEntryAtIndex(0), page1Url, page1Url, PAGE_1_TITLE, true);

        // Make sure the second entry is still okay
        checkHistoryItem(list.getEntryAtIndex(1), page2Url, page2Url, PAGE_2_TITLE, true);

        // Make sure the current index is back to 0
        Assert.assertEquals(0, list.getCurrentEntryIndex());
    }

    @Test
    @SmallTest
    public void testFavicon() throws Throwable {
        mWebServer.setResponseBase64(
                "/" + CommonResources.FAVICON_FILENAME,
                CommonResources.FAVICON_DATA_BASE64,
                CommonResources.getImagePngHeaders(false));
        final String url =
                mWebServer.setResponse("/favicon.html", CommonResources.FAVICON_STATIC_HTML, null);

        NavigationHistory list = getNavigationHistory(mAwContents);
        Assert.assertEquals(1, list.getEntryCount());

        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setImagesEnabled(true);
        int faviconLoadCount = mContentsClient.getFaviconHelper().getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        mContentsClient.getFaviconHelper().waitForCallback(faviconLoadCount);

        list = getNavigationHistory(mAwContents);
        // Make sure the first entry is still okay.
        checkHistoryItem(list.getEntryAtIndex(0), url, url, "", false);
    }

    // See http://crbug.com/481570
    @Test
    @SmallTest
    public void testTitleUpdatedWhenGoingBack() throws Throwable {
        final TestCallbackHelperContainer.OnPageFinishedHelper onPageFinishedHelper =
                mContentsClient.getOnPageFinishedHelper();
        NavigationHistory list = getNavigationHistory(mAwContents);
        Assert.assertEquals(1, list.getEntryCount());

        final String page1Url = addPage1ToServer(mWebServer);
        final String page2Url = addPage2ToServer(mWebServer);

        TestAwContentsClient.OnReceivedTitleHelper onReceivedTitleHelper =
                mContentsClient.getOnReceivedTitleHelper();
        // It would be unreliable to retrieve the call count after the first loadUrlSync,
        // as it is not synchronous with respect to updating the title. Instead, we capture
        // the initial call count (zero?) here, and keep waiting until we receive the update
        // from the second page load.
        int onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, page1Url);
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, page2Url);
        do {
            onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);
            onReceivedTitleCallCount = onReceivedTitleHelper.getCallCount();
        } while (!PAGE_2_TITLE.equals(onReceivedTitleHelper.getTitle()));
        navigateBack();
        onReceivedTitleHelper.waitForCallback(onReceivedTitleCallCount);
        Assert.assertEquals(PAGE_1_TITLE, onReceivedTitleHelper.getTitle());
    }

    // Test that a WebContents which hasn't navigated to any URL has a
    // NavigationHistory that has 1 entry: the initial NavigationEntry.
    @Test
    @SmallTest
    public void testFreshWebContentsInitialNavigationHistory() throws Throwable {
        NavigationHistory navHistory = mAwContents.getNavigationHistory();
        Assert.assertEquals(1, navHistory.getEntryCount());
        Assert.assertEquals(0, navHistory.getCurrentEntryIndex());
        Assert.assertTrue(navHistory.getEntryAtIndex(0).isInitialEntry());
        Assert.assertEquals(GURL.emptyGURL(), navHistory.getEntryAtIndex(0).getUrl());

        // Navigate the WebContents' main frame to another URL, which will
        // create a new NavigationEntry that replaces the initial
        // NavigationEntry.
        String nonEmptyUrl = mWebServer.setResponse("/nonEmptyURL.html", "", null);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), nonEmptyUrl);
        // Assert that we got an onPageFinished call for `nonEmptyUrl`.
        Assert.assertEquals(nonEmptyUrl, mContentsClient.getOnPageFinishedHelper().getUrl());

        // We committed a brand new NavigationEntry.
        navHistory = mAwContents.getNavigationHistory();
        Assert.assertEquals(1, navHistory.getEntryCount());
        Assert.assertEquals(0, navHistory.getCurrentEntryIndex());
        Assert.assertFalse(navHistory.getEntryAtIndex(0).isInitialEntry());
        Assert.assertEquals(nonEmptyUrl, navHistory.getEntryAtIndex(0).getUrl().getSpec());
    }

    // Tests that NavigationHistory in a new popup WebContents contains the
    // initial NavigationEntry.
    @Test
    @SmallTest
    public void testPopupInitialNavigationHistory() throws Throwable {
        // Open a popup without an URL.
        final String parentPageHtml =
                CommonResources.makeHtmlPageFrom(
                        "",
                        "<script>"
                                + "function tryOpenWindow() {"
                                + "  var newWindow = window.open();"
                                + "}</script>");
        mActivityTestRule.triggerPopup(
                mAwContents,
                mContentsClient,
                mWebServer,
                parentPageHtml,
                /* popupHtml= */ null,
                /* popupPath= */ null,
                "tryOpenWindow()");
        PopupInfo popupInfo = mActivityTestRule.createPopupContents(mAwContents);
        final AwContents popupContents = popupInfo.popupContents;

        // Test that the new WebContents, which stays on the initial empty
        // document, stays on the initial NavigationEntry.
        NavigationHistory navHistory = popupContents.getNavigationHistory();
        Assert.assertEquals(1, navHistory.getEntryCount());
        Assert.assertEquals(0, navHistory.getCurrentEntryIndex());
        Assert.assertTrue(navHistory.getEntryAtIndex(0).isInitialEntry());
        Assert.assertEquals(GURL.emptyGURL(), navHistory.getEntryAtIndex(0).getUrl());

        // Navigate the popup main frame to another URL, which will create a new
        // NavigationEntry that replaces the initial NavigationEntry.
        TestCallbackHelperContainer.OnPageFinishedHelper popupOnPageFinishedHelper =
                popupInfo.popupContentsClient.getOnPageFinishedHelper();
        String nonEmptyUrl = mWebServer.setResponse("/nonEmptyURL.html", "", null);
        mActivityTestRule.loadUrlSync(popupContents, popupOnPageFinishedHelper, nonEmptyUrl);
        // Assert that we got an onPageFinished call for `nonEmptyUrl`.
        Assert.assertEquals(nonEmptyUrl, popupOnPageFinishedHelper.getUrl());

        navHistory = popupContents.getNavigationHistory();
        Assert.assertEquals(1, navHistory.getEntryCount());
        Assert.assertEquals(0, navHistory.getCurrentEntryIndex());
        // We committed a brand new NavigationEntry that replaces the initial
        // NavigationEntry and has no relation to it, so isInitialEntry() is
        // false and it will be exposed to WebBackForwardList.
        Assert.assertFalse(navHistory.getEntryAtIndex(0).isInitialEntry());
        Assert.assertEquals(nonEmptyUrl, navHistory.getEntryAtIndex(0).getUrl().getSpec());
    }
}
