// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.payments.CSPChecker;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.components.payments.PaymentManifestVerifier;
import org.chromium.components.payments.PaymentManifestVerifier.ManifestVerifyCallback;
import org.chromium.components.payments.PaymentManifestWebDataService;
import org.chromium.components.payments.PaymentManifestWebDataService.PaymentManifestWebDataServiceCallback;
import org.chromium.components.payments.WebAppManifestSection;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.HashSet;
import java.util.Set;

/** A test for the verifier of a payment app manifest. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(AndroidPaymentAppFinderUnitTest.PAYMENTS_BROWSER_UNIT_TESTS)
public class PaymentManifestVerifierTest {
    private static final String ERROR_MESSAGE = "This is an error message.";

    private Origin mTestOrigin;
    private GURL mMethodName;
    private ResolveInfo mAlicePay;
    private ResolveInfo mBobPay;
    private Set<ResolveInfo> mMatchingApps;
    private PaymentManifestDownloader mDownloader;
    private PaymentManifestParser mParser;

    // SHA256("01020304050607080900"):
    public static final byte[][] BOB_PAY_SIGNATURE_FINGERPRINTS = {
        {
            (byte) 0x9A,
            (byte) 0x89,
            (byte) 0xC6,
            (byte) 0x8C,
            (byte) 0x4C,
            (byte) 0x5E,
            (byte) 0x28,
            (byte) 0xB8,
            (byte) 0xC4,
            (byte) 0xA5,
            (byte) 0x56,
            (byte) 0x76,
            (byte) 0x73,
            (byte) 0xD4,
            (byte) 0x62,
            (byte) 0xFF,
            (byte) 0xF5,
            (byte) 0x15,
            (byte) 0xDB,
            (byte) 0x46,
            (byte) 0x11,
            (byte) 0x6F,
            (byte) 0x99,
            (byte) 0x00,
            (byte) 0x62,
            (byte) 0x4D,
            (byte) 0x09,
            (byte) 0xC4,
            (byte) 0x74,
            (byte) 0xF5,
            (byte) 0x93,
            (byte) 0xFB
        }
    };
    public static final Signature BOB_PAY_SIGNATURE = new Signature("01020304050607080900");

    @Rule public ChromeBrowserTestRule mTestRule = new ChromeBrowserTestRule();

    @Mock private PaymentManifestWebDataService mWebDataService;
    @Mock private PackageManagerDelegate mPackageManagerDelegate;
    @Mock private ManifestVerifyCallback mCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

        mTestOrigin = PaymentManifestDownloader.createOpaqueOriginForTest();
        mMethodName = new GURL("https://example.test");

        mAlicePay = new ResolveInfo();
        mAlicePay.activityInfo = new ActivityInfo();
        mAlicePay.activityInfo.packageName = "com.alicepay.app";

        mBobPay = new ResolveInfo();
        mBobPay.activityInfo = new ActivityInfo();
        mBobPay.activityInfo.packageName = "com.bobpay.app";

        mMatchingApps = new HashSet<>();
        mMatchingApps.add(mAlicePay);
        mMatchingApps.add(mBobPay);

        mDownloader =
                new PaymentManifestDownloader() {
                    @Override
                    public void initialize(WebContents webContents, CSPChecker cspChecker) {}

                    @Override
                    public void downloadPaymentMethodManifest(
                            Origin merchantOrigin, GURL url, ManifestDownloadCallback callback) {
                        callback.onPaymentMethodManifestDownloadSuccess(
                                url, mTestOrigin, "some content here");
                    }

                    @Override
                    public void downloadWebAppManifest(
                            Origin paymentMethodManifestOrigin,
                            GURL url,
                            ManifestDownloadCallback callback) {
                        callback.onWebAppManifestDownloadSuccess("some content here");
                    }

                    @Override
                    public void destroy() {}
                };

        Mockito.when(
                        mWebDataService.getPaymentMethodManifest(
                                Mockito.any(String.class),
                                Mockito.any(PaymentManifestWebDataServiceCallback.class)))
                .thenReturn(false);

        mParser =
                new PaymentManifestParser() {
                    @Override
                    public void parsePaymentMethodManifest(
                            GURL paymentMethodManifestUrl,
                            String content,
                            ManifestParseCallback callback) {
                        callback.onPaymentMethodManifestParseSuccess(
                                new GURL[] {new GURL("https://bobpay.test/app.json")}, new GURL[0]);
                    }

                    @Override
                    public void parseWebAppManifest(
                            String content, ManifestParseCallback callback) {
                        WebAppManifestSection[] manifest = new WebAppManifestSection[1];
                        int minVersion = 10;
                        manifest[0] =
                                new WebAppManifestSection(
                                        "com.bobpay.app",
                                        minVersion,
                                        BOB_PAY_SIGNATURE_FINGERPRINTS);
                        callback.onWebAppManifestParseSuccess(manifest);
                    }
                };

        PackageInfo bobPayPackageInfo = new PackageInfo();
        bobPayPackageInfo.versionCode = 10;
        bobPayPackageInfo.signatures = new Signature[1];
        bobPayPackageInfo.signatures[0] = BOB_PAY_SIGNATURE;
        Mockito.when(mPackageManagerDelegate.getPackageInfoWithSignatures("com.bobpay.app"))
                .thenReturn(bobPayPackageInfo);

        PackageInfo alicePayPackageInfo = new PackageInfo();
        alicePayPackageInfo.versionCode = 10;
        alicePayPackageInfo.signatures = new Signature[1];
        alicePayPackageInfo.signatures[0] = new Signature("ABCDEFABCDEFABCDEFAB");
        Mockito.when(mPackageManagerDelegate.getPackageInfoWithSignatures("com.alicepay.app"))
                .thenReturn(alicePayPackageInfo);
    }

    @SmallTest
    @Test
    public void testUnableToDownloadPaymentMethodManifest() {
        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(
                        mTestOrigin,
                        mMethodName,
                        mMatchingApps,
                        /* supportedOrigins= */ null,
                        mWebDataService,
                        new PaymentManifestDownloader() {
                            @Override
                            public void initialize(
                                    WebContents webContents, CSPChecker cspChecker) {}

                            @Override
                            public void downloadPaymentMethodManifest(
                                    Origin merchantOrigin,
                                    GURL url,
                                    ManifestDownloadCallback callback) {
                                callback.onManifestDownloadFailure(ERROR_MESSAGE);
                            }

                            @Override
                            public void destroy() {}
                        },
                        mParser,
                        mPackageManagerDelegate,
                        mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(GURL.class), Mockito.any(ResolveInfo.class));
    }

    @SmallTest
    @Test
    public void testUnableToDownloadWebAppManifest() {
        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(
                        mTestOrigin,
                        mMethodName,
                        mMatchingApps,
                        /* supportedOrigins= */ null,
                        mWebDataService,
                        new PaymentManifestDownloader() {
                            @Override
                            public void initialize(
                                    WebContents webContents, CSPChecker cspChecker) {}

                            @Override
                            public void downloadPaymentMethodManifest(
                                    Origin merchantOrigin,
                                    GURL url,
                                    ManifestDownloadCallback callback) {
                                callback.onPaymentMethodManifestDownloadSuccess(
                                        url, mTestOrigin, "some content");
                            }

                            @Override
                            public void downloadWebAppManifest(
                                    Origin paymentMethodManifestOrigin,
                                    GURL url,
                                    ManifestDownloadCallback callback) {
                                callback.onManifestDownloadFailure(ERROR_MESSAGE);
                            }

                            @Override
                            public void destroy() {}
                        },
                        mParser,
                        mPackageManagerDelegate,
                        mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(GURL.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
    }

    @SmallTest
    @Test
    public void testUnableToParsePaymentMethodManifest() {
        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(
                        mTestOrigin,
                        mMethodName,
                        mMatchingApps,
                        /* supportedOrigins= */ null,
                        mWebDataService,
                        mDownloader,
                        new PaymentManifestParser() {
                            @Override
                            public void parsePaymentMethodManifest(
                                    GURL paymentMethodManifestUrl,
                                    String content,
                                    ManifestParseCallback callback) {
                                callback.onManifestParseFailure();
                            }
                        },
                        mPackageManagerDelegate,
                        mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(GURL.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
    }

    @SmallTest
    @Test
    public void testUnableToParseWebAppManifest() {
        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(
                        mTestOrigin,
                        mMethodName,
                        mMatchingApps,
                        /* supportedOrigins= */ null,
                        mWebDataService,
                        mDownloader,
                        new PaymentManifestParser() {
                            @Override
                            public void parsePaymentMethodManifest(
                                    GURL paymentMethodManifestUrl,
                                    String content,
                                    ManifestParseCallback callback) {
                                callback.onPaymentMethodManifestParseSuccess(
                                        new GURL[] {new GURL("https://alicepay.test/app.json")},
                                        new GURL[0]);
                            }

                            @Override
                            public void parseWebAppManifest(
                                    String content, ManifestParseCallback callback) {
                                callback.onManifestParseFailure();
                            }
                        },
                        mPackageManagerDelegate,
                        mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(GURL.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
    }

    @SmallTest
    @Test
    public void testBobPayAllowed() {
        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(
                        mTestOrigin,
                        mMethodName,
                        mMatchingApps,
                        /* supportedOrigins= */ null,
                        mWebDataService,
                        mDownloader,
                        mParser,
                        mPackageManagerDelegate,
                        mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never()).onValidDefaultPaymentApp(mMethodName, mAlicePay);
        Mockito.verify(mCallback).onValidDefaultPaymentApp(mMethodName, mBobPay);
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
    }

    private static class CountingParser extends PaymentManifestParser {
        public int mParseWebAppManifestCounter;
    }

    private static class CountingDownloader extends PaymentManifestDownloader {
        public int mDownloadWebAppManifestCounter;
    }

    /** If a single web app manifest fails to download, all downloads should be aborted. */
    @SmallTest
    @Test
    public void testFirstOfTwoManifestsFailsToDownload() {
        CountingParser parser =
                new CountingParser() {
                    @Override
                    public void parsePaymentMethodManifest(
                            GURL paymentMethodManifestUrl,
                            String content,
                            ManifestParseCallback callback) {
                        callback.onPaymentMethodManifestParseSuccess(
                                new GURL[] {
                                    new GURL("https://alicepay.test/app.json"),
                                    new GURL("https://bobpay.test/app.json")
                                },
                                new GURL[0]);
                    }

                    @Override
                    public void parseWebAppManifest(
                            String content, ManifestParseCallback callback) {
                        mParseWebAppManifestCounter++;
                        callback.onManifestParseFailure();
                    }
                };

        CountingDownloader downloader =
                new CountingDownloader() {
                    @Override
                    public void downloadPaymentMethodManifest(
                            Origin merchantOrigin, GURL url, ManifestDownloadCallback callback) {
                        callback.onPaymentMethodManifestDownloadSuccess(
                                url, mTestOrigin, "some content");
                    }

                    @Override
                    public void downloadWebAppManifest(
                            Origin paymentMethodManifestOrigin,
                            GURL url,
                            ManifestDownloadCallback callback) {
                        if (mDownloadWebAppManifestCounter++ == 0) {
                            callback.onManifestDownloadFailure(ERROR_MESSAGE);
                        } else {
                            callback.onWebAppManifestDownloadSuccess("some content");
                        }
                    }
                };

        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(
                        mTestOrigin,
                        mMethodName,
                        mMatchingApps,
                        /* supportedOrigins= */ null,
                        mWebDataService,
                        downloader,
                        parser,
                        mPackageManagerDelegate,
                        mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(GURL.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
        Assert.assertEquals(1, downloader.mDownloadWebAppManifestCounter);
        Assert.assertEquals(0, parser.mParseWebAppManifestCounter);
    }

    /** If a single web app manifest fails to parse, all downloads should be aborted. */
    @SmallTest
    @Test
    public void testFirstOfTwoManifestsFailsToParse() {
        CountingParser parser =
                new CountingParser() {
                    @Override
                    public void parsePaymentMethodManifest(
                            GURL paymentMethodManifestUrl,
                            String content,
                            ManifestParseCallback callback) {
                        callback.onPaymentMethodManifestParseSuccess(
                                new GURL[] {
                                    new GURL("https://alicepay.test/app.json"),
                                    new GURL("https://bobpay.test/app.json")
                                },
                                new GURL[0]);
                    }

                    @Override
                    public void parseWebAppManifest(
                            String content, ManifestParseCallback callback) {
                        if (mParseWebAppManifestCounter++ == 0) {
                            callback.onManifestParseFailure();
                        } else {
                            callback.onWebAppManifestParseSuccess(new WebAppManifestSection[0]);
                        }
                    }
                };

        CountingDownloader downloader =
                new CountingDownloader() {
                    @Override
                    public void downloadPaymentMethodManifest(
                            Origin merchantOrigin, GURL url, ManifestDownloadCallback callback) {
                        callback.onPaymentMethodManifestDownloadSuccess(
                                url, mTestOrigin, "some content");
                    }

                    @Override
                    public void downloadWebAppManifest(
                            Origin paymentMethodManifestOrigin,
                            GURL url,
                            ManifestDownloadCallback callback) {
                        mDownloadWebAppManifestCounter++;
                        callback.onWebAppManifestDownloadSuccess("some content");
                    }
                };

        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(
                        mTestOrigin,
                        mMethodName,
                        mMatchingApps,
                        /* supportedOrigins= */ null,
                        mWebDataService,
                        downloader,
                        parser,
                        mPackageManagerDelegate,
                        mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(GURL.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
        Assert.assertEquals(1, downloader.mDownloadWebAppManifestCounter);
        Assert.assertEquals(1, parser.mParseWebAppManifestCounter);
    }
}
