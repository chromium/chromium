// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.pm.ActivityInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.payments.PaymentManifestVerifier.ManifestVerifyCallback;
import org.chromium.chrome.browser.payments.PaymentManifestWebDataService.PaymentManifestWebDataServiceCallback;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.components.payments.WebAppManifestSection;
import org.chromium.content_public.browser.WebContents;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.HashSet;
import java.util.Set;

/** A test for the verifier of a payment app manifest. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = 21, manifest = Config.NONE)
public class PaymentManifestVerifierTest {
    private static final String ERROR_MESSAGE = "This is an error message.";

    private final URI mMethodName;
    private final ResolveInfo mAlicePay;
    private final ResolveInfo mBobPay;
    private final Set<ResolveInfo> mMatchingApps;
    private final PaymentManifestDownloader mDownloader;
    private final PaymentManifestWebDataService mWebDataService;
    private final PaymentManifestParser mParser;
    private final PackageManagerDelegate mPackageManagerDelegate;
    private final ManifestVerifyCallback mCallback;

    // SHA256("01020304050607080900"):
    public final static byte[][] BOB_PAY_SIGNATURE_FINGERPRINTS = {{(byte) 0x9A, (byte) 0x89,
            (byte) 0xC6, (byte) 0x8C, (byte) 0x4C, (byte) 0x5E, (byte) 0x28, (byte) 0xB8,
            (byte) 0xC4, (byte) 0xA5, (byte) 0x56, (byte) 0x76, (byte) 0x73, (byte) 0xD4,
            (byte) 0x62, (byte) 0xFF, (byte) 0xF5, (byte) 0x15, (byte) 0xDB, (byte) 0x46,
            (byte) 0x11, (byte) 0x6F, (byte) 0x99, (byte) 0x00, (byte) 0x62, (byte) 0x4D,
            (byte) 0x09, (byte) 0xC4, (byte) 0x74, (byte) 0xF5, (byte) 0x93, (byte) 0xFB}};
    public final static Signature BOB_PAY_SIGNATURE = new Signature("01020304050607080900");

    public PaymentManifestVerifierTest() throws URISyntaxException {
        mMethodName = new URI("https://example.com");

        mAlicePay = new ResolveInfo();
        mAlicePay.activityInfo = new ActivityInfo();
        mAlicePay.activityInfo.packageName = "com.alicepay.app";

        mBobPay = new ResolveInfo();
        mBobPay.activityInfo = new ActivityInfo();
        mBobPay.activityInfo.packageName = "com.bobpay.app";

        mMatchingApps = new HashSet<>();
        mMatchingApps.add(mAlicePay);
        mMatchingApps.add(mBobPay);

        mDownloader = new PaymentManifestDownloader() {
            @Override
            public void initialize(WebContents webContents) {}

            @Override
            public void downloadPaymentMethodManifest(URI uri, ManifestDownloadCallback callback) {
                callback.onPaymentMethodManifestDownloadSuccess("some content here");
            }

            @Override
            public void downloadWebAppManifest(URI uri, ManifestDownloadCallback callback) {
                callback.onWebAppManifestDownloadSuccess("some content here");
            }

            @Override
            public void destroy() {}
        };

        mWebDataService = Mockito.mock(PaymentManifestWebDataService.class);
        Mockito.when(mWebDataService.getPaymentMethodManifest(Mockito.any(String.class),
                             Mockito.any(PaymentManifestWebDataServiceCallback.class)))
                .thenReturn(false);

        mParser = new PaymentManifestParser() {
            @Override
            public void parsePaymentMethodManifest(String content, ManifestParseCallback callback) {
                try {
                    callback.onPaymentMethodManifestParseSuccess(
                            new URI[] {new URI("https://bobpay.com/app.json")}, new URI[0], false);
                } catch (URISyntaxException e) {
                    assert false;
                }
            }

            @Override
            public void parseWebAppManifest(String content, ManifestParseCallback callback) {
                WebAppManifestSection[] manifest = new WebAppManifestSection[1];
                int minVersion = 10;
                manifest[0] = new WebAppManifestSection(
                        "com.bobpay.app", minVersion, BOB_PAY_SIGNATURE_FINGERPRINTS);
                callback.onWebAppManifestParseSuccess(manifest);
            }
        };

        mPackageManagerDelegate = Mockito.mock(PackageManagerDelegate.class);

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

        mCallback = Mockito.mock(ManifestVerifyCallback.class);
    }

    @Test
    public void testUnableToDownloadPaymentMethodManifest() {
        PaymentManifestVerifier verifier = new PaymentManifestVerifier(mMethodName, mMatchingApps,
                null /* supportedOrigins */, mWebDataService, new PaymentManifestDownloader() {
                    @Override
                    public void initialize(WebContents webContents) {}

                    @Override
                    public void downloadPaymentMethodManifest(
                            URI uri, ManifestDownloadCallback callback) {
                        callback.onManifestDownloadFailure(ERROR_MESSAGE);
                    }

                    @Override
                    public void destroy() {}
                }, mParser, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(URI.class), Mockito.any(ResolveInfo.class));
    }

    @Test
    public void testUnableToDownloadWebAppManifest() {
        PaymentManifestVerifier verifier = new PaymentManifestVerifier(mMethodName, mMatchingApps,
                null /* supportedOrigins */, mWebDataService, new PaymentManifestDownloader() {
                    @Override
                    public void initialize(WebContents webContents) {}

                    @Override
                    public void downloadPaymentMethodManifest(
                            URI uri, ManifestDownloadCallback callback) {
                        callback.onPaymentMethodManifestDownloadSuccess("some content");
                    }

                    @Override
                    public void downloadWebAppManifest(URI uri, ManifestDownloadCallback callback) {
                        callback.onManifestDownloadFailure(ERROR_MESSAGE);
                    }

                    @Override
                    public void destroy() {}
                }, mParser, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(URI.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
    }

    @Test
    public void testUnableToParsePaymentMethodManifest() {
        PaymentManifestVerifier verifier = new PaymentManifestVerifier(mMethodName, mMatchingApps,
                null /* supportedOrigins */, mWebDataService,
                mDownloader, new PaymentManifestParser() {
                    @Override
                    public void parsePaymentMethodManifest(
                            String content, ManifestParseCallback callback) {
                        callback.onManifestParseFailure();
                    }
                }, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(URI.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
    }

    @Test
    public void testUnableToParseWebAppManifest() {
        PaymentManifestVerifier verifier = new PaymentManifestVerifier(mMethodName, mMatchingApps,
                null /* supportedOrigins */, mWebDataService,
                mDownloader, new PaymentManifestParser() {
                    @Override
                    public void parsePaymentMethodManifest(
                            String content, ManifestParseCallback callback) {
                        try {
                            callback.onPaymentMethodManifestParseSuccess(
                                    new URI[] {new URI("https://alicepay.com/app.json")},
                                    new URI[0], false);
                        } catch (URISyntaxException e) {
                            Assert.assertTrue(false);
                        }
                    }

                    @Override
                    public void parseWebAppManifest(
                            String content, ManifestParseCallback callback) {
                        callback.onManifestParseFailure();
                    }
                }, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(URI.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
    }

    @Test
    public void testBobPayAllowed() {
        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(mMethodName, mMatchingApps, null /* supportedOrigins */,
                        mWebDataService, mDownloader, mParser, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never()).onValidDefaultPaymentApp(mMethodName, mAlicePay);
        Mockito.verify(mCallback).onValidDefaultPaymentApp(mMethodName, mBobPay);
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
    }

    private class CountingParser extends PaymentManifestParser {
        public int mParseWebAppManifestCounter;
    }

    private class CountingDownloader extends PaymentManifestDownloader {
        public int mDownloadWebAppManifestCounter;
    }

    /** If a single web app manifest fails to download, all downloads should be aborted. */
    @Test
    public void testFirstOfTwoManifestsFailsToDownload() {
        CountingParser parser = new CountingParser() {
            @Override
            public void parsePaymentMethodManifest(String content, ManifestParseCallback callback) {
                try {
                    callback.onPaymentMethodManifestParseSuccess(
                            new URI[] {new URI("https://alicepay.com/app.json"),
                                    new URI("https://bobpay.com/app.json")},
                            new URI[0], false);
                } catch (URISyntaxException e) {
                    Assert.assertTrue(false);
                }
            }

            @Override
            public void parseWebAppManifest(String content, ManifestParseCallback callback) {
                mParseWebAppManifestCounter++;
                callback.onManifestParseFailure();
            }
        };

        CountingDownloader downloader = new CountingDownloader() {
            @Override
            public void downloadPaymentMethodManifest(URI uri, ManifestDownloadCallback callback) {
                callback.onPaymentMethodManifestDownloadSuccess("some content");
            }

            @Override
            public void downloadWebAppManifest(URI uri, ManifestDownloadCallback callback) {
                if (mDownloadWebAppManifestCounter++ == 0) {
                    callback.onManifestDownloadFailure(ERROR_MESSAGE);
                } else {
                    callback.onWebAppManifestDownloadSuccess("some content");
                }
            }
        };

        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(mMethodName, mMatchingApps, null /* supportedOrigins */,
                        mWebDataService, downloader, parser, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(URI.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
        Assert.assertEquals(1, downloader.mDownloadWebAppManifestCounter);
        Assert.assertEquals(0, parser.mParseWebAppManifestCounter);
    }

    /** If a single web app manifest fails to parse, all downloads should be aborted. */
    @Test
    public void testFirstOfTwoManifestsFailsToParse() {
        CountingParser parser = new CountingParser() {
            @Override
            public void parsePaymentMethodManifest(String content, ManifestParseCallback callback) {
                try {
                    callback.onPaymentMethodManifestParseSuccess(
                            new URI[] {new URI("https://alicepay.com/app.json"),
                                    new URI("https://bobpay.com/app.json")},
                            new URI[0], false);
                } catch (URISyntaxException e) {
                    Assert.assertTrue(false);
                }
            }

            @Override
            public void parseWebAppManifest(String content, ManifestParseCallback callback) {
                if (mParseWebAppManifestCounter++ == 0) {
                    callback.onManifestParseFailure();
                } else {
                    callback.onWebAppManifestParseSuccess(new WebAppManifestSection[0]);
                }
            }
        };

        CountingDownloader downloader = new CountingDownloader() {
            @Override
            public void downloadPaymentMethodManifest(URI uri, ManifestDownloadCallback callback) {
                callback.onPaymentMethodManifestDownloadSuccess("some content");
            }

            @Override
            public void downloadWebAppManifest(URI uri, ManifestDownloadCallback callback) {
                mDownloadWebAppManifestCounter++;
                callback.onWebAppManifestDownloadSuccess("some content");
            }
        };

        PaymentManifestVerifier verifier =
                new PaymentManifestVerifier(mMethodName, mMatchingApps, null /* supportedOrigins */,
                        mWebDataService, downloader, parser, mPackageManagerDelegate, mCallback);

        verifier.verify();

        Mockito.verify(mCallback, Mockito.never())
                .onValidDefaultPaymentApp(Mockito.any(URI.class), Mockito.any(ResolveInfo.class));
        Mockito.verify(mCallback).onFinishedVerification();
        Mockito.verify(mCallback).onFinishedUsingResources();
        Assert.assertEquals(1, downloader.mDownloadWebAppManifestCounter);
        Assert.assertEquals(1, parser.mParseWebAppManifestCounter);
    }
}
