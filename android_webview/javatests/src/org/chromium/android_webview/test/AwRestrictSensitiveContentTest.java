// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;
import android.util.Pair;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwOriginVerificationScheduler;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.PackageUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the restricting access sensitive web content.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwRestrictSensitiveContentTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static class OnProgressChangedClient extends TestAwContentsClient {
        List<Integer> mProgresses = new ArrayList<Integer>();

        @Override
        public void onProgressChanged(int progress) {
            super.onProgressChanged(progress);
            mProgresses.add(Integer.valueOf(progress));
            if (progress == 100 && mCallbackHelper.getCallCount() == 0) {
                mCallbackHelper.notifyCalled();
            }
        }

        public void waitForFullLoad() throws TimeoutException {
            mCallbackHelper.waitForFirst();
        }
        private CallbackHelper mCallbackHelper = new CallbackHelper();
    }

    private TestWebServer mWebServer;
    private OnProgressChangedClient mContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;

    @Before
    public void setUp() throws Exception {
        mContentsClient = new OnProgressChangedClient();
        mTestContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();

        mWebServer = TestWebServer.start();

        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AwOriginVerificationScheduler.init(context.getPackageName(),
                                mActivityTestRule.getAwBrowserContext(), context));
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private String addPageToTestServer(TestWebServer webServer, String httpPath, String html) {
        return addPageToTestServer(
                webServer, httpPath, html, new ArrayList<Pair<String, String>>());
    }

    private String addPageToTestServer(TestWebServer webServer, String httpPath, String html,
            List<Pair<String, String>> additionalHeaders) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("Content-Type", "text/html"));
        headers.add(Pair.create("Cache-Control", "no-store"));
        headers.addAll(additionalHeaders);
        return webServer.setResponse(httpPath, html, headers);
    }

    private String addAboutPageToTestServer(
            TestWebServer webServer, List<Pair<String, String>> additionalHeaders) {
        return addPageToTestServer(webServer, "/" + CommonResources.ABOUT_FILENAME,
                CommonResources.ABOUT_HTML, additionalHeaders);
    }

    private String addAssetListToTestServer(TestWebServer webServer, String fingerprint) {
        return addPageToTestServer(webServer, CommonResources.ASSET_LINKS_PATH,
                CommonResources.makeAssetFile(fingerprint));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"disable-features=WebViewRestrictSensitiveContent"})
    public void disablingFeatureDoesBlockOrRunValidation() throws Throwable {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("X-Embedder-Ancestors", "none"));
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer, headers);

        AwOriginVerificationScheduler scheduler = AwOriginVerificationScheduler.getInstance();

        scheduler.addPendingOriginForTesting(Origin.create(aboutPageUrl));
        Assert.assertEquals(2, scheduler.getPendingOriginsForTesting().size());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mTestContainerView.getAwContents().loadUrl(aboutPageUrl, null));
        mContentsClient.waitForFullLoad();

        Assert.assertEquals(CommonResources.ABOUT_TITLE, mAwContents.getTitle());
        Assert.assertEquals(2, scheduler.getPendingOriginsForTesting().size());
        Assert.assertTrue(
                scheduler.getPendingOriginsForTesting().contains(Origin.create(aboutPageUrl)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewRestrictSensitiveContent"})
    public void testInitAndScheduleAll() throws Throwable {

        CountDownLatch countVerifiedLatch = new CountDownLatch(1);
        mActivityTestRule.runOnUiThread(() -> {
            AwOriginVerificationScheduler.initAndScheduleAll(
                    (res) -> { countVerifiedLatch.countDown(); });
        });

        countVerifiedLatch.await();
        AwOriginVerificationScheduler scheduler = AwOriginVerificationScheduler.getInstance();

        Assert.assertNotNull(scheduler);
        Set<Origin> pendingOrigins = scheduler.getPendingOriginsForTesting();
        Assert.assertEquals(0, pendingOrigins.size());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewRestrictSensitiveContent"})
    public void doesNotBlockDALVerifiedContent() throws Throwable {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("X-Embedder-Ancestors", "none"));
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer, headers);

        List<String> mSignatureFingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(context.getPackageName());
        final String assetLinksUrl =
                addAssetListToTestServer(mWebServer, mSignatureFingerprints.get(0));

        AwOriginVerificationScheduler scheduler = AwOriginVerificationScheduler.getInstance();

        AwOriginVerificationScheduler.getInstance().addPendingOriginForTesting(
                Origin.create(aboutPageUrl));
        Set<Origin> pendingOrigins = scheduler.getPendingOriginsForTesting();
        Assert.assertEquals(2, pendingOrigins.size());
        Assert.assertTrue(pendingOrigins.contains(Origin.create(aboutPageUrl)));

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mTestContainerView.getAwContents().loadUrl(aboutPageUrl, null));
        mContentsClient.waitForFullLoad();

        Set<Origin> pendingOriginsAfterRequest = scheduler.getPendingOriginsForTesting();
        Assert.assertEquals(1, pendingOriginsAfterRequest.size());
        Assert.assertFalse(pendingOriginsAfterRequest.contains(Origin.create(aboutPageUrl)));

        Assert.assertEquals(CommonResources.ABOUT_TITLE, mAwContents.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewRestrictSensitiveContent"})
    public void doesNotBlockHeaderVerifiedContent() throws Throwable {
        final String webpageNotAvailable = "Webpage not available";
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("X-Embedder-Ancestors", "*"));
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer, headers);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mTestContainerView.getAwContents().loadUrl(aboutPageUrl, null));
        mContentsClient.waitForFullLoad();

        Assert.assertEquals(CommonResources.ABOUT_TITLE, mAwContents.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewRestrictSensitiveContent"})
    public void headerCanBlockRedirects() throws Throwable {
        final String webpageNotAvailable = "Webpage not available";

        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("X-Embedder-Ancestors", "*"));
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer, headers);

        String redirect_path = "/redirect.html";
        List<Pair<String, String>> blocking_headers = new ArrayList<Pair<String, String>>();
        blocking_headers.add(Pair.create("X-Embedder-Ancestors", "none"));
        final String initialUrl =
                mWebServer.setRedirect(redirect_path, aboutPageUrl, blocking_headers);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mTestContainerView.getAwContents().loadUrl(initialUrl, null));
        mContentsClient.waitForFullLoad();
        Assert.assertEquals(webpageNotAvailable, mAwContents.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewRestrictSensitiveContent"})
    public void allowDALVerifiedRedirects() throws Throwable {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("X-Embedder-Ancestors", "*"));
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer, headers);

        List<String> mSignatureFingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(context.getPackageName());
        final String assetLinksUrl =
                addAssetListToTestServer(mWebServer, mSignatureFingerprints.get(0));

        String redirect_path = "/redirect.html";
        List<Pair<String, String>> blocking_headers = new ArrayList<Pair<String, String>>();
        blocking_headers.add(Pair.create("X-Embedder-Ancestors", "none"));
        final String initialUrl =
                mWebServer.setRedirect(redirect_path, aboutPageUrl, blocking_headers);
        AwOriginVerificationScheduler.getInstance().addPendingOriginForTesting(
                Origin.create(initialUrl));

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mTestContainerView.getAwContents().loadUrl(initialUrl, null));
        mContentsClient.waitForFullLoad();
        Assert.assertEquals(CommonResources.ABOUT_TITLE, mAwContents.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add({"enable-features=WebViewRestrictSensitiveContent"})
    public void doesBlockForNotVerifiedContent() throws Throwable {
        final String webpageNotAvailable = "Webpage not available";
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create("X-Embedder-Ancestors", "none"));
        final String aboutPageUrl = addAboutPageToTestServer(mWebServer, headers);

        Set<Origin> pendingOrigins =
                AwOriginVerificationScheduler.getInstance().getPendingOriginsForTesting();
        Assert.assertFalse(pendingOrigins.contains(Origin.create(aboutPageUrl)));

        InstrumentationRegistry.getInstrumentation().runOnMainSync(
                () -> mTestContainerView.getAwContents().loadUrl(aboutPageUrl, null));
        mContentsClient.waitForFullLoad();

        Assert.assertEquals(webpageNotAvailable, mAwContents.getTitle());
    }
}
