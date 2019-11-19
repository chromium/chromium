// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.components.payments.PaymentManifestDownloader;
import org.chromium.components.payments.PaymentManifestDownloader.ManifestDownloadCallback;
import org.chromium.components.payments.PaymentManifestParser;
import org.chromium.components.payments.PaymentManifestParser.ManifestParseCallback;
import org.chromium.components.payments.WebAppManifestSection;

import java.net.URI;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Formatter;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Verifies that the discovered native Android payment apps have the sufficient privileges
 * to handle a single payment method. Downloads and parses the manifest to compare package
 * names, versions, and signatures to the apps.
 *
 * Spec:
 * https://docs.google.com/document/d/1izV4uC-tiRJG3JLooqY3YRLU22tYOsLTNq0P_InPJeE/edit#heading=h.cjp3jlnl47h5
 */
public class PaymentManifestVerifier
        implements ManifestDownloadCallback, ManifestParseCallback,
                   PaymentManifestWebDataService.PaymentManifestWebDataServiceCallback {
    /** Interface for the callback to invoke when finished verification. */
    public interface ManifestVerifyCallback {
        /**
         * Enables invoking the given native Android payment app for the given payment method as
         * a default app. Called when the app has been found to have the right privileges to
         * handle this payment method via a web app manifest that's one of the
         * "default_applications".
         *
         * @param methodName  The payment method name that the payment app offers to handle.
         * @param resolveInfo Identifying information for the native Android payment app.
         */
        void onValidDefaultPaymentApp(URI methodName, ResolveInfo resolveInfo);

        /**
         * Enables native Android payment apps from the given origin to use this payment method
         * name.
         *
         * @param methodName      The payment method name that can be used.
         * @param supportedOrigin The origin of the payment apps that can use the method name.
         */
        void onValidSupportedOrigin(URI methodName, URI supportedOrigin);

        /**
         * Enables all native Android payment apps to use  the given <code>methodName</code> as
         * their payment method name.
         *
         * @param methodName The payment method name that can be used by all payment apps.
         */
        void onAllOriginsSupported(URI methodName);

        /**
         * Called when a part of verification has failed.
         *
         * @param errorMessage Developer facing error message.
         */
        void onVerificationError(String errorMessage);

        /**
         * Called when the manifest has been fully verified. No more valid apps or origins will
         * be found after this call.
         */
        void onFinishedVerification();

        /**
         * Called when all the operations are done. After this call, the caller can release
         * resources used by this class: cache, downloader, and parser.
         */
        void onFinishedUsingResources();
    }

    /** Identifying information about an installed native Android payment app. */
    private static class AppInfo {
        /** Identifies a native Android payment app. */
        public ResolveInfo resolveInfo;

        /** The version code for the native Android payment app, e.g., 123. */
        public long version;

        /**
         * The SHA256 certificate fingerprints for the native Android payment app, .e.g,
         * ["308201dd30820146020101300d06092a864886f70d010105050030"].
         */
        public Set<String> sha256CertFingerprints;
    }

    private static final String TAG = "PaymentManifest";
    private static final String ALL_ORIGINS_SUPPORTED_INDICATOR = "*";

    /**
     * The payment method name that's being verified. The corresponding payment method manifest
     * and default web app manifests will be downloaded, parsed, and cached.
     */
    private final URI mMethodName;

    /** A mapping from the package name to the default application that matches the method name. */
    private final Map<String, AppInfo> mDefaultApplications = new HashMap<>();

    /** A set of origins of the non-default payment apps for the method name. */
    private final Set<URI> mSupportedOrigins;

    /**
     * A set of package names and origin names of the apps to cache. May also contain "*" to
     * indicate that apps from all origins are supported.
     */
    private final Set<String> mAppIdentifiersToCache = new HashSet<>();

    /** A list of web app manifests to cache. */
    private final List<WebAppManifestSection[]> mWebAppManifestsToCache = new ArrayList<>();

    private final PaymentManifestWebDataService mCache;
    private final PaymentManifestDownloader mDownloader;
    private final PaymentManifestParser mParser;
    private final PackageManagerDelegate mPackageManagerDelegate;
    private final ManifestVerifyCallback mCallback;
    private final MessageDigest mMessageDigest;

    /**
     * The number of web app manifests that have not yet been retrieved from cache or downloaded
     * from the web.
     */
    private int mPendingWebAppManifestsCount;

    /** Whether the manifest cache is stale (unusable). */
    private boolean mIsManifestCacheStaleOrUnusable;

    /**
     * Whether at least one payment method manifest or web app manifest failed to download or parse.
     */
    private boolean mAtLeastOneManifestFailedToDownloadOrParse;

    /**
     * Builds the manifest verifier.
     *
     * @param methodName             The name of the payment method name that apps offer to handle.
     *                               Must be an absolute URI with HTTPS scheme or HTTP localhost.
     * @param defaultApplications    The identifying information for the native Android payment apps
     *                               that offer to handle this payment method as a default app,
     *                               i.e., as one of the "default_applications". Can be null.
     * @param supportedOrigins       The origins of the apps that claim support of this payment
     *                               method as their non-default, i.e., as one of the
     *                               "supported_origins". Can be null.
     * @param webDataService         The web data service to cache manifest.
     * @param downloader             The manifest downloader.
     * @param parser                 The manifest parser.
     * @param packageManagerDelegate The package information retriever.
     * @param callback               The callback to be notified of verification result.
     */
    public PaymentManifestVerifier(URI methodName, @Nullable Set<ResolveInfo> defaultApplications,
            @Nullable Set<URI> supportedOrigins, PaymentManifestWebDataService webDataService,
            PaymentManifestDownloader downloader, PaymentManifestParser parser,
            PackageManagerDelegate packageManagerDelegate, ManifestVerifyCallback callback) {
        assert methodName.isAbsolute();

        mMethodName = methodName;

        if (defaultApplications != null) {
            for (ResolveInfo defaultApp : defaultApplications) {
                AppInfo appInfo = new AppInfo();
                appInfo.resolveInfo = defaultApp;
                mDefaultApplications.put(appInfo.resolveInfo.activityInfo.packageName, appInfo);
            }
        }

        mSupportedOrigins = Collections.unmodifiableSet(
                supportedOrigins == null ? new HashSet<URI>() : new HashSet<>(supportedOrigins));
        mDownloader = downloader;
        mCache = webDataService;
        mParser = parser;
        mPackageManagerDelegate = packageManagerDelegate;
        mCallback = callback;

        MessageDigest md = null;
        if (!mDefaultApplications.isEmpty()) {
            try {
                md = MessageDigest.getInstance("SHA-256");
            } catch (NoSuchAlgorithmException e) {
                // Intentionally ignore until verify() is called.
                Log.e(TAG, "Unable to generate SHA-256 hashes.");
            }
        }
        mMessageDigest = md;
    }

    /**
     * Verifies that the discovered native Android payment apps have the sufficient privileges to
     * handle this payment method.
     */
    public void verify() {
        if (!mDefaultApplications.isEmpty() && mMessageDigest == null) {
            mCallback.onFinishedVerification();
            mCallback.onFinishedUsingResources();
            return;
        }

        List<String> invalidAppsToRemove = new ArrayList<>();
        for (Map.Entry<String, AppInfo> entry : mDefaultApplications.entrySet()) {
            String packageName = entry.getKey();
            AppInfo appInfo = entry.getValue();

            PackageInfo packageInfo =
                    mPackageManagerDelegate.getPackageInfoWithSignatures(packageName);
            if (packageInfo == null) {
                invalidAppsToRemove.add(packageName);
                continue;
            }

            appInfo.version = packageInfo.versionCode;
            appInfo.sha256CertFingerprints = new HashSet<>();
            Signature[] signatures = packageInfo.signatures;
            for (int i = 0; i < signatures.length; i++) {
                mMessageDigest.update(signatures[i].toByteArray());

                // The digest is reset after completing the hash computation.
                appInfo.sha256CertFingerprints.add(byteArrayToString(mMessageDigest.digest()));
            }
        }

        for (int i = 0; i < invalidAppsToRemove.size(); i++) {
            mDefaultApplications.remove(invalidAppsToRemove.get(i));
        }

        // Try to fetch manifest from the cache first.
        if (!mCache.getPaymentMethodManifest(mMethodName.toString(), this)) {
            mIsManifestCacheStaleOrUnusable = true;
            mDownloader.downloadPaymentMethodManifest(mMethodName, this);
        }
    }

    /**
     * Formats bytes into a string for easier comparison as a member of a set.
     *
     * @param input Input bytes.
     * @return A string representation of the input bytes, e.g., "0123456789abcdef".
     */
    private static String byteArrayToString(byte[] input) {
        if (input == null) return null;

        StringBuilder builder = new StringBuilder(input.length * 2);
        Formatter formatter = new Formatter(builder);
        for (byte b : input) {
            formatter.format("%02x", b);
        }

        String result = builder.toString();
        formatter.close();
        return result;
    }

    @Override
    public void onPaymentMethodManifestFetched(String[] appIdentifiers) {
        Set<String> cachedDefaultAppPackageNames = new HashSet<>();
        Set<URI> cachedSupportedOrigins = new HashSet<>();
        boolean cachedAllOriginsSupported = false;
        for (int i = 0; i < appIdentifiers.length; i++) {
            if (appIdentifiers[i] == null) {
                // The cache is stale. Download the manifest from the web instead.
                mIsManifestCacheStaleOrUnusable = true;
                mDownloader.downloadPaymentMethodManifest(mMethodName, this);
                return;
            }

            if (appIdentifiers[i].equals(ALL_ORIGINS_SUPPORTED_INDICATOR)) {
                cachedAllOriginsSupported = true;
                continue;
            }

            if (UriUtils.looksLikeUriMethod(appIdentifiers[i])) {
                URI uriOrigin = UriUtils.parseUriFromString(appIdentifiers[i]);
                if (uriOrigin != null) cachedSupportedOrigins.add(uriOrigin);
                continue;
            }

            cachedDefaultAppPackageNames.add(appIdentifiers[i]);
        }

        // The cache may be stale if it doesn't contain all matching apps, so download the
        // manifest from the web instead.
        if (appIdentifiers.length == 0
                || !cachedDefaultAppPackageNames.containsAll(mDefaultApplications.keySet())
                || (!cachedSupportedOrigins.containsAll(mSupportedOrigins)
                           && !cachedAllOriginsSupported)) {
            mIsManifestCacheStaleOrUnusable = true;
            mDownloader.downloadPaymentMethodManifest(mMethodName, this);
            return;
        }

        if (cachedAllOriginsSupported) {
            mCallback.onAllOriginsSupported(mMethodName);
        } else {
            cachedSupportedOrigins.retainAll(mSupportedOrigins);
            for (URI validSupportedOrigin : cachedSupportedOrigins) {
                mCallback.onValidSupportedOrigin(mMethodName, validSupportedOrigin);
            }
        }

        if (mDefaultApplications.isEmpty()) {
            mCallback.onFinishedVerification();
            // Download and parse manifest to refresh cache.
            mDownloader.downloadPaymentMethodManifest(mMethodName, this);
            return;
        }

        mPendingWebAppManifestsCount = mDefaultApplications.size();
        for (String matchingAppPackageName : mDefaultApplications.keySet()) {
            if (!mCache.getPaymentWebAppManifest(matchingAppPackageName, this)) {
                mIsManifestCacheStaleOrUnusable = true;
                mPendingWebAppManifestsCount = 0;
                mDownloader.downloadPaymentMethodManifest(mMethodName, this);
                return;
            }
        }
    }

    @Override
    public void onPaymentWebAppManifestFetched(WebAppManifestSection[] manifest) {
        if (mIsManifestCacheStaleOrUnusable) return;

        if (manifest == null || manifest.length == 0) {
            mIsManifestCacheStaleOrUnusable = true;
            mPendingWebAppManifestsCount = 0;
            mDownloader.downloadPaymentMethodManifest(mMethodName, this);
            return;
        }

        Set<String> validAppPackageNames = verifyAppWithWebAppManifest(manifest);
        for (String validAppPackageName : validAppPackageNames) {
            mCallback.onValidDefaultPaymentApp(
                    mMethodName, mDefaultApplications.get(validAppPackageName).resolveInfo);
        }

        mPendingWebAppManifestsCount--;
        if (mPendingWebAppManifestsCount != 0) return;

        mCallback.onFinishedVerification();

        // Download and parse manifest to refresh cache.
        mDownloader.downloadPaymentMethodManifest(mMethodName, this);
    }

    @Override
    public void onPaymentMethodManifestDownloadSuccess(String content) {
        mParser.parsePaymentMethodManifest(content, this);
    }

    @Override
    public void onPaymentMethodManifestParseSuccess(
            URI[] webAppManifestUris, URI[] supportedOrigins, boolean allOriginsSupported) {
        assert webAppManifestUris != null;
        assert supportedOrigins != null;
        assert webAppManifestUris.length > 0 || supportedOrigins.length > 0 || allOriginsSupported;
        assert !mAtLeastOneManifestFailedToDownloadOrParse;
        assert mPendingWebAppManifestsCount == 0;

        if (allOriginsSupported) {
            if (mIsManifestCacheStaleOrUnusable) mCallback.onAllOriginsSupported(mMethodName);
            mAppIdentifiersToCache.add(ALL_ORIGINS_SUPPORTED_INDICATOR);
        } else {
            Set<URI> downloadedSupportedOrigins = new HashSet<>();
            for (int i = 0; i < supportedOrigins.length; i++) {
                downloadedSupportedOrigins.add(supportedOrigins[i]);
                mAppIdentifiersToCache.add(supportedOrigins[i].toString());
            }
            if (mIsManifestCacheStaleOrUnusable) {
                downloadedSupportedOrigins.retainAll(mSupportedOrigins);
                for (URI validSupportedOrigin : downloadedSupportedOrigins) {
                    mCallback.onValidSupportedOrigin(mMethodName, validSupportedOrigin);
                }
            }
        }

        if (webAppManifestUris.length == 0) {
            if (mIsManifestCacheStaleOrUnusable) mCallback.onFinishedVerification();
            // Cache supported package names and origins as well as possibly "*".
            mCache.addPaymentMethodManifest(mMethodName.toString(),
                    mAppIdentifiersToCache.toArray(new String[mAppIdentifiersToCache.size()]));
            mCallback.onFinishedUsingResources();
            return;
        }

        mPendingWebAppManifestsCount = webAppManifestUris.length;
        for (int i = 0; i < webAppManifestUris.length; i++) {
            if (mAtLeastOneManifestFailedToDownloadOrParse) return;
            assert webAppManifestUris[i] != null;
            mDownloader.downloadWebAppManifest(webAppManifestUris[i], this);
        }
    }

    @Override
    public void onWebAppManifestDownloadSuccess(String content) {
        if (mAtLeastOneManifestFailedToDownloadOrParse) return;
        mParser.parseWebAppManifest(content, this);
    }

    @Override
    public void onWebAppManifestParseSuccess(WebAppManifestSection[] manifest) {
        assert manifest != null;
        assert manifest.length > 0;

        if (mAtLeastOneManifestFailedToDownloadOrParse) return;

        for (int i = 0; i < manifest.length; i++) {
            mAppIdentifiersToCache.add(manifest[i].id);
        }
        mWebAppManifestsToCache.add(manifest);

        // Verify payment apps only if they have not already been verified by the cached manifest.
        if (mIsManifestCacheStaleOrUnusable) {
            Set<String> validAppPackageNames = verifyAppWithWebAppManifest(manifest);
            for (String validAppPackageName : validAppPackageNames) {
                mCallback.onValidDefaultPaymentApp(
                        mMethodName, mDefaultApplications.get(validAppPackageName).resolveInfo);
            }
        }

        mPendingWebAppManifestsCount--;
        if (mPendingWebAppManifestsCount != 0) return;

        if (mIsManifestCacheStaleOrUnusable) mCallback.onFinishedVerification();

        // Cache supported apps' package names and origins. (Also cache "*" if applicable.)
        mCache.addPaymentMethodManifest(mMethodName.toString(),
                mAppIdentifiersToCache.toArray(new String[mAppIdentifiersToCache.size()]));

        // Cache supported apps' parsed manifests.
        mCache.addPaymentWebAppManifest(flattenListOfArrays(mWebAppManifestsToCache));

        mCallback.onFinishedUsingResources();
    }

    /**
     * Flattens a list of arrays into a single array.
     *
     * @param listOfLists A lists of arrays to flatten.
     * @return The single array result.
     */
    private static WebAppManifestSection[] flattenListOfArrays(
            List<WebAppManifestSection[]> listOfLists) {
        int totalNumberOfItems = 0;
        for (int i = 0; i < listOfLists.size(); i++) {
            totalNumberOfItems += listOfLists.get(i).length;
        }

        WebAppManifestSection[] flattenedList = new WebAppManifestSection[totalNumberOfItems];
        for (int i = 0, k = 0; i < listOfLists.size(); i++) {
            for (int j = 0; j < listOfLists.get(i).length; j++, k++) {
                assert k < flattenedList.length;
                flattenedList[k] = listOfLists.get(i)[j];
            }
        }

        return flattenedList;
    }

    /**
     * @return The set of package names of payment apps that match the manifest. Could be empty,
     * but never null.
     */
    private Set<String> verifyAppWithWebAppManifest(WebAppManifestSection[] manifest) {
        assert manifest.length > 0;

        List<Set<String>> sectionsFingerprints = new ArrayList<>();
        for (int i = 0; i < manifest.length; i++) {
            WebAppManifestSection section = manifest[i];
            Set<String> fingerprints = new HashSet<>();
            for (int j = 0; j < section.fingerprints.length; j++) {
                fingerprints.add(byteArrayToString(section.fingerprints[j]));
            }
            sectionsFingerprints.add(fingerprints);
        }

        Set<String> packageNames = new HashSet<>();
        for (int i = 0; i < manifest.length; i++) {
            WebAppManifestSection section = manifest[i];
            AppInfo appInfo = mDefaultApplications.get(section.id);
            if (appInfo == null) continue;

            if (appInfo.version < section.minVersion) {
                Log.e(TAG, "\"%s\" version is %d, but at least %d is required.", section.id,
                        appInfo.version, section.minVersion);
                continue;
            }

            if (appInfo.sha256CertFingerprints == null) {
                Log.e(TAG, "Unable to determine fingerprints of \"%s\".", section.id);
                continue;
            }

            if (!appInfo.sha256CertFingerprints.equals(sectionsFingerprints.get(i))) {
                Log.e(TAG,
                        "\"%s\" fingerprints don't match the manifest. Expected %s, but found %s.",
                        section.id, setToString(sectionsFingerprints.get(i)),
                        setToString(appInfo.sha256CertFingerprints));
                continue;
            }

            packageNames.add(section.id);
        }

        return packageNames;
    }

    private static String setToString(Set<String> set) {
        StringBuilder result = new StringBuilder("[");
        for (String item : set) {
            result.append(' ');
            result.append(item);
        }
        result.append(" ]");
        return result.toString();
    }

    @Override
    public void onManifestDownloadFailure(String errorMessage) {
        if (mAtLeastOneManifestFailedToDownloadOrParse) return;
        mAtLeastOneManifestFailedToDownloadOrParse = true;

        mCallback.onVerificationError(errorMessage);

        if (mIsManifestCacheStaleOrUnusable) mCallback.onFinishedVerification();
        mCallback.onFinishedUsingResources();
    }

    @Override
    public void onManifestParseFailure() {
        if (mAtLeastOneManifestFailedToDownloadOrParse) return;
        mAtLeastOneManifestFailedToDownloadOrParse = true;

        if (mIsManifestCacheStaleOrUnusable) mCallback.onFinishedVerification();
        mCallback.onFinishedUsingResources();
    }
}