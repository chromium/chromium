// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryParams;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.components.payments.WebAppManifestSection;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Tests for the native Android payment app finder. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(AndroidPaymentAppFinderUnitTest.PAYMENTS_BROWSER_UNIT_TESTS)
public class AndroidPaymentAppFinderUnitTest extends DummyUiActivityTestCase {
    // Collection of payments unit tests that require the browser process to be initialized.
    static final String PAYMENTS_BROWSER_UNIT_TESTS = "PaymentsBrowserUnitTests";
    private static final IntentArgumentMatcher sPayIntentArgumentMatcher =
            new IntentArgumentMatcher(new Intent("org.chromium.intent.action.PAY"));

    @Rule
    public ChromeBrowserTestRule mTestRule = new ChromeBrowserTestRule();

    @Mock
    private PaymentManifestWebDataService mPaymentManifestWebDataService;
    @Mock
    private PaymentManifestDownloader mPaymentManifestDownloader;
    @Mock
    private PaymentManifestParser mPaymentManifestParser;
    @Mock
    private PackageManagerDelegate mPackageManagerDelegate;

    private WindowAndroid mWindowAndroid;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mWindowAndroid = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return new ActivityWindowAndroid(getActivity()); });

        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> { mWindowAndroid.destroy(); });
    }

    /**
     * Argument matcher that matches Intents using |filterEquals| method.
     */
    private static class IntentArgumentMatcher implements ArgumentMatcher<Intent> {
        private final Intent mIntent;

        public IntentArgumentMatcher(Intent intent) {
            mIntent = intent;
        }

        @Override
        public boolean matches(Intent other) {
            return mIntent.filterEquals(other);
        }

        @Override
        public String toString() {
            return mIntent.toString();
        }
    }

    private PaymentAppFactoryDelegate findApps(String[] methodNames,
            PaymentManifestDownloader downloader, PaymentManifestParser parser,
            PackageManagerDelegate packageManagerDelegate) {
        Map<String, PaymentMethodData> methodData = new HashMap<>();
        for (String methodName : methodNames) {
            PaymentMethodData data = new PaymentMethodData();
            data.supportedMethod = methodName;
            data.stringifiedData = "{\"key\":\"value\"}";
            methodData.put(methodName, data);
        }
        PaymentAppFactoryParams params = Mockito.mock(PaymentAppFactoryParams.class);
        WebContents webContents = Mockito.mock(WebContents.class);
        TabModelSelector tabModelSelector = Mockito.mock(TabModelSelector.class);
        TabModelSelectorSupplier.setInstanceForTesting(tabModelSelector);
        Mockito.when(tabModelSelector.isIncognitoSelected()).thenReturn(false);
        Mockito.when(webContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        Mockito.when(params.getWebContents()).thenReturn(webContents);
        Mockito.when(params.getId()).thenReturn("id");
        Mockito.when(params.getMethodData()).thenReturn(methodData);
        Mockito.when(params.getTopLevelOrigin()).thenReturn("https://chromium.org");
        Mockito.when(params.getPaymentRequestOrigin()).thenReturn("https://chromium.org");
        Mockito.when(params.getCertificateChain()).thenReturn(null);
        Mockito.when(params.getUnmodifiableModifiers())
                .thenReturn(new HashMap<String, PaymentDetailsModifier>());
        Mockito.when(params.getMayCrawl()).thenReturn(false);
        PaymentAppFactoryDelegate delegate = Mockito.mock(PaymentAppFactoryDelegate.class);
        Mockito.when(delegate.getParams()).thenReturn(params);
        AndroidPaymentAppFinder finder =
                new AndroidPaymentAppFinder(mPaymentManifestWebDataService, downloader, parser,
                        packageManagerDelegate, new TwaPackageManagerDelegate(), delegate,
                        /*factory=*/null);
        finder.bypassIsReadyToPayServiceInTest();
        finder.findAndroidPaymentApps();
        return delegate;
    }

    private void verifyNoAppsFound(PaymentAppFactoryDelegate delegate) {
        Mockito.verify(delegate, Mockito.never())
                .onPaymentAppCreated(Mockito.any(PaymentApp.class));
        Mockito.verify(delegate, Mockito.never())
                .onPaymentAppCreationError(Mockito.any(String.class));
        Mockito.verify(delegate).onCanMakePaymentCalculated(false);
        Mockito.verify(delegate).onDoneCreatingPaymentApps(/*factory=*/null);
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testNoValidPaymentMethodNames() {
        verifyNoAppsFound(findApps(new String[] {"unknown-payment-method-name",
                                           "http://not.secure.payment.method.name.com", "https://"},
                mPaymentManifestDownloader, mPaymentManifestParser, mPackageManagerDelegate));
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testQueryWithoutApps() {
        Mockito.when(mPackageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(new ArrayList<ResolveInfo>());

        verifyNoAppsFound(findApps(new String[] {"basic-card"}, mPaymentManifestDownloader,
                mPaymentManifestParser, mPackageManagerDelegate));

        Mockito.verify(mPackageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testQueryWithoutMetaData() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        activities.add(alicePay);

        Mockito.when(mPackageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(mPackageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        verifyNoAppsFound(findApps(new String[] {"basic-card"}, mPaymentManifestDownloader,
                mPaymentManifestParser, mPackageManagerDelegate));

        Mockito.verify(mPackageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testQueryWithoutLabel() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        Bundle activityMetaData = new Bundle();
        activityMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        alicePay.activityInfo.metaData = activityMetaData;
        activities.add(alicePay);

        Mockito.when(mPackageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        verifyNoAppsFound(findApps(new String[] {"basic-card"}, mPaymentManifestDownloader,
                mPaymentManifestParser, mPackageManagerDelegate));

        Mockito.verify(mPackageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testQueryUnsupportedPaymentMethod() {
        PackageManagerDelegate packageManagerDelegate = installPaymentApps(
                new String[] {"com.alicepay.app"}, new String[] {"unsupported-payment-method"});

        verifyNoAppsFound(findApps(new String[] {"unsupported-payment-method"},
                mPaymentManifestDownloader, mPaymentManifestParser, packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    private PackageManagerDelegate installPaymentApps(String[] packageNames, String[] methodNames) {
        assert packageNames.length == methodNames.length;
        List<ResolveInfo> activities = new ArrayList<>();
        for (int i = 0; i < packageNames.length; i++) {
            ResolveInfo alicePay = new ResolveInfo();
            alicePay.activityInfo = new ActivityInfo();
            alicePay.activityInfo.packageName = packageNames[i];
            alicePay.activityInfo.name = packageNames[i] + ".WebPaymentActivity";
            Bundle activityMetaData = new Bundle();
            activityMetaData.putString(
                    AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                    methodNames[i]);
            alicePay.activityInfo.metaData = activityMetaData;
            activities.add(alicePay);
        }

        Mockito.when(mPackageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(mPackageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);
        return mPackageManagerDelegate;
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testQueryDifferentPaymentMethod() {
        PackageManagerDelegate packageManagerDelegate =
                installPaymentApps(new String[] {"com.alicepay.app"}, new String[] {"basic-card"});

        verifyNoAppsFound(findApps(new String[] {"interledger"}, mPaymentManifestDownloader,
                mPaymentManifestParser, packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testQueryNoPaymentMethod() {
        PackageManagerDelegate packageManagerDelegate =
                installPaymentApps(new String[] {"com.alicepay.app"}, new String[] {"basic-card"});

        verifyNoAppsFound(findApps(new String[0], mPaymentManifestDownloader,
                mPaymentManifestParser, packageManagerDelegate));

        Mockito.verify(packageManagerDelegate, Mockito.never())
                .getStringArrayResourceForApplication(
                        ArgumentMatchers.any(ApplicationInfo.class), ArgumentMatchers.anyInt());
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testQueryBasicCardsWithTwoApps() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = "com.alicepay.app";
        alicePay.activityInfo.name = "com.alicepay.app.WebPaymentActivity";
        alicePay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle alicePayMetaData = new Bundle();
        alicePayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        alicePayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 1);
        alicePay.activityInfo.metaData = alicePayMetaData;
        activities.add(alicePay);

        ResolveInfo bobPay = new ResolveInfo();
        bobPay.activityInfo = new ActivityInfo();
        bobPay.activityInfo.packageName = "com.bobpay.app";
        bobPay.activityInfo.name = "com.bobpay.app.WebPaymentActivity";
        bobPay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle bobPayMetaData = new Bundle();
        bobPayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "basic-card");
        bobPayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 2);
        bobPay.activityInfo.metaData = bobPayMetaData;
        activities.add(bobPay);

        Mockito.when(mPackageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(mPackageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);
        Mockito.when(mPackageManagerDelegate.getServicesThatCanRespondToIntent(
                             ArgumentMatchers.argThat(new IntentArgumentMatcher(
                                     new Intent(AndroidPaymentAppFinder.ACTION_IS_READY_TO_PAY)))))
                .thenReturn(new ArrayList<ResolveInfo>());

        Mockito.when(mPackageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(alicePay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(1)))
                .thenReturn(new String[] {"https://alicepay.com"});
        Mockito.when(mPackageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(bobPay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(2)))
                .thenReturn(new String[] {"https://bobpay.com"});

        PaymentAppFactoryDelegate delegate = findApps(new String[] {"basic-card"},
                mPaymentManifestDownloader, mPaymentManifestParser, mPackageManagerDelegate);

        Mockito.verify(delegate).onCanMakePaymentCalculated(true);
        Mockito.verify(delegate).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.alicepay.app")));
        Mockito.verify(delegate).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.bobpay.app")));
        Mockito.verify(delegate).onDoneCreatingPaymentApps(/*factory=*/null);
    }

    @SmallTest
    @Test
    @UiThreadTest
    public void testQueryBobPayWithOneAppThatHasIsReadyToPayService() {
        List<ResolveInfo> activities = new ArrayList<>();
        ResolveInfo bobPay = new ResolveInfo();
        bobPay.activityInfo = new ActivityInfo();
        bobPay.activityInfo.packageName = "com.bobpay.app";
        bobPay.activityInfo.name = "com.bobpay.app.WebPaymentActivity";
        bobPay.activityInfo.applicationInfo = new ApplicationInfo();
        Bundle bobPayMetaData = new Bundle();
        bobPayMetaData.putString(
                AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                "https://bobpay.com");
        bobPayMetaData.putInt(AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES, 1);
        bobPay.activityInfo.metaData = bobPayMetaData;
        activities.add(bobPay);

        Mockito.when(mPackageManagerDelegate.getAppLabel(Mockito.any(ResolveInfo.class)))
                .thenReturn("A non-empty label");
        Mockito.when(mPackageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                             ArgumentMatchers.argThat(sPayIntentArgumentMatcher)))
                .thenReturn(activities);

        Mockito.when(mPackageManagerDelegate.getStringArrayResourceForApplication(
                             ArgumentMatchers.eq(bobPay.activityInfo.applicationInfo),
                             ArgumentMatchers.eq(1)))
                .thenReturn(new String[] {"https://bobpay.com", "basic-card"});

        List<ResolveInfo> services = new ArrayList<>();
        ResolveInfo isBobPayReadyToPay = new ResolveInfo();
        isBobPayReadyToPay.serviceInfo = new ServiceInfo();
        isBobPayReadyToPay.serviceInfo.packageName = "com.bobpay.app";
        isBobPayReadyToPay.serviceInfo.name = "com.bobpay.app.IsReadyToWebPay";
        services.add(isBobPayReadyToPay);
        Intent isReadyToPayIntent = new Intent(AndroidPaymentAppFinder.ACTION_IS_READY_TO_PAY);
        Mockito
                .when(mPackageManagerDelegate.getServicesThatCanRespondToIntent(
                        ArgumentMatchers.argThat(new IntentArgumentMatcher(isReadyToPayIntent))))
                .thenReturn(services);

        PackageInfo bobPayPackageInfo = new PackageInfo();
        bobPayPackageInfo.versionCode = 10;
        bobPayPackageInfo.signatures = new Signature[1];
        bobPayPackageInfo.signatures[0] = PaymentManifestVerifierTest.BOB_PAY_SIGNATURE;
        Mockito.when(mPackageManagerDelegate.getPackageInfoWithSignatures("com.bobpay.app"))
                .thenReturn(bobPayPackageInfo);

        PaymentManifestDownloader downloader = new PaymentManifestDownloader() {
            @Override
            public void initialize(WebContents webContents) {}

            @Override
            public void downloadPaymentMethodManifest(
                    Origin merchantOrigin, GURL url, ManifestDownloadCallback callback) {
                callback.onPaymentMethodManifestDownloadSuccess(url,
                        PaymentManifestDownloader.createOpaqueOriginForTest(), "some content here");
            }

            @Override
            public void downloadWebAppManifest(Origin paynentMethodManifestOrigin, GURL url,
                    ManifestDownloadCallback callback) {
                callback.onWebAppManifestDownloadSuccess("some content here");
            }

            @Override
            public void destroy() {}
        };

        PaymentManifestParser parser = new PaymentManifestParser() {
            @Override
            public void parsePaymentMethodManifest(
                    GURL paymentMethodManifestUrl, String content, ManifestParseCallback callback) {
                callback.onPaymentMethodManifestParseSuccess(
                        new GURL[] {new GURL("https://bobpay.com/app.json")}, new GURL[0]);
            }

            @Override
            public void parseWebAppManifest(String content, ManifestParseCallback callback) {
                WebAppManifestSection[] manifest = new WebAppManifestSection[1];
                int minVersion = 10;
                manifest[0] = new WebAppManifestSection("com.bobpay.app", minVersion,
                        PaymentManifestVerifierTest.BOB_PAY_SIGNATURE_FINGERPRINTS);
                callback.onWebAppManifestParseSuccess(manifest);
            }

            @Override
            public void createNative(WebContents webContents) {}

            @Override
            public void destroyNative() {}
        };

        PaymentAppFactoryDelegate delegate = findApps(
                new String[] {"https://bobpay.com"}, downloader, parser, mPackageManagerDelegate);

        Mockito.verify(delegate).onPaymentAppCreated(
                ArgumentMatchers.argThat(Matches.paymentAppIdentifier("com.bobpay.app")));
        Mockito.verify(delegate).onDoneCreatingPaymentApps(/*factory=*/null);
    }

    private static final class Matches implements ArgumentMatcher<PaymentApp> {
        private final String mExpectedAppIdentifier;

        private Matches(String expectedAppIdentifier) {
            mExpectedAppIdentifier = expectedAppIdentifier;
        }

        /**
         * Builds a matcher based on payment app identifier.
         *
         * @param expectedAppIdentifier The expected app identifier to match.
         * @return A matcher to use in a mock expectation.
         */
        public static ArgumentMatcher<PaymentApp> paymentAppIdentifier(
                String expectedAppIdentifier) {
            return new Matches(expectedAppIdentifier);
        }

        @Override
        public boolean matches(PaymentApp app) {
            return app.getIdentifier().equals(mExpectedAppIdentifier);
        }
    }
}
