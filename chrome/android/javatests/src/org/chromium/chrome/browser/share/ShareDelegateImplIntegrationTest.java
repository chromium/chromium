// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareSheetDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.ui_metrics.CanonicalURLResult;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Integration tests for the Share Menu handling. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShareDelegateImplIntegrationTest {
    private static final String PAGE_WITH_HTTPS_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_https_canonical.html";
    private static final String PAGE_WITH_HTTP_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_http_canonical.html";
    private static final String PAGE_WITH_NO_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_no_canonical.html";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Test
    @SmallTest
    public void testCanonicalUrlsOverHttps() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        final String httpsCanonicalUrl = testServer.getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        final String httpCanonicalUrl = testServer.getURL(PAGE_WITH_HTTP_CANONICAL_URL);
        final String noCanonicalUrl = testServer.getURL(PAGE_WITH_NO_CANONICAL_URL);

        verifyShareUrl(
                httpsCanonicalUrl,
                "https://examplehttps.com/",
                CanonicalURLResult.SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE);
        verifyShareUrl(
                httpCanonicalUrl,
                "http://examplehttp.com/",
                CanonicalURLResult.SUCCESS_CANONICAL_URL_NOT_HTTPS);
        verifyShareUrl(
                noCanonicalUrl, noCanonicalUrl, CanonicalURLResult.FAILED_NO_CANONICAL_URL_DEFINED);
    }

    @Test
    @SmallTest
    public void testCanonicalUrlsOverHttp() throws TimeoutException {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        final String httpsCanonicalUrl = testServer.getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        final String httpCanonicalUrl = testServer.getURL(PAGE_WITH_HTTP_CANONICAL_URL);
        final String noCanonicalUrl = testServer.getURL(PAGE_WITH_NO_CANONICAL_URL);

        verifyShareUrl(
                httpsCanonicalUrl,
                httpsCanonicalUrl,
                CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS);
        verifyShareUrl(
                httpCanonicalUrl,
                httpCanonicalUrl,
                CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS);
        verifyShareUrl(
                noCanonicalUrl, noCanonicalUrl, CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS);
    }

    private void verifyShareUrl(
            String pageUrl, String expectedShareUrl, @CanonicalURLResult int expectedUrlResult)
            throws IllegalArgumentException, TimeoutException {
        sActivityTestRule.loadUrl(pageUrl);
        var urlResultHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        ShareDelegateImpl.CANONICAL_URL_RESULT_HISTOGRAM, expectedUrlResult);
        ShareParams params = triggerShare();
        Assert.assertTrue(params.getTextAndUrl().contains(expectedShareUrl));
        urlResultHistogram.assertExpected();
    }

    private ShareParams triggerShare() throws TimeoutException {
        final CallbackHelper helper = new CallbackHelper();
        final AtomicReference<ShareParams> paramsRef = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ShareSheetDelegate delegate =
                            new ShareSheetDelegate() {
                                @Override
                                void share(
                                        ShareParams params,
                                        ChromeShareExtras chromeShareParams,
                                        BottomSheetController controller,
                                        ActivityLifecycleDispatcher lifecycleDispatcher,
                                        Supplier<Tab> tabProvider,
                                        Supplier<TabModelSelector> tabModelSelectorProvider,
                                        Supplier<Profile> profileSupplier,
                                        Callback<Tab> printCallback,
                                        int shareOrigin,
                                        long shareStartTime,
                                        boolean sharingHubEnabled) {
                                    paramsRef.set(params);
                                    helper.notifyCalled();
                                }
                            };

                    new ShareDelegateImpl(
                                    sActivityTestRule
                                            .getActivity()
                                            .getRootUiCoordinatorForTesting()
                                            .getBottomSheetController(),
                                    sActivityTestRule.getActivity().getLifecycleDispatcher(),
                                    sActivityTestRule.getActivity().getActivityTabProvider(),
                                    sActivityTestRule.getActivity().getTabModelSelectorSupplier(),
                                    new ObservableSupplierImpl<>(),
                                    delegate,
                                    false)
                            .share(
                                    sActivityTestRule.getActivity().getActivityTab(),
                                    false,
                                    /* shareOrigin= */ 0);
                });
        helper.waitForCallback(0);
        return paramsRef.get();
    }
}
