// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.payments.PaymentAppFactory.PaymentAppCreatedCallback;
import org.chromium.chrome.browser.payments.PaymentManifestVerifier.ManifestVerifyCallback;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.content_public.browser.WebContents;

import java.net.URI;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Finds installed native Android payment apps and verifies their signatures according to the
 * payment method manifests. The manifests are located based on the payment method name, which is a
 * URI that starts with "https://" (localhosts can be "http://", however). The W3C-published non-URI
 * payment method names are exceptions: these are common payment method names that do not have a
 * manifest and can be used by any payment app.
 */
public class AndroidPaymentAppFinder implements ManifestVerifyCallback {
    private static final String TAG = "PaymentAppFinder";

    /** The maximum number of payment method manifests to download. */
    private static final int MAX_NUMBER_OF_MANIFESTS = 10;

    /** The name of the intent for the service to check whether an app is ready to pay. */
    /* package */ static final String ACTION_IS_READY_TO_PAY =
            "org.chromium.intent.action.IS_READY_TO_PAY";

    /**
     * Meta data name of an app's supported payment method names.
     */
    /* package */ static final String META_DATA_NAME_OF_PAYMENT_METHOD_NAMES =
            "org.chromium.payment_method_names";

    /**
     * Meta data name of an app's supported default payment method name.
     */
    /* package */ static final String META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME =
            "org.chromium.default_payment_method_name";

    private final WebContents mWebContents;
    private final Set<String> mNonUriPaymentMethods;
    private final Set<URI> mUriPaymentMethods;
    private final PaymentManifestDownloader mDownloader;
    private final PaymentManifestWebDataService mWebDataService;
    private final PaymentManifestParser mParser;
    private final PackageManagerDelegate mPackageManagerDelegate;
    private final PaymentAppCreatedCallback mCallback;
    private final boolean mIsIncognito;

    /**
     * A mapping from an Android package name to the payment app with that package name. The apps
     * will be sent to the <code>PaymentAppCreatedCallback</code> once all of their payment methods
     * have been validated. The package names are used for identification because they are unique on
     * Android. Example contents:
     *
     * {"com.bobpay.app.v1": androidPaymentApp1, "com.alicepay.app.v1": androidPaymentApp2}
     */
    private final Map<String, AndroidPaymentApp> mValidApps = new HashMap<>();

    /**
     * A mapping from origins of payment apps to the URI payment methods of these apps. Used to look
     * up payment apps in <code>mVerifiedPaymentMethods</code> based on the supported origins that
     * have been verified in <code>PaymentManifestVerifier</code>. Example contents:
     *
     * {"https://bobpay.com": ("https://bobpay.com/personal", "https://bobpay.com/business")}
     */
    private final Map<URI, Set<URI>> mOriginToUriDefaultMethodsMapping = new HashMap<>();

    /**
     * A mapping from URI payment methods to the applications that support this payment method,
     * but not as their default payment method. Used to find all apps that claim support for a given
     * URI payment method when the payment manifest of this method contains
     * "supported_origins": "*". Example contents:
     *
     * {"https://bobpay.com/public-standard": (resolveInfo1, resolveInfo2, resolveInfo3)}
     */
    private final Map<URI, Set<ResolveInfo>> mMethodToSupportedAppsMapping = new HashMap<>();

    /** Contains information about a URI payment method. */
    private static final class PaymentMethod {
        /** The default applications for this payment method. */
        public final Set<ResolveInfo> defaultApplications = new HashSet<>();

        /** The supported origins of this payment method. */
        public final Set<URI> supportedOrigins = new HashSet<>();

        /** Whether all origins are supported. */
        public boolean supportsAllOrigins;
    }

    /**
     * A mapping from URI payment methods to the verified information about these methods. Used to
     * accumulate the incremental information that arrives from
     * <code>PaymentManifestVerifier</code>s for each of the payment method manifests that need to
     * be downloaded. Example contents:
     *
     * { "https://bobpay.com/business": method1, "https://bobpay.com/personal": method2}
     */
    private final Map<URI, PaymentMethod> mVerifiedPaymentMethods = new HashMap<>();

    private int mPendingVerifiersCount;
    private int mPendingResourceUsersCount;

    /**
     * Finds native Android payment apps.
     *
     * @param webContents            The web contents that invoked the web payments API.
     * @param methods                The list of payment methods requested by the merchant. For
     *                               example, "https://bobpay.com", "https://android.com/pay",
     *                               "basic-card".
     * @param webDataService         The web data service to cache manifest.
     * @param downloader             The manifest downloader.
     * @param parser                 The manifest parser.
     * @param packageManagerDelegate The package information retriever.
     * @param callback               The asynchronous callback to be invoked (on the UI thread) when
     *                               all Android payment apps have been found.
     */
    public static void find(WebContents webContents, Set<String> methods,
            PaymentManifestWebDataService webDataService, PaymentManifestDownloader downloader,
            PaymentManifestParser parser, PackageManagerDelegate packageManagerDelegate,
            PaymentAppCreatedCallback callback) {
        new AndroidPaymentAppFinder(webContents, methods, webDataService, downloader, parser,
                packageManagerDelegate, callback)
                .findAndroidPaymentApps();
    }

    private AndroidPaymentAppFinder(WebContents webContents, Set<String> methods,
            PaymentManifestWebDataService webDataService, PaymentManifestDownloader downloader,
            PaymentManifestParser parser, PackageManagerDelegate packageManagerDelegate,
            PaymentAppCreatedCallback callback) {
        mWebContents = webContents;

        // For non-URI payment method names, only names published by W3C should be supported. Keep
        // this in sync with manifest_verifier.cc.
        Set<String> supportedNonUriPaymentMethods = new HashSet<>();
        supportedNonUriPaymentMethods.add(MethodStrings.BASIC_CARD);
        supportedNonUriPaymentMethods.add(MethodStrings.INTERLEDGER);
        supportedNonUriPaymentMethods.add(MethodStrings.PAYEE_CREDIT_TRANSFER);
        supportedNonUriPaymentMethods.add(MethodStrings.PAYER_CREDIT_TRANSFER);
        supportedNonUriPaymentMethods.add(MethodStrings.TOKENIZED_CARD);

        mNonUriPaymentMethods = new HashSet<>();
        mUriPaymentMethods = new HashSet<>();
        for (String method : methods) {
            assert !TextUtils.isEmpty(method);
            if (supportedNonUriPaymentMethods.contains(method)) {
                mNonUriPaymentMethods.add(method);
            } else if (UriUtils.looksLikeUriMethod(method)) {
                URI uri = UriUtils.parseUriFromString(method);
                if (uri != null) mUriPaymentMethods.add(uri);
            }
        }

        mDownloader = downloader;
        mWebDataService = webDataService;
        mParser = parser;
        mPackageManagerDelegate = packageManagerDelegate;
        mCallback = callback;
        ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
        mIsIncognito = activity != null && activity.getCurrentTabModel().isIncognito();
    }

    /**
     * Finds and validates the installed android payment apps that support the payment method names
     * that the merchant is using.
     */
    private void findAndroidPaymentApps() {
        List<ResolveInfo> allInstalledPaymentApps =
                mPackageManagerDelegate.getActivitiesThatCanRespondToIntentWithMetaData(
                        new Intent(AndroidPaymentApp.ACTION_PAY));
        if (allInstalledPaymentApps.isEmpty()) {
            onAllAppsFound();
            return;
        }

        // All URI methods for which manifests should be downloaded. For example, if merchant
        // supports "https://bobpay.com/personal" payment method, but user also has Alice Pay app
        // that has the default payment method name of "https://alicepay.com/webpay" that claims to
        // support "https://bobpay.com/personal" method as well, then both of these methods will be
        // in this set:
        //
        // ("https://bobpay.com/personal", "https://alicepay.com/webpay")
        Set<URI> uriMethods = new HashSet<>(mUriPaymentMethods);

        // A mapping from all known payment method names to the corresponding payment apps that
        // claim to support these payment methods. Example contents:
        //
        // {"basic-card": (bobPay, alicePay), "https://alicepay.com/webpay": (alicePay)}
        //
        // In case of non-URI payment methods, such as "basic-card", all apps that claim to support
        // it are considered valid. In case of URI payment methods, if no apps claim to support a
        // URI method, then no information will be downloaded for this method.
        Map<String, Set<ResolveInfo>> methodToAppsMapping = new HashMap<>();

        // A mapping from URI payment method names to the corresponding default payment apps. The
        // payment manifest verifiers compare these apps against the information in
        // "default_applications" of the payment method manifests to determine the validity of these
        // apps. Example contents:
        //
        // {"https://bobpay.com/personal": (bobPay), "https://alicepay.com/webpay": (alicePay)}
        Map<URI, Set<ResolveInfo>> uriMethodToDefaultAppsMapping = new HashMap<>();

        // A mapping from URI payment method names to the origins of the payment apps that support
        // that method name. The payment manifest verifiers compare these origins against the
        // information in "supported_origins" of the payment method manifests to determine validity
        // of these origins. Example contents:
        //
        // {"https://bobpay.com/personal": ("https://alicepay.com")}
        Map<URI, Set<URI>> uriMethodToSupportedOriginsMapping = new HashMap<>();

        for (int i = 0; i < allInstalledPaymentApps.size(); i++) {
            ResolveInfo app = allInstalledPaymentApps.get(i);

            String defaultMethod = app.activityInfo.metaData == null
                    ? null
                    : app.activityInfo.metaData.getString(
                              META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME);

            URI appOrigin = null;
            URI defaultUriMethod = null;
            if (!TextUtils.isEmpty(defaultMethod)) {
                if (!methodToAppsMapping.containsKey(defaultMethod)) {
                    methodToAppsMapping.put(defaultMethod, new HashSet<ResolveInfo>());
                }
                methodToAppsMapping.get(defaultMethod).add(app);

                if (UriUtils.looksLikeUriMethod(defaultMethod)) {
                    defaultUriMethod = UriUtils.parseUriFromString(defaultMethod);
                    if (defaultUriMethod != null) {
                        uriMethods.add(defaultUriMethod);

                        if (!uriMethodToDefaultAppsMapping.containsKey(defaultUriMethod)) {
                            uriMethodToDefaultAppsMapping.put(
                                    defaultUriMethod, new HashSet<ResolveInfo>());
                        }
                        uriMethodToDefaultAppsMapping.get(defaultUriMethod).add(app);

                        appOrigin = UriUtils.getOrigin(defaultUriMethod);
                        if (!mOriginToUriDefaultMethodsMapping.containsKey(appOrigin)) {
                            mOriginToUriDefaultMethodsMapping.put(appOrigin, new HashSet<URI>());
                        }
                        mOriginToUriDefaultMethodsMapping.get(appOrigin).add(defaultUriMethod);
                    }
                }
            }

            // Note that a payment app with non-URI default payment method (e.g., "basic-card")
            // can support URI payment methods (e.g., "https://bobpay.com/public-standard").
            Set<String> supportedMethods = getSupportedPaymentMethods(app.activityInfo);
            for (String supportedMethod : supportedMethods) {
                URI supportedUriMethod = UriUtils.looksLikeUriMethod(supportedMethod)
                        ? UriUtils.parseUriFromString(supportedMethod)
                        : null;
                if (supportedUriMethod != null && supportedUriMethod.equals(defaultUriMethod)) {
                    continue;
                }

                if (!methodToAppsMapping.containsKey(supportedMethod)) {
                    methodToAppsMapping.put(supportedMethod, new HashSet<ResolveInfo>());
                }
                methodToAppsMapping.get(supportedMethod).add(app);

                if (supportedUriMethod == null) continue;

                if (!mMethodToSupportedAppsMapping.containsKey(supportedUriMethod)) {
                    mMethodToSupportedAppsMapping.put(
                            supportedUriMethod, new HashSet<ResolveInfo>());
                }
                mMethodToSupportedAppsMapping.get(supportedUriMethod).add(app);

                if (appOrigin == null) continue;

                if (!uriMethodToSupportedOriginsMapping.containsKey(supportedUriMethod)) {
                    uriMethodToSupportedOriginsMapping.put(supportedUriMethod, new HashSet<URI>());
                }
                uriMethodToSupportedOriginsMapping.get(supportedUriMethod).add(appOrigin);
            }
        }

        List<PaymentManifestVerifier> manifestVerifiers = new ArrayList<>();
        for (URI uriMethodName : uriMethods) {
            if (!methodToAppsMapping.containsKey(uriMethodName.toString())) continue;

            if (!mParser.isNativeInitialized()) mParser.createNative(mWebContents);

            // Initialize the native side of the downloader, once we know that a manifest file needs
            // to be downloaded.
            if (!mDownloader.isInitialized()) mDownloader.initialize(mWebContents);

            manifestVerifiers.add(new PaymentManifestVerifier(uriMethodName,
                    uriMethodToDefaultAppsMapping.get(uriMethodName),
                    uriMethodToSupportedOriginsMapping.get(uriMethodName), mWebDataService,
                    mDownloader, mParser, mPackageManagerDelegate, this /* callback */));

            if (manifestVerifiers.size() == MAX_NUMBER_OF_MANIFESTS) {
                Log.e(TAG, "Reached maximum number of allowed payment app manifests.");
                break;
            }
        }

        for (String nonUriMethodName : mNonUriPaymentMethods) {
            if (methodToAppsMapping.containsKey(nonUriMethodName)) {
                Set<ResolveInfo> supportedApps = methodToAppsMapping.get(nonUriMethodName);
                for (ResolveInfo supportedApp : supportedApps) {
                    // Chrome does not verify app manifests for non-URI payment method support.
                    onValidPaymentAppForPaymentMethodName(supportedApp, nonUriMethodName);
                }
            }
        }

        if (manifestVerifiers.isEmpty()) {
            onAllAppsFound();
            return;
        }

        mPendingVerifiersCount = mPendingResourceUsersCount = manifestVerifiers.size();
        for (PaymentManifestVerifier manifestVerifier : manifestVerifiers) {
            manifestVerifier.verify();
        }
    }

    /**
     * Queries the Android app metadata for the names of the non-default payment methods that the
     * given app supports.
     *
     * @param activityInfo The application information to query.
     * @return The set of non-default payment method names that this application supports. Never
     *         null.
     */
    private Set<String> getSupportedPaymentMethods(ActivityInfo activityInfo) {
        Set<String> result = new HashSet<>();
        if (activityInfo.metaData == null) return result;

        int resId = activityInfo.metaData.getInt(META_DATA_NAME_OF_PAYMENT_METHOD_NAMES);
        if (resId == 0) return result;

        String[] nonDefaultPaymentMethodNames =
                mPackageManagerDelegate.getStringArrayResourceForApplication(
                        activityInfo.applicationInfo, resId);
        if (nonDefaultPaymentMethodNames == null) return result;

        Collections.addAll(result, nonDefaultPaymentMethodNames);

        return result;
    }

    @Override
    public void onValidDefaultPaymentApp(URI methodName, ResolveInfo resolveInfo) {
        getOrCreateVerifiedPaymentMethod(methodName).defaultApplications.add(resolveInfo);
    }

    @Override
    public void onValidSupportedOrigin(URI methodName, URI supportedOrigin) {
        getOrCreateVerifiedPaymentMethod(methodName).supportedOrigins.add(supportedOrigin);
    }

    @Override
    public void onAllOriginsSupported(URI methodName) {
        getOrCreateVerifiedPaymentMethod(methodName).supportsAllOrigins = true;
    }

    private PaymentMethod getOrCreateVerifiedPaymentMethod(URI methodName) {
        PaymentMethod verifiedPaymentManifest = mVerifiedPaymentMethods.get(methodName);
        if (verifiedPaymentManifest == null) {
            verifiedPaymentManifest = new PaymentMethod();
            mVerifiedPaymentMethods.put(methodName, verifiedPaymentManifest);
        }
        return verifiedPaymentManifest;
    }

    @Override
    public void onVerificationError(String errorMessage) {
        mCallback.onGetPaymentAppsError(errorMessage);
    }

    @Override
    public void onFinishedVerification() {
        mPendingVerifiersCount--;
        if (mPendingVerifiersCount != 0) return;

        for (Map.Entry<URI, PaymentMethod> nameAndMethod : mVerifiedPaymentMethods.entrySet()) {
            URI methodName = nameAndMethod.getKey();
            if (!mUriPaymentMethods.contains(methodName)) continue;

            PaymentMethod method = nameAndMethod.getValue();
            for (ResolveInfo app : method.defaultApplications) {
                onValidPaymentAppForPaymentMethodName(app, methodName.toString());
            }

            // Chrome does not verify payment apps if they claim to support URI payment methods
            // that support all origins.
            if (method.supportsAllOrigins) {
                Set<ResolveInfo> supportedApps = mMethodToSupportedAppsMapping.get(methodName);
                if (supportedApps == null) continue;
                for (ResolveInfo supportedApp : supportedApps) {
                    onValidPaymentAppForPaymentMethodName(supportedApp, methodName.toString());
                }
                continue;
            }

            for (URI supportedOrigin : method.supportedOrigins) {
                Set<URI> supportedAppMethodNames =
                        mOriginToUriDefaultMethodsMapping.get(supportedOrigin);
                if (supportedAppMethodNames == null) continue;

                for (URI supportedAppMethodName : supportedAppMethodNames) {
                    PaymentMethod supportedAppMethod =
                            mVerifiedPaymentMethods.get(supportedAppMethodName);
                    if (supportedAppMethod == null) continue;

                    for (ResolveInfo supportedApp : supportedAppMethod.defaultApplications) {
                        onValidPaymentAppForPaymentMethodName(supportedApp, methodName.toString());
                    }
                }
            }
        }

        onAllAppsFound();
    }

    /** Notifies callback that all payment apps have been found. */
    private void onAllAppsFound() {
        assert mPendingVerifiersCount == 0;

        if (!mIsIncognito) {
            List<ResolveInfo> resolveInfos =
                    mPackageManagerDelegate.getServicesThatCanRespondToIntent(
                            new Intent(ACTION_IS_READY_TO_PAY));
            for (int i = 0; i < resolveInfos.size(); i++) {
                ResolveInfo resolveInfo = resolveInfos.get(i);
                AndroidPaymentApp app = mValidApps.get(resolveInfo.serviceInfo.packageName);
                if (app != null) app.setIsReadyToPayAction(resolveInfo.serviceInfo.name);
            }
        }

        for (Map.Entry<String, AndroidPaymentApp> entry : mValidApps.entrySet()) {
            mCallback.onPaymentAppCreated(entry.getValue());
        }

        mCallback.onAllPaymentAppsCreated();
    }

    /**
     * Enables the given payment app to use this method name.
     *
     * @param resolveInfo The payment app that's allowed to use the method name.
     * @param methodName  The method name that can be used by the app.
     */
    private void onValidPaymentAppForPaymentMethodName(ResolveInfo resolveInfo, String methodName) {
        String packageName = resolveInfo.activityInfo.packageName;
        AndroidPaymentApp app = mValidApps.get(packageName);
        if (app == null) {
            CharSequence label = mPackageManagerDelegate.getAppLabel(resolveInfo);
            if (TextUtils.isEmpty(label)) {
                Log.e(TAG, "Skipping \"%s\" because of empty label.", packageName);
                return;
            }

            // Dedupe corresponding ServiceWorkerPaymentApp which is registered with the default
            // payment method name as the scope and the scope is used as the app Id.
            String webAppIdCanDeduped = resolveInfo.activityInfo.metaData == null
                    ? null
                    : resolveInfo.activityInfo.metaData.getString(
                              META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME);
            app = new AndroidPaymentApp(mWebContents, packageName, resolveInfo.activityInfo.name,
                    label.toString(), mPackageManagerDelegate.getAppIcon(resolveInfo), mIsIncognito,
                    webAppIdCanDeduped == null ? null
                                               : UriUtils.parseUriFromString(webAppIdCanDeduped));
            mValidApps.put(packageName, app);
        }

        // The same method may be added multiple times.
        app.addMethodName(methodName);
    }

    @Override
    public void onFinishedUsingResources() {
        mPendingResourceUsersCount--;
        if (mPendingResourceUsersCount != 0) return;

        mWebDataService.destroy();
        if (mDownloader.isInitialized()) mDownloader.destroy();
        if (mParser.isNativeInitialized()) mParser.destroyNative();
    }
}
