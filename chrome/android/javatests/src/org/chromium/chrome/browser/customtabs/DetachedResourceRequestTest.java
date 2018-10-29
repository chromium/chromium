// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsSession;
import android.support.customtabs.CustomTabsSessionToken;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.PathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.MockSafeBrowsingApiHandler;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.NetError;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for detached resource requests. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DetachedResourceRequestTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();
    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    private CustomTabsConnection mConnection;
    private Context mContext;
    private EmbeddedTestServer mServer;

    private static final String PRIVATE_DATA_DIRECTORY_SUFFIX = "chrome";
    private static final Uri ORIGIN = Uri.parse("http://cats.google.com");
    private static final int NET_OK = 0;

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        PathUtils.setPrivateDataDirectorySuffix(PRIVATE_DATA_DIRECTORY_SUFFIX);
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        mConnection = CustomTabsTestUtils.setUpConnection();
        mContext = InstrumentationRegistry.getInstrumentation()
                           .getTargetContext()
                           .getApplicationContext();
    }

    @After
    public void tearDown() throws Exception {
        CustomTabsTestUtils.cleanupSessions(mConnection);
        if (mServer != null) mServer.stopAndDestroyServer();
        mServer = null;
    }

    @Test
    @SmallTest
    public void testCanDoParallelRequest() throws Exception {
        CustomTabsSessionToken session = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        ThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(mConnection.canDoParallelRequest(session, ORIGIN)); });
        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertFalse(mConnection.canDoParallelRequest(session, ORIGIN)); });
        ThreadUtils.runOnUiThreadBlocking(() -> {
            String packageName = mContext.getPackageName();
            OriginVerifier.addVerifiedOriginForPackage(packageName, new Origin(ORIGIN.toString()),
                    CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertTrue(mConnection.canDoParallelRequest(session, ORIGIN));
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_RESOURCE_PREFETCH)
    public void testCanDoResourcePrefetch() throws Exception {
        CustomTabsSessionToken session = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        ThreadUtils.runOnUiThreadBlocking(() -> {
            String packageName = mContext.getPackageName();
            OriginVerifier.addVerifiedOriginForPackage(packageName, new Origin(ORIGIN.toString()),

                    CustomTabsService.RELATION_USE_AS_ORIGIN);
        });
        Intent intent = prepareIntentForResourcePrefetch(
                Arrays.asList(Uri.parse("https://foo.bar")), ORIGIN);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));
        });

        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));
        });

        mConnection.mClientManager.setAllowResourcePrefetchForSession(session, true);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(1, mConnection.maybePrefetchResources(session, intent));
        });
    }

    @Test
    @SmallTest
    public void testStartParallelRequestValidation() throws Exception {
        CustomTabsSessionToken session = prepareSession();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            int expected = CustomTabsConnection.ParallelRequestStatus.NO_REQUEST;
            HistogramDelta histogram =
                    new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            Assert.assertEquals(expected, mConnection.handleParallelRequest(session, new Intent()));
            Assert.assertEquals(1, histogram.getDelta());

            expected = CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_URL;
            histogram = new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            Intent intent =
                    prepareIntent(Uri.parse("android-app://this.is.an.android.app"), ORIGIN);
            Assert.assertEquals("Should not allow android-app:// scheme", expected,
                    mConnection.handleParallelRequest(session, intent));
            Assert.assertEquals(1, histogram.getDelta());

            expected = CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_URL;
            histogram = new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            intent = prepareIntent(Uri.parse(""), ORIGIN);
            Assert.assertEquals("Should not allow an empty URL", expected,
                    mConnection.handleParallelRequest(session, intent));
            Assert.assertEquals(1, histogram.getDelta());

            expected =
                    CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_REFERRER_FOR_SESSION;
            histogram = new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            intent = prepareIntent(Uri.parse("HTTPS://foo.bar"), Uri.parse("wrong://origin"));
            Assert.assertEquals("Should not allow an arbitrary origin", expected,
                    mConnection.handleParallelRequest(session, intent));

            expected = CustomTabsConnection.ParallelRequestStatus.SUCCESS;
            histogram = new HistogramDelta("CustomTabs.ParallelRequestStatusOnStart", expected);
            intent = prepareIntent(Uri.parse("HTTPS://foo.bar"), ORIGIN);
            Assert.assertEquals(expected, mConnection.handleParallelRequest(session, intent));
            Assert.assertEquals(1, histogram.getDelta());
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_RESOURCE_PREFETCH)
    public void testStartResourcePrefetchUrlsValidation() throws Exception {
        CustomTabsSessionToken session = prepareSession();

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, new Intent()));

            ArrayList<Uri> urls = new ArrayList<>();
            Intent intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));

            urls.add(Uri.parse("android-app://this.is.an.android.app"));
            intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));

            urls.add(Uri.parse(""));
            intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));

            urls.add(Uri.parse("https://foo.bar"));
            intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(1, mConnection.maybePrefetchResources(session, intent));

            urls.add(Uri.parse("https://bar.foo"));
            intent = prepareIntentForResourcePrefetch(urls, ORIGIN);
            Assert.assertEquals(2, mConnection.maybePrefetchResources(session, intent));

            intent = prepareIntentForResourcePrefetch(urls, Uri.parse("wrong://origin"));
            Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent));
        });
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testCanStartParallelRequest() throws Exception {
        final CallbackHelper cb = new CallbackHelper();
        setUpTestServerWithListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                cb.notifyCalled();
            }
        });
        Uri url = Uri.parse(mServer.getURL("/echotitle"));

        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                    mConnection.handleParallelRequest(session, prepareIntent(url, ORIGIN)));
        });
        customTabsCallback.waitForRequest();
        cb.waitForCallback(0, 1);
        customTabsCallback.waitForCompletion();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testParallelRequestFailureCallback() throws Exception {
        Uri url = Uri.parse("http://request-url");
        int status =
                CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_REFERRER_FOR_SESSION;
        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(url, status, 0);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(status,
                    mConnection.handleParallelRequest(
                            session, prepareIntent(url, Uri.parse("http://not-the-right-origin"))));
        });
        customTabsCallback.waitForRequest();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testParallelRequestCompletionFailureCallback() throws Exception {
        final CallbackHelper cb = new CallbackHelper();
        setUpTestServerWithListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                cb.notifyCalled();
            }
        });
        Uri url = Uri.parse(mServer.getURL("/close-socket"));

        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(url,
                        CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                        Math.abs(NetError.ERR_EMPTY_RESPONSE));
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);

        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                    mConnection.handleParallelRequest(session, prepareIntent(url, ORIGIN)));
        });
        customTabsCallback.waitForRequest();
        cb.waitForCallback(0, 1);
        customTabsCallback.waitForCompletion();
    }

    @Test
    @SmallTest
    public void testCanStartResourcePrefetch() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        final CallbackHelper cb = new CallbackHelper();
        // We expect one read per prefetched url.
        setUpTestServerWithListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                cb.notifyCalled();
            }
        });

        List<Uri> urls = Arrays.asList(Uri.parse(mServer.getURL("/echo-raw?a=1")),
                Uri.parse(mServer.getURL("/echo-raw?a=2")),
                Uri.parse(mServer.getURL("/echo-raw?a=3")));
        ThreadUtils.runOnUiThread(() -> {
            Assert.assertEquals(urls.size(),
                    mConnection.maybePrefetchResources(
                            session, prepareIntentForResourcePrefetch(urls, ORIGIN)));
        });
        cb.waitForCallback(0, urls.size());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testCanSetCookie() throws Exception {
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));

        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                    mConnection.handleParallelRequest(session, prepareIntent(url, ORIGIN)));
        });
        customTabsCallback.waitForRequest();
        customTabsCallback.waitForCompletion();

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    /**
     * Tests that cached detached resource requests that are forbidden by SafeBrowsing don't end up
     * in the content area, for a main resource.
     */
    @Test
    @SmallTest
    public void testSafeBrowsingMainResource() throws Exception {
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        CustomTabsSessionToken session = prepareSession();
        String cacheable = "/cachetime";
        CallbackHelper readFromSocketCallback = waitForDetachedRequest(session, cacheable);
        Uri url = Uri.parse(mServer.getURL(cacheable));

        try {
            MockSafeBrowsingApiHandler.addMockResponse(
                    url.toString(), "{\"matches\":[{\"threat_type\":\"5\"}]}");

            Intent intent =
                    CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, url.toString());
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

            Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> Assert.assertTrue(tab.getWebContents().isShowingInterstitialPage()));
            // 1 read from the detached request, and 0 from the page load, as
            // the response comes from the cache, and SafeBrowsing blocks it.
            Assert.assertEquals(1, readFromSocketCallback.getCallCount());
        } finally {
            MockSafeBrowsingApiHandler.clearMockResponses();
        }
    }

    /**
     * Tests that cached detached resource requests that are forbidden by SafeBrowsing don't end up
     * in the content area, for a subresource.
     */
    @Test
    @SmallTest
    public void testSafeBrowsingSubresource() throws Exception {
        SafeBrowsingApiBridge.setSafeBrowsingHandlerType(
                new MockSafeBrowsingApiHandler().getClass());
        CustomTabsSessionToken session = prepareSession();
        String cacheable = "/cachetime";
        waitForDetachedRequest(session, cacheable);
        Uri url = Uri.parse(mServer.getURL(cacheable));

        try {
            MockSafeBrowsingApiHandler.addMockResponse(
                    url.toString(), "{\"matches\":[{\"threat_type\":\"5\"}]}");

            String pageUrl = mServer.getURL("/chrome/test/data/android/cacheable_subresource.html");
            Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, pageUrl);
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

            Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
            WebContents webContents = tab.getWebContents();
            // Need to poll as the subresource request is async.
            CriteriaHelper.pollUiThread(() -> webContents.isShowingInterstitialPage());
        } finally {
            MockSafeBrowsingApiHandler.clearMockResponses();
        }
    }

    @Test
    @SmallTest
    public void testCanBlockThirdPartyCookies() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge prefs = PrefServiceBridge.getInstance();
            Assert.assertFalse(prefs.isBlockThirdPartyCookiesEnabled());
            prefs.setBlockThirdPartyCookiesEnabled(true);
        });
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                    mConnection.handleParallelRequest(session, prepareIntent(url, ORIGIN)));
        });

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"None\"", content);
    }

    @Test
    @SmallTest
    public void testThirdPartyCookieBlockingAllowsFirstParty() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        ThreadUtils.runOnUiThreadBlocking(() -> {
            PrefServiceBridge prefs = PrefServiceBridge.getInstance();
            Assert.assertFalse(prefs.isBlockThirdPartyCookiesEnabled());
            prefs.setBlockThirdPartyCookiesEnabled(true);
        });
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        final Uri origin = Uri.parse(new Origin(url).toString());
        CustomTabsSessionToken session = prepareSession(url);

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                    mConnection.handleParallelRequest(session, prepareIntent(url, origin)));
        });

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content = JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    private CustomTabsSessionToken prepareSession() throws Exception {
        return prepareSession(ORIGIN, null);
    }

    private CustomTabsSessionToken prepareSession(Uri origin) throws Exception {
        return prepareSession(origin, null);
    }

    private CustomTabsSessionToken prepareSession(Uri origin, CustomTabsCallback callback)
            throws Exception {
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(callback);
        Intent intent = (new CustomTabsIntent.Builder(session)).build().intent;
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(mConnection.newSession(token));
        mConnection.mClientManager.setAllowParallelRequestForSession(token, true);
        mConnection.mClientManager.setAllowResourcePrefetchForSession(token, true);
        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            OriginVerifier.addVerifiedOriginForPackage(mContext.getPackageName(),
                    new Origin(origin.toString()), CustomTabsService.RELATION_USE_AS_ORIGIN);
            Assert.assertTrue(mConnection.canDoParallelRequest(token, origin));
        });
        return token;
    }

    private void setUpTestServerWithListener(EmbeddedTestServer.ConnectionListener listener)
            throws InterruptedException {
        mServer = new EmbeddedTestServer();
        final CallbackHelper readFromSocketCallback = new CallbackHelper();
        mServer.initializeNative(mContext, EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mServer.setConnectionListener(listener);
        mServer.addDefaultHandlers("");
        Assert.assertTrue(mServer.start());
    }

    private CallbackHelper waitForDetachedRequest(CustomTabsSessionToken session,
            String relativeUrl) throws InterruptedException, TimeoutException {
        // Count the number of times data is read from the socket.
        // We expect 1 for the detached request.
        // Cannot count connections as Chrome opens multiple sockets at page load time.
        CallbackHelper readFromSocketCallback = new CallbackHelper();
        setUpTestServerWithListener(new EmbeddedTestServer.ConnectionListener() {
            @Override
            public void readFromSocket(long socketId) {
                readFromSocketCallback.notifyCalled();
            }
        });
        Uri url = Uri.parse(mServer.getURL(relativeUrl));

        ThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                    mConnection.handleParallelRequest(session, prepareIntent(url, ORIGIN)));
        });
        readFromSocketCallback.waitForCallback(0);
        return readFromSocketCallback;
    }

    private static Intent prepareIntent(Uri url, Uri referrer) {
        Intent intent = new Intent();
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_URL_KEY, url);
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, referrer);
        return intent;
    }

    private static Intent prepareIntentForResourcePrefetch(List<Uri> urls, Uri referrer) {
        Intent intent = new Intent();
        intent.putExtra(CustomTabsConnection.RESOURCE_PREFETCH_URL_LIST_KEY, new ArrayList<>(urls));
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, referrer);
        return intent;
    }

    private static class DetachedResourceRequestCheckCallback extends CustomTabsCallback {
        private final Uri mExpectedUrl;
        private final int mExpectedRequestStatus;
        private final int mExpectedFinalStatus;
        private final CallbackHelper mRequestedWaiter = new CallbackHelper();
        private final CallbackHelper mCompletionWaiter = new CallbackHelper();

        public DetachedResourceRequestCheckCallback(
                Uri expectedUrl, int expectedRequestStatus, int expectedFinalStatus) {
            super();
            mExpectedUrl = expectedUrl;
            mExpectedRequestStatus = expectedRequestStatus;
            mExpectedFinalStatus = expectedFinalStatus;
        }

        @Override
        public void extraCallback(String callbackName, Bundle args) {
            if (CustomTabsConnection.ON_DETACHED_REQUEST_REQUESTED.equals(callbackName)) {
                Uri url = args.getParcelable("url");
                int status = args.getInt("status");
                Assert.assertEquals(mExpectedUrl, url);
                Assert.assertEquals(mExpectedRequestStatus, status);
                mRequestedWaiter.notifyCalled();
            } else if (CustomTabsConnection.ON_DETACHED_REQUEST_COMPLETED.equals(callbackName)) {
                Uri url = args.getParcelable("url");
                int status = args.getInt("net_error");
                Assert.assertEquals(mExpectedUrl, url);
                Assert.assertEquals(mExpectedFinalStatus, status);
                mCompletionWaiter.notifyCalled();
            }
        }

        public void waitForRequest() throws InterruptedException, TimeoutException {
            mRequestedWaiter.waitForCallback(0, 1);
        }

        public void waitForCompletion() throws InterruptedException, TimeoutException {
            mCompletionWaiter.waitForCallback(0, 1);
        }
    }
}
