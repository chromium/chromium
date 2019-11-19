// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestDownloader.ManifestDownloadCallback;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;

import java.net.URI;

/** An integration test for the payment manifest downloader. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MediumTest
public class PaymentManifestDownloaderTest implements ManifestDownloadCallback {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private static final String EXPECTED_PAYMENT_METHOD_MANIFEST = "{\n"
            + "  \"default_applications\": [\"https://bobpay.com/app.json\"]\n"
            + "}\n";

    private static final String EXPECTED_WEB_APP_MANIFEST = "{\n"
            + "  \"name\": \"BobPay\",\n"
            + "  \"related_applications\": [{\n"
            + "    \"platform\": \"play\",\n"
            + "    \"id\": \"com.bobpay\",\n"
            + "    \"min_version\": \"1\",\n"
            + "    \"fingerprints\": [{\n"
            + "      \"type\": \"sha256_cert\",\n"
            + "      \"value\": \"9A:89:C6:8C:4C:5E:28:B8:C4:A5:56:76:73:D4:62:"
            + "FF:F5:15:DB:46:11:6F:99:00:62:4D:09:C4:74:F5:93:FB\",\n"
            + "      \"comment\": \"This fingperint is SHA256 of '01020304050607080900'\"\n"
            + "    }]\n"
            + "  }]\n"
            + "}\n";

    private final PaymentManifestDownloader mDownloader = new PaymentManifestDownloader();
    private EmbeddedTestServer mServer;
    private boolean mDownloadComplete;
    private boolean mDownloadPaymentMethodManifestSuccess;
    private boolean mDownloadWebAppManifestSuccess;
    private boolean mDownloadFailure;
    private String mErrorMessage;
    private String mPaymentMethodManifest;
    private String mWebAppManifest;

    @Override
    public void onPaymentMethodManifestDownloadSuccess(String content) {
        mDownloadComplete = true;
        mDownloadPaymentMethodManifestSuccess = true;
        mPaymentMethodManifest = content;
    }

    @Override
    public void onWebAppManifestDownloadSuccess(String content) {
        mDownloadComplete = true;
        mDownloadWebAppManifestSuccess = true;
        mWebAppManifest = content;
    }

    @Override
    public void onManifestDownloadFailure(String errorMessage) {
        mDownloadComplete = true;
        mDownloadFailure = true;
        mErrorMessage = errorMessage;
    }

    @Before
    public void setUp() throws Throwable {
        mRule.startMainActivityOnBlankPage();
        mServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mRule.runOnUiThread((Runnable) () -> {
            mDownloader.initialize(mRule.getActivity().getCurrentWebContents());
        });
        mDownloadComplete = false;
        mDownloadPaymentMethodManifestSuccess = false;
        mDownloadWebAppManifestSuccess = false;
        mDownloadFailure = false;
        mErrorMessage = "";
        mPaymentMethodManifest = null;
        mWebAppManifest = null;
    }

    @After
    public void tearDown() throws Throwable {
        mRule.runOnUiThread((Runnable) () -> mDownloader.destroy());
        mServer.stopAndDestroyServer();
    }

    @Test
    @Feature({"Payments"})
    public void testDownloadWebAppManifest() throws Throwable {
        final URI uri =
                new URI(mServer.getURL("/components/test/data/payments/bobpay.com/app.json"));
        mRule.runOnUiThread((Runnable) () -> mDownloader.downloadWebAppManifest(uri,
                PaymentManifestDownloaderTest.this));
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadComplete;
            }
        });

        Assert.assertTrue(
                "Web app manifest should have been downloaded.", mDownloadWebAppManifestSuccess);
        Assert.assertEquals(EXPECTED_WEB_APP_MANIFEST, mWebAppManifest);
    }

    @Test
    @Feature({"Payments"})
    public void testUnableToDownloadWebAppManifest() throws Throwable {
        final URI uri = new URI(mServer.getURL("/no-such-app.json"));
        mRule.runOnUiThread((Runnable) () -> mDownloader.downloadWebAppManifest(uri,
                PaymentManifestDownloaderTest.this));
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadComplete;
            }
        });

        Assert.assertTrue("Web app manifest should not have been downloaded.", mDownloadFailure);
        Assert.assertEquals(
                "Unable to download payment manifest \"" + uri.toString() + "\".", mErrorMessage);
    }

    @Test
    @Feature({"Payments"})
    public void testDownloadPaymentMethodManifest() throws Throwable {
        final URI uri = new URI(mServer.getURL("/components/test/data/payments/bobpay.com/webpay"));
        mRule.runOnUiThread((Runnable) () -> mDownloader.downloadPaymentMethodManifest(uri,
                PaymentManifestDownloaderTest.this));
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadComplete;
            }
        });

        Assert.assertTrue("Payment method manifest should have been downloaded.",
                mDownloadPaymentMethodManifestSuccess);
        Assert.assertEquals(EXPECTED_PAYMENT_METHOD_MANIFEST, mPaymentMethodManifest);
    }

    @Test
    @Feature({"Payments"})
    public void testUnableToDownloadPaymentMethodManifest() throws Throwable {
        final URI uri = new URI(mServer.getURL("/no-such-payment-method-name"));
        mRule.runOnUiThread((Runnable) () -> mDownloader.downloadPaymentMethodManifest(uri,
                PaymentManifestDownloaderTest.this));
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadComplete;
            }
        });

        Assert.assertTrue(
                "Payment method manifest should have not have been downloaded.", mDownloadFailure);
        Assert.assertEquals("Unable to make a HEAD request to \"" + uri.toString()
                        + "\" for payment method manifest.",
                mErrorMessage);
    }

    @Test
    @Feature({"Payments"})
    public void testSeveralDownloadsAtOnce() throws Throwable {
        final URI paymentMethodUri1 = new URI(mServer.getURL("/no-such-payment-method-name"));
        final URI paymentMethodUri2 =
                new URI(mServer.getURL("/components/test/data/payments/bobpay.com/webpay"));
        final URI webAppUri1 = new URI(mServer.getURL("/no-such-app.json"));
        final URI webAppUri2 =
                new URI(mServer.getURL("/components/test/data/payments/bobpay.com/app.json"));
        mRule.runOnUiThread((Runnable) () -> {
            mDownloader.downloadPaymentMethodManifest(
                    paymentMethodUri1, PaymentManifestDownloaderTest.this);
            mDownloader.downloadPaymentMethodManifest(
                    paymentMethodUri2, PaymentManifestDownloaderTest.this);
            mDownloader.downloadWebAppManifest(webAppUri1, PaymentManifestDownloaderTest.this);
            mDownloader.downloadWebAppManifest(webAppUri2, PaymentManifestDownloaderTest.this);
        });
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return mDownloadWebAppManifestSuccess && mDownloadPaymentMethodManifestSuccess
                        && mDownloadFailure;
            }
        });

        Assert.assertEquals(EXPECTED_PAYMENT_METHOD_MANIFEST, mPaymentMethodManifest);
        Assert.assertEquals(EXPECTED_WEB_APP_MANIFEST, mWebAppManifest);
    }
}
