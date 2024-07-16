// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.payments.CSPChecker;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestDownloader.ManifestDownloadCallback;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** An integration test for the payment manifest downloader. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MediumTest
public class PaymentManifestDownloaderTest implements ManifestDownloadCallback {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String EXPECTED_PAYMENT_METHOD_MANIFEST =
            "{\n" + "  \"default_applications\": [\"https://bobpay.test/app.json\"]\n" + "}\n";

    private static final String EXPECTED_WEB_APP_MANIFEST =
            "{\n"
                    + "  \"name\": \"BobPay\",\n"
                    + "  \"icons\": [{\n"
                    + "    \"src\": \"icon.png\",\n"
                    + "    \"sizes\": \"48x48\",\n"
                    + "    \"type\": \"image/png\"\n"
                    + "  }],\n"
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
    private Origin mTestOrigin;
    private EmbeddedTestServer mServer;
    private boolean mDownloadComplete;
    private boolean mDownloadPaymentMethodManifestSuccess;
    private boolean mDownloadWebAppManifestSuccess;
    private boolean mDownloadFailure;
    private String mErrorMessage;
    private String mPaymentMethodManifest;
    private String mWebAppManifest;

    @Override
    public void onPaymentMethodManifestDownloadSuccess(
            GURL paymentMethodManifestUrl, Origin paymentMethodManifestOrigin, String content) {
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
        mActivityTestRule.startMainActivityOnBlankPage();
        mServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDownloader.initialize(
                            mActivityTestRule.getActivity().getCurrentWebContents(),
                            new CSPChecker() {
                                @Override
                                public void allowConnectToSource(
                                        GURL url,
                                        GURL urlBeforeRedirects,
                                        boolean didFollowRedirect,
                                        Callback<Boolean> resultCallback) {
                                    resultCallback.onResult(/* allow= */ true);
                                }
                            });
                    mTestOrigin = PaymentManifestDownloader.createOpaqueOriginForTest();
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
        ThreadUtils.runOnUiThreadBlocking(() -> mDownloader.destroy());
    }

    @Test
    @Feature({"Payments"})
    public void testDownloadWebAppManifest() throws Throwable {
        final GURL url =
                new GURL(mServer.getURL("/components/test/data/payments/bobpay.test/app.json"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mDownloader.downloadWebAppManifest(mTestOrigin, url, this));
        CriteriaHelper.pollInstrumentationThread(() -> mDownloadComplete);

        Assert.assertTrue(
                "Web app manifest should have been downloaded.", mDownloadWebAppManifestSuccess);
        Assert.assertEquals(EXPECTED_WEB_APP_MANIFEST, mWebAppManifest);
    }

    @Test
    @Feature({"Payments"})
    public void testUnableToDownloadWebAppManifest() throws Throwable {
        final GURL url = new GURL(mServer.getURL("/no-such-app.json"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mDownloader.downloadWebAppManifest(mTestOrigin, url, this));
        CriteriaHelper.pollInstrumentationThread(() -> mDownloadComplete);

        Assert.assertTrue("Web app manifest should not have been downloaded.", mDownloadFailure);
        Assert.assertEquals(
                "Unable to download payment manifest \""
                        + url.getSpec()
                        + "\". HTTP 404 Not Found.",
                mErrorMessage);
    }

    @Test
    @Feature({"Payments"})
    public void testDownloadPaymentMethodManifest() throws Throwable {
        final GURL url =
                new GURL(mServer.getURL("/components/test/data/payments/bobpay.test/webpay"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mDownloader.downloadPaymentMethodManifest(mTestOrigin, url, this));
        CriteriaHelper.pollInstrumentationThread(() -> mDownloadComplete);

        Assert.assertTrue(
                "Payment method manifest should have been downloaded.",
                mDownloadPaymentMethodManifestSuccess);
        Assert.assertEquals(EXPECTED_PAYMENT_METHOD_MANIFEST, mPaymentMethodManifest);
    }

    @Test
    @Feature({"Payments"})
    public void testUnableToDownloadPaymentMethodManifest() throws Throwable {
        final GURL url = new GURL(mServer.getURL("/no-such-payment-method-name"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> mDownloader.downloadPaymentMethodManifest(mTestOrigin, url, this));
        CriteriaHelper.pollInstrumentationThread(() -> mDownloadComplete);

        Assert.assertTrue(
                "Payment method manifest should have not have been downloaded.", mDownloadFailure);
        Assert.assertEquals(
                "Unable to download payment manifest \""
                        + url.getSpec()
                        + "\". HTTP 404 Not Found.",
                mErrorMessage);
    }

    @Test
    @Feature({"Payments"})
    public void testSeveralDownloadsAtOnce() throws Throwable {
        final GURL paymentMethodUri1 = new GURL(mServer.getURL("/no-such-payment-method-name"));
        final GURL paymentMethodUri2 =
                new GURL(mServer.getURL("/components/test/data/payments/bobpay.test/webpay"));
        final GURL webAppUri1 = new GURL(mServer.getURL("/no-such-app.json"));
        final GURL webAppUri2 =
                new GURL(mServer.getURL("/components/test/data/payments/bobpay.test/app.json"));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDownloader.downloadPaymentMethodManifest(mTestOrigin, paymentMethodUri1, this);
                    mDownloader.downloadPaymentMethodManifest(mTestOrigin, paymentMethodUri2, this);
                    mDownloader.downloadWebAppManifest(mTestOrigin, webAppUri1, this);
                    mDownloader.downloadWebAppManifest(mTestOrigin, webAppUri2, this);
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(mDownloadWebAppManifestSuccess, Matchers.is(true));
                    Criteria.checkThat(mDownloadPaymentMethodManifestSuccess, Matchers.is(true));
                    Criteria.checkThat(mDownloadFailure, Matchers.is(true));
                });

        Assert.assertEquals(EXPECTED_PAYMENT_METHOD_MANIFEST, mPaymentMethodManifest);
        Assert.assertEquals(EXPECTED_WEB_APP_MANIFEST, mWebAppManifest);
    }
}
