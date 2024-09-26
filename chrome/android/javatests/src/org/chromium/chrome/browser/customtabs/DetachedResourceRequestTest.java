// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsCallback;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.MockSafeBrowsingApiHandler;
import org.chromium.chrome.browser.browserservices.verification.ChromeOriginVerifier;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.NetError;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests for detached resource requests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(ChromeFeatureList.SAFE_BROWSING_DELAYED_WARNINGS)
public class DetachedResourceRequestTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private CustomTabsConnection mConnection;
    private Context mContext;
    private EmbeddedTestServer mServer;

    private static final Uri ORIGIN = Uri.parse("http://cats.google.com");
    private static final int NET_OK = 0;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));
        mConnection = CustomTabsTestUtils.setUpConnection();
        mContext =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        ThreadUtils.runOnUiThreadBlocking(ChromeOriginVerifier::clearCachedVerificationsForTesting);
    }

    @After
    public void tearDown() {
        CustomTabsTestUtils.cleanupSessions(mConnection);
    }

    @Test
    @SmallTest
    public void testCanDoParallelRequest() {
        CustomTabsSessionToken session = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertFalse(mConnection.canDoParallelRequest(session, ORIGIN)));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String packageName = mContext.getPackageName();
                    ChromeOriginVerifier.addVerificationOverride(
                            packageName,
                            Origin.create(ORIGIN.toString()),
                            CustomTabsService.RELATION_USE_AS_ORIGIN);
                    Assert.assertTrue(mConnection.canDoParallelRequest(session, ORIGIN));
                });
    }

    @Test
    @SmallTest
    public void testCanDoResourcePrefetch() throws Exception {
        CustomTabsSessionToken session = CustomTabsSessionToken.createMockSessionTokenForTesting();
        Assert.assertTrue(mConnection.newSession(session));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    String packageName = mContext.getPackageName();
                    ChromeOriginVerifier.addVerificationOverride(
                            packageName,
                            Origin.create(ORIGIN.toString()),
                            CustomTabsService.RELATION_USE_AS_ORIGIN);
                });
        Intent intent =
                prepareIntentForResourcePrefetch(
                        Arrays.asList(Uri.parse("https://foo.bar")), ORIGIN);
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent)));

        CustomTabsTestUtils.warmUpAndWait();
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(0, mConnection.maybePrefetchResources(session, intent)));

        mConnection.mClientManager.setAllowResourcePrefetchForSession(session, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(1, mConnection.maybePrefetchResources(session, intent)));
    }

    @Test
    @SmallTest
    public void testStartParallelRequestValidation() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        CustomTabsTestUtils.warmUpAndWait();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int expected = CustomTabsConnection.ParallelRequestStatus.NO_REQUEST;
                    var histogram =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "CustomTabs.ParallelRequestStatusOnStart", expected);
                    Assert.assertEquals(
                            expected, mConnection.handleParallelRequest(session, new Intent()));
                    histogram.assertExpected();

                    expected = CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_URL;
                    histogram =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "CustomTabs.ParallelRequestStatusOnStart", expected);
                    Intent intent =
                            prepareIntent(
                                    Uri.parse("android-app://this.is.an.android.app"), ORIGIN);
                    Assert.assertEquals(
                            "Should not allow android-app:// scheme",
                            expected,
                            mConnection.handleParallelRequest(session, intent));
                    histogram.assertExpected();

                    expected = CustomTabsConnection.ParallelRequestStatus.FAILURE_INVALID_URL;
                    histogram =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "CustomTabs.ParallelRequestStatusOnStart", expected);
                    intent = prepareIntent(Uri.parse(""), ORIGIN);
                    Assert.assertEquals(
                            "Should not allow an empty URL",
                            expected,
                            mConnection.handleParallelRequest(session, intent));
                    histogram.assertExpected();

                    expected =
                            CustomTabsConnection.ParallelRequestStatus
                                    .FAILURE_INVALID_REFERRER_FOR_SESSION;
                    histogram =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "CustomTabs.ParallelRequestStatusOnStart", expected);
                    intent =
                            prepareIntent(
                                    Uri.parse("HTTPS://foo.bar"), Uri.parse("wrong://origin"));
                    Assert.assertEquals(
                            "Should not allow an arbitrary origin",
                            expected,
                            mConnection.handleParallelRequest(session, intent));
                    histogram.assertExpected();

                    expected = CustomTabsConnection.ParallelRequestStatus.SUCCESS;
                    histogram =
                            HistogramWatcher.newSingleRecordWatcher(
                                    "CustomTabs.ParallelRequestStatusOnStart", expected);
                    intent = prepareIntent(Uri.parse("HTTPS://foo.bar"), ORIGIN);
                    Assert.assertEquals(
                            expected, mConnection.handleParallelRequest(session, intent));
                    histogram.assertExpected();
                });
    }

    @Test
    @SmallTest
    public void testStartResourcePrefetchUrlsValidation() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        CustomTabsTestUtils.warmUpAndWait();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            0, mConnection.maybePrefetchResources(session, new Intent()));

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
        testCanStartParallelRequest(true);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testCanStartParallelRequestBeforeNative() throws Exception {
        testCanStartParallelRequest(false);
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
        CustomTabsTestUtils.warmUpAndWait();

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Assert.assertEquals(
                            status,
                            mConnection.handleParallelRequest(
                                    session,
                                    prepareIntent(url, Uri.parse("http://not-the-right-origin"))));
                });
        customTabsCallback.waitForRequest(0, 1);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testParallelRequestCompletionFailureCallback() throws Exception {
        final CallbackHelper cb = new CallbackHelper();
        setUpTestServerWithListener(
                new EmbeddedTestServer.ConnectionListener() {
                    @Override
                    public void readFromSocket(long socketId) {
                        cb.notifyCalled();
                    }
                });
        Uri url = Uri.parse(mServer.getURL("/close-socket"));

        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url,
                        CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                        Math.abs(NetError.ERR_EMPTY_RESPONSE));
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);

        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> mConnection.onHandledIntent(session, prepareIntent(url, ORIGIN)));
        CustomTabsTestUtils.warmUpAndWait();
        customTabsCallback.waitForRequest(0, 1);
        cb.waitForCallback(0, 1);
        customTabsCallback.waitForCompletion(0, 1);
    }

    @Test
    @SmallTest
    public void testCanStartResourcePrefetch() throws Exception {
        CustomTabsSessionToken session = prepareSession();
        CustomTabsTestUtils.warmUpAndWait();

        final CallbackHelper cb = new CallbackHelper();
        // We expect one read per prefetched url.
        setUpTestServerWithListener(
                new EmbeddedTestServer.ConnectionListener() {
                    @Override
                    public void readFromSocket(long socketId) {
                        cb.notifyCalled();
                    }
                });

        List<Uri> urls =
                Arrays.asList(
                        Uri.parse(mServer.getURL("/echo-raw?a=1")),
                        Uri.parse(mServer.getURL("/echo-raw?a=2")),
                        Uri.parse(mServer.getURL("/echo-raw?a=3")));
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    Assert.assertEquals(
                            urls.size(),
                            mConnection.maybePrefetchResources(
                                    session, prepareIntentForResourcePrefetch(urls, ORIGIN)));
                });
        cb.waitForCallback(0, urls.size());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testCanSetCookie() throws Exception {
        testCanSetCookie(true);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testCanSetCookieBeforeNative() throws Exception {
        testCanSetCookie(false);
    }

    /**
     * Tests that cached detached resource requests that are forbidden by SafeBrowsing don't end up
     * in the content area, for a main resource.
     */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY)
    @DisabledTest(message = "https://crbug.com/1431268")
    public void testSafeBrowsingMainResource() throws Exception {
        testSafeBrowsingMainResource(/* afterNative= */ true, /* splitCacheEnabled= */ false);
    }

    /**
     * Tests that non-cached detached resource requests that are forbidden by SafeBrowsing don't end
     * up in the content area, for a main resource.
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY)
    @DisabledTest(message = "Flaky. See crbug.com/1523239")
    public void testSafeBrowsingMainResourceWithSplitCache() throws Exception {
        testSafeBrowsingMainResource(/* afterNative= */ true, /* splitCacheEnabled= */ true);
    }

    /**
     * Tests that cached detached resource requests that are forbidden by SafeBrowsing don't end up
     * in the content area, for a main resource.
     */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SPLIT_CACHE_BY_NETWORK_ISOLATION_KEY)
    @DisabledTest(message = "https://crbug.com/1431268")
    public void testSafeBrowsingMainResourceBeforeNative() throws Exception {
        testSafeBrowsingMainResource(/* afterNative= */ false, /* splitCacheEnabled= */ false);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testCanBlockThirdPartyCookies() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(mContext, ServerCertificate.CERT_OK);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefs = UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    Assert.assertEquals(
                            prefs.getInteger(COOKIE_CONTROLS_MODE),
                            CookieControlsMode.INCOGNITO_ONLY);
                    prefs.setInteger(COOKIE_CONTROLS_MODE, CookieControlsMode.BLOCK_THIRD_PARTY);
                });
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie;SameSite=none;Secure"));
        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                            mConnection.handleParallelRequest(session, prepareIntent(url, ORIGIN)));
                });
        customTabsCallback.waitForRequest(0, 1);
        customTabsCallback.waitForCompletion(0, 1);

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"None\"", content);
    }

    /**
     * Demonstrates that cookies are SameSite=Lax by default, and cookies in third-party contexts
     * require both SameSite=None and Secure.
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testSameSiteLaxByDefaultCookies() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(mContext, ServerCertificate.CERT_OK);
        // This isn't blocking third-party cookies by preferences.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefs = UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    Assert.assertEquals(
                            prefs.getInteger(COOKIE_CONTROLS_MODE),
                            CookieControlsMode.INCOGNITO_ONLY);
                });

        // Of the three cookies, only one that's both SameSite=None and Secure
        // is actually set. (And Secure is meant as the attribute, being over
        // https isn't enough).
        final Uri url =
                Uri.parse(
                        mServer.getURL(
                                "/set-cookie?a=1&b=2;SameSite=None&c=3;SameSite=None;Secure;"));
        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                            mConnection.handleParallelRequest(session, prepareIntent(url, ORIGIN)));
                });
        customTabsCallback.waitForRequest(0, 1);
        customTabsCallback.waitForCompletion(0, 1);

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"c=3\"", content);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testThirdPartyCookieBlockingAllowsFirstParty() throws Exception {
        CustomTabsTestUtils.warmUpAndWait();
        mServer = EmbeddedTestServer.createAndStartServer(mContext);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefs = UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    Assert.assertEquals(
                            prefs.getInteger(COOKIE_CONTROLS_MODE),
                            CookieControlsMode.INCOGNITO_ONLY);
                    prefs.setInteger(COOKIE_CONTROLS_MODE, CookieControlsMode.BLOCK_THIRD_PARTY);
                });
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        final Uri origin = Uri.parse(Origin.create(url).toString());
        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSessionToken session = prepareSession(origin, customTabsCallback);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            CustomTabsConnection.ParallelRequestStatus.SUCCESS,
                            mConnection.handleParallelRequest(session, prepareIntent(url, origin)));
                });
        customTabsCallback.waitForRequest(0, 1);
        customTabsCallback.waitForCompletion(0, 1);

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.CCT_REPORT_PARALLEL_REQUEST_STATUS)
    public void testRepeatedIntents() throws Exception {
        mServer = EmbeddedTestServer.createAndStartServer(mContext);

        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie"));
        DetachedResourceRequestCheckCallback callback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(callback).session;

        Uri launchedUrl = Uri.parse(mServer.getURL("/echotitle"));
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.setComponent(new ComponentName(mContext, ChromeLauncherActivity.class));
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(launchedUrl);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_URL_KEY, url);
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, ORIGIN);

        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(mConnection.newSession(token));
        mConnection.mClientManager.setAllowParallelRequestForSession(token, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeOriginVerifier.addVerificationOverride(
                            mContext.getPackageName(),
                            Origin.create(ORIGIN.toString()),
                            CustomTabsService.RELATION_USE_AS_ORIGIN);
                    Assert.assertTrue(mConnection.canDoParallelRequest(token, ORIGIN));
                });

        // Launching a CCT and loading a URL takes more time than usual. Gives a longer timeout.
        // See crbug.com/40737671.
        mContext.startActivity(intent);
        callback.waitForRequest(0, 1, 10, TimeUnit.SECONDS);
        callback.waitForCompletion(0, 1, 10, TimeUnit.SECONDS);

        mContext.startActivity(intent);
        callback.waitForRequest(1, 1, 10, TimeUnit.SECONDS);
        callback.waitForCompletion(1, 1, 10, TimeUnit.SECONDS);
    }

    private void testCanStartParallelRequest(boolean afterNative) throws Exception {
        final CallbackHelper cb = new CallbackHelper();
        setUpTestServerWithListener(
                new EmbeddedTestServer.ConnectionListener() {
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

        if (afterNative) CustomTabsTestUtils.warmUpAndWait();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> mConnection.onHandledIntent(session, prepareIntent(url, ORIGIN)));
        if (!afterNative) CustomTabsTestUtils.warmUpAndWait();

        customTabsCallback.waitForRequest(0, 1);
        cb.waitForCallback(0, 1);
        customTabsCallback.waitForCompletion(0, 1);
    }

    private void testCanSetCookie(boolean afterNative) throws Exception {
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(mContext, ServerCertificate.CERT_OK);
        final Uri url = Uri.parse(mServer.getURL("/set-cookie?acookie;SameSite=none;Secure"));

        DetachedResourceRequestCheckCallback customTabsCallback =
                new DetachedResourceRequestCheckCallback(
                        url, CustomTabsConnection.ParallelRequestStatus.SUCCESS, NET_OK);
        CustomTabsSessionToken session = prepareSession(ORIGIN, customTabsCallback);
        if (afterNative) CustomTabsTestUtils.warmUpAndWait();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mConnection.onHandledIntent(session, prepareIntent(url, ORIGIN)));

        if (!afterNative) CustomTabsTestUtils.warmUpAndWait();
        customTabsCallback.waitForRequest(0, 1);
        customTabsCallback.waitForCompletion(0, 1);

        String echoUrl = mServer.getURL("/echoheader?Cookie");
        Intent intent = CustomTabsIntentTestUtils.createMinimalCustomTabIntent(mContext, echoUrl);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
        String content =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "document.body.textContent");
        Assert.assertEquals("\"acookie\"", content);
    }

    private void testSafeBrowsingMainResource(boolean afterNative, boolean splitCacheEnabled)
            throws Exception {
        SafeBrowsingApiBridge.setSafeBrowsingApiHandler(new MockSafeBrowsingApiHandler());
        CustomTabsSessionToken session = prepareSession();

        String cacheable = "/cachetime";
        CallbackHelper readFromSocketCallback =
                waitForDetachedRequest(session, cacheable, afterNative);
        Uri url = Uri.parse(mServer.getURL(cacheable));

        try {
            MockSafeBrowsingApiHandler.addMockResponse(
                    url.toString(), MockSafeBrowsingApiHandler.SOCIAL_ENGINEERING_CODE);

            Intent intent =
                    CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                            mContext, url.toString());
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

            Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();

            // TODO(crbug.com/40666836): For now, we check the presence of an interstitial through
            // the title since isShowingInterstitialPage does not work with committed interstitials.
            // Once we fully migrate to committed interstitials, this should be changed to a more
            // robust check.
            CriteriaHelper.pollUiThread(
                    () -> tab.getWebContents().getTitle().equals("Security error"));

            if (splitCacheEnabled) {
                // Note that since the SplitCacheByNetworkIsolationKey feature is
                // enabled, the detached request and the original request both
                // would be read from the socket as they both would have different
                // top frame origins in the cache partitioning key: |ORIGIN| and
                // mServer's base url, respectively.
                Assert.assertEquals(2, readFromSocketCallback.getCallCount());
            } else {
                // 1 read from the detached request, and 0 from the page load, as
                // the response comes from the cache, and SafeBrowsing blocks it.
                Assert.assertEquals(1, readFromSocketCallback.getCallCount());
            }
        } finally {
            MockSafeBrowsingApiHandler.clearMockResponses();
            SafeBrowsingApiBridge.clearHandlerForTesting();
        }
    }

    private CustomTabsSessionToken prepareSession() throws Exception {
        return prepareSession(ORIGIN, null);
    }

    private CustomTabsSessionToken prepareSession(Uri origin) throws Exception {
        return prepareSession(origin, null);
    }

    private CustomTabsSessionToken prepareSession(Uri origin, CustomTabsCallback callback)
            throws Exception {
        CustomTabsSession session = CustomTabsTestUtils.bindWithCallback(callback).session;
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(mConnection.newSession(token));
        mConnection.mClientManager.setAllowParallelRequestForSession(token, true);
        mConnection.mClientManager.setAllowResourcePrefetchForSession(token, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeOriginVerifier.addVerificationOverride(
                            mContext.getPackageName(),
                            Origin.create(origin.toString()),
                            CustomTabsService.RELATION_USE_AS_ORIGIN);
                    Assert.assertTrue(mConnection.canDoParallelRequest(token, origin));
                });
        return token;
    }

    private void setUpTestServerWithListener(EmbeddedTestServer.ConnectionListener listener) {
        mServer = new EmbeddedTestServer();
        final CallbackHelper readFromSocketCallback = new CallbackHelper();
        mServer.initializeNative(mContext, EmbeddedTestServer.ServerHTTPSSetting.USE_HTTP);
        mServer.setConnectionListener(listener);
        mServer.addDefaultHandlers("");
        Assert.assertTrue(mServer.start());
    }

    private CallbackHelper waitForDetachedRequest(
            CustomTabsSessionToken session, String relativeUrl, boolean afterNative)
            throws TimeoutException {
        // Count the number of times data is read from the socket.
        // We expect 1 for the detached request.
        // Cannot count connections as Chrome opens multiple sockets at page load time.
        CallbackHelper readFromSocketCallback = new CallbackHelper();
        setUpTestServerWithListener(
                new EmbeddedTestServer.ConnectionListener() {
                    @Override
                    public void readFromSocket(long socketId) {
                        readFromSocketCallback.notifyCalled();
                    }
                });
        Uri url = Uri.parse(mServer.getURL(relativeUrl));
        if (afterNative) CustomTabsTestUtils.warmUpAndWait();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mConnection.onHandledIntent(session, prepareIntent(url, ORIGIN)));
        if (!afterNative) CustomTabsTestUtils.warmUpAndWait();
        readFromSocketCallback.waitForCallback(0);
        return readFromSocketCallback;
    }

    private static Intent prepareIntent(Uri url, Uri referrer) {
        Intent intent = new Intent();
        intent.setData(Uri.parse("http://www.example.com"));
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_URL_KEY, url);
        intent.putExtra(CustomTabsConnection.PARALLEL_REQUEST_REFERRER_KEY, referrer);
        return intent;
    }

    private static Intent prepareIntentForResourcePrefetch(List<Uri> urls, Uri referrer) {
        Intent intent = new Intent();
        intent.setData(Uri.parse("http://www.example.com"));
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

        public void waitForRequest() throws TimeoutException {
            mRequestedWaiter.waitForOnly();
        }

        public void waitForRequest(int currentCallCount, int numberOfCallsToWaitFor)
                throws TimeoutException {
            mRequestedWaiter.waitForCallback(currentCallCount, numberOfCallsToWaitFor);
        }

        public void waitForRequest(int currentCount, int expectCount, int timeout, TimeUnit unit)
                throws TimeoutException {
            mRequestedWaiter.waitForCallback(currentCount, expectCount, timeout, unit);
        }

        public void waitForCompletion() throws TimeoutException {
            mCompletionWaiter.waitForOnly();
        }

        public void waitForCompletion(int currentCallCount, int numberOfCallsToWaitFor)
                throws TimeoutException {
            mCompletionWaiter.waitForCallback(currentCallCount, numberOfCallsToWaitFor);
        }

        public void waitForCompletion(int currentCount, int expectCount, int timeout, TimeUnit unit)
                throws TimeoutException {
            mCompletionWaiter.waitForCallback(currentCount, expectCount, timeout, unit);
        }
    }
}
