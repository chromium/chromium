// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.share.ShareSheetMediator.ShareSheetDelegate;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.ui_metrics.CanonicalURLResult;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for the Share Menu handling.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShareSheetMediatorIntegrationTest {
    private static final String PAGE_WITH_HTTPS_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_https_canonical.html";
    private static final String PAGE_WITH_HTTP_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_http_canonical.html";
    private static final String PAGE_WITH_NO_CANONICAL_URL =
            "/chrome/test/data/android/share/link_share_no_canonical.html";

    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @Test
    @SmallTest
    public void testCanonicalUrlsOverHttps() throws TimeoutException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartHTTPSServer(
                InstrumentationRegistry.getInstrumentation().getContext(),
                ServerCertificate.CERT_OK);
        final String httpsCanonicalUrl = testServer.getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        final String httpCanonicalUrl = testServer.getURL(PAGE_WITH_HTTP_CANONICAL_URL);
        final String noCanonicalUrl = testServer.getURL(PAGE_WITH_NO_CANONICAL_URL);

        try {
            verifyShareUrl(httpsCanonicalUrl, "https://examplehttps.com/",
                    CanonicalURLResult.SUCCESS_CANONICAL_URL_DIFFERENT_FROM_VISIBLE);
            verifyShareUrl(httpCanonicalUrl, "http://examplehttp.com/",
                    CanonicalURLResult.SUCCESS_CANONICAL_URL_NOT_HTTPS);
            verifyShareUrl(noCanonicalUrl, noCanonicalUrl,
                    CanonicalURLResult.FAILED_NO_CANONICAL_URL_DEFINED);
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    @Test
    @SmallTest
    public void testCanonicalUrlsOverHttp() throws TimeoutException {
        EmbeddedTestServer testServer = EmbeddedTestServer.createAndStartServer(
                InstrumentationRegistry.getInstrumentation().getContext());
        final String httpsCanonicalUrl = testServer.getURL(PAGE_WITH_HTTPS_CANONICAL_URL);
        final String httpCanonicalUrl = testServer.getURL(PAGE_WITH_HTTP_CANONICAL_URL);
        final String noCanonicalUrl = testServer.getURL(PAGE_WITH_NO_CANONICAL_URL);

        try {
            verifyShareUrl(httpsCanonicalUrl, httpsCanonicalUrl,
                    CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS);
            verifyShareUrl(httpCanonicalUrl, httpCanonicalUrl,
                    CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS);
            verifyShareUrl(noCanonicalUrl, noCanonicalUrl,
                    CanonicalURLResult.FAILED_VISIBLE_URL_NOT_HTTPS);
        } finally {
            testServer.stopAndDestroyServer();
        }
    }

    private void verifyShareUrl(
            String pageUrl, String expectedShareUrl, @CanonicalURLResult int expectedUrlResult)
            throws IllegalArgumentException, TimeoutException {
        mActivityTestRule.loadUrl(pageUrl);
        HistogramDelta urlResultDelta = new HistogramDelta(
                ShareSheetMediator.CANONICAL_URL_RESULT_HISTOGRAM, expectedUrlResult);
        ShareParams params = triggerShare();
        Assert.assertEquals(expectedShareUrl, params.getUrl());
        Assert.assertEquals(1, urlResultDelta.getDelta());
    }

    private ShareParams triggerShare() throws TimeoutException {
        final CallbackHelper helper = new CallbackHelper();
        final AtomicReference<ShareParams> paramsRef = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShareSheetDelegate delegate = new ShareSheetDelegate() {
                @Override
                void share(ShareParams params, BottomSheetController controller) {
                    paramsRef.set(params);
                    helper.notifyCalled();
                }
            };

            new ShareSheetMediator(
                    delegate, mActivityTestRule.getActivity().getBottomSheetController())
                    .onShareSelected(mActivityTestRule.getActivity(),
                            mActivityTestRule.getActivity().getActivityTab(), false, false);
        });
        helper.waitForCallback(0);
        return paramsRef.get();
    }
}
