// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.annotation.SuppressLint;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.support.annotation.NonNull;
import android.support.customtabs.CustomTabsService;
import android.support.customtabs.CustomTabsService.Relation;
import android.text.TextUtils;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.BrowserStartupController;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Locale;

/**
 * Used to verify postMessage origin for a designated package name.
 *
 * Uses Digital Asset Links to confirm that the given origin is associated with the package name as
 * a postMessage origin. It caches any origin that has been verified during the current application
 * lifecycle and reuses that without making any new network requests.
 *
 * The lifecycle of this object is governed by the owner. The owner has to call
 * {@link OriginVerifier#cleanUp()} for proper cleanup of dependencies.
 */
@JNINamespace("customtabs")
public class OriginVerifier {
    private static final String TAG = "OriginVerifier";
    private static final char[] HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray();
    private static final String USE_AS_ORIGIN = "delegate_permission/common.use_as_origin";
    private static final String HANDLE_ALL_URLS = "delegate_permission/common.handle_all_urls";

    private final OriginVerificationListener mListener;
    private final String mPackageName;
    private final String mSignatureFingerprint;
    private final @Relation int mRelation;
    private long mNativeOriginVerifier;
    private Origin mOrigin;

    /** Small helper class to post a result of origin verification. */
    private class VerifiedCallback implements Runnable {
        private final boolean mResult;
        private final Boolean mOnline;

        public VerifiedCallback(boolean result, Boolean online) {
            mResult = result;
            mOnline = online;
        }

        @Override
        public void run() {
            originVerified(mResult, mOnline);
        }
    }

    public static Uri getPostMessageUriFromVerifiedOrigin(String packageName,
            Origin verifiedOrigin) {
        return Uri.parse(IntentHandler.ANDROID_APP_REFERRER_SCHEME + "://"
                + verifiedOrigin.uri().getHost() + "/" + packageName);
    }

    /** Clears all known relations. */
    @VisibleForTesting
    public static void clearCachedVerificationsForTesting() {
        VerificationResultStore.clearStoredRelationships();
    }

    /**
     * Mark an origin as verified for a package.
     * @param packageName The package name to prepopulate for.
     * @param origin The origin to add as verified.
     * @param relation The Digital Asset Links relation verified.
     */
    public static void addVerifiedOriginForPackage(
            String packageName, Origin origin, @Relation int relation) {
        Log.d(TAG, "Adding: %s for %s", packageName, origin);
        VerificationResultStore.addRelationship(new Relationship(packageName, origin, relation));

        TrustedWebActivityClient.registerClient(ContextUtils.getApplicationContext(),
                origin, packageName);
    }

    /**
     * Returns whether an origin is first-party relative to a given package name.
     *
     * This only returns data from previously cached relations, and does not
     * trigger an asynchronous validation.
     *
     * @param packageName The package name
     * @param origin The origin to verify
     * @param relation The Digital Asset Links relation to verify for.
     */
    public static boolean isValidOrigin(String packageName, Origin origin, @Relation int relation) {
        return VerificationResultStore.isRelationshipSaved(
                new Relationship(packageName, origin, relation));
    }

    /**
     * Callback interface for getting verification results.
     */
    public interface OriginVerificationListener {
        /**
         * To be posted on the handler thread after the verification finishes.
         * @param packageName The package name for the origin verification query for this result.
         * @param origin The origin that was declared on the query for this result.
         * @param verified Whether the given origin was verified to correspond to the given package.
         * @param online Whether the device could connect to the internet to perform verification.
         *               Will be {@code null} if internet was not required for check (eg
         *               verification had already been attempted this Chrome lifetime and the
         *               result was cached or the origin was not https).
         */
        void onOriginVerified(String packageName, Origin origin, boolean verified, Boolean online);
    }

    /**
     * Main constructor.
     * Use {@link OriginVerifier#start(Origin)}
     * @param listener The listener who will get the verification result.
     * @param packageName The package for the Android application for verification.
     * @param relation Digital Asset Links {@link Relation} to use during verification.
     */
    public OriginVerifier(
            OriginVerificationListener listener, String packageName, @Relation int relation) {
        mListener = listener;
        mPackageName = packageName;
        mSignatureFingerprint = getCertificateSHA256FingerprintForPackage(mPackageName);
        mRelation = relation;
    }

    /**
     * Verify the claimed origin for the cached package name asynchronously. This will end up
     * making a network request for non-cached origins with a URLFetcher using the last used
     * profile as context.
     * @param origin The postMessage origin the application is claiming to have. Can't be null.
     */
    public void start(@NonNull Origin origin) {
        ThreadUtils.assertOnUiThread();
        mOrigin = origin;

        // Website to app Digital Asset Link verification can be skipped for a specific URL by
        // passing a command line flag to ease development.
        String disableDalUrl = CommandLine.getInstance().getSwitchValue(
                ChromeSwitches.DISABLE_DIGITAL_ASSET_LINK_VERIFICATION);
        if (!TextUtils.isEmpty(disableDalUrl)
                && mOrigin.equals(new Origin(disableDalUrl))) {
            Log.i(TAG, "Verification skipped for %s due to command line flag.", origin);
            ThreadUtils.runOnUiThread(new VerifiedCallback(true, null));
            return;
        }

        String scheme = mOrigin.uri().getScheme();
        if (TextUtils.isEmpty(scheme)
                || !UrlConstants.HTTPS_SCHEME.equals(scheme.toLowerCase(Locale.US))) {
            Log.i(TAG, "Verification failed for %s as not https.", origin);
            BrowserServicesMetrics.recordVerificationResult(
                    BrowserServicesMetrics.VerificationResult.HTTPS_FAILURE);
            ThreadUtils.runOnUiThread(new VerifiedCallback(false, null));
            return;
        }

        // If this origin is cached as verified already, use that.
        if (isValidOrigin(mPackageName, origin, mRelation)) {
            Log.i(TAG, "Verification succeeded for %s, it was cached.", origin);
            BrowserServicesMetrics.recordVerificationResult(
                    BrowserServicesMetrics.VerificationResult.CACHED_SUCCESS);
            ThreadUtils.runOnUiThread(new VerifiedCallback(true, null));
            return;
        }
        if (mNativeOriginVerifier != 0) cleanUp();
        if (!BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                        .isStartupSuccessfullyCompleted()) {
            // Early return for testing without native.
            return;
        }
        mNativeOriginVerifier = nativeInit(Profile.getLastUsedProfile().getOriginalProfile());
        assert mNativeOriginVerifier != 0;
        String relationship = null;
        switch (mRelation) {
            case CustomTabsService.RELATION_USE_AS_ORIGIN:
                relationship = USE_AS_ORIGIN;
                break;
            case CustomTabsService.RELATION_HANDLE_ALL_URLS:
                relationship = HANDLE_ALL_URLS;
                break;
            default:
                assert false;
                break;
        }

        boolean requestSent = nativeVerifyOrigin(mNativeOriginVerifier, mPackageName,
                mSignatureFingerprint, mOrigin.toString(), relationship);
        if (!requestSent) {
            BrowserServicesMetrics.recordVerificationResult(
                    BrowserServicesMetrics.VerificationResult.REQUEST_FAILURE);
            ThreadUtils.runOnUiThread(new VerifiedCallback(false, false));
        }
    }

    /**
     * Cleanup native dependencies on this object.
     */
    public void cleanUp() {
        if (mNativeOriginVerifier == 0) return;
        nativeDestroy(mNativeOriginVerifier);
        mNativeOriginVerifier = 0;
    }

    private static PackageInfo getPackageInfo(String packageName) {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();

        PackageInfo packageInfo = null;
        try {
            packageInfo = pm.getPackageInfo(packageName, PackageManager.GET_SIGNATURES);
        } catch (PackageManager.NameNotFoundException e) {
            // Will return null if there is no package found.
        }
        return packageInfo;
    }

    /**
     * Computes the SHA256 certificate for the given package name. The app with the given package
     * name has to be installed on device. The output will be a 30 long HEX string with : between
     * each value.
     * @param packageName The package name to query the signature for.
     * @return The SHA256 certificate for the package name.
     */
    @SuppressLint("PackageManagerGetSignatures")
    // https://stackoverflow.com/questions/39192844/android-studio-warning-when-using-packagemanager-get-signatures
    static String getCertificateSHA256FingerprintForPackage(String packageName) {
        PackageInfo packageInfo = getPackageInfo(packageName);
        if (packageInfo == null) return null;

        InputStream input = new ByteArrayInputStream(packageInfo.signatures[0].toByteArray());
        X509Certificate certificate = null;
        String hexString = null;
        try {
            certificate =
                    (X509Certificate) CertificateFactory.getInstance("X509").generateCertificate(
                            input);
            hexString = byteArrayToHexString(
                    MessageDigest.getInstance("SHA256").digest(certificate.getEncoded()));
        } catch (CertificateEncodingException e) {
            Log.w(TAG, "Certificate type X509 encoding failed");
        } catch (CertificateException | NoSuchAlgorithmException e) {
            // This shouldn't happen.
        }
        return hexString;
    }

    /**
     * Converts a byte array to hex string with : inserted between each element.
     * @param byteArray The array to be converted.
     * @return A string with two letters representing each byte and : in between.
     */
    static String byteArrayToHexString(byte[] byteArray) {
        StringBuilder hexString = new StringBuilder(byteArray.length * 3 - 1);
        for (int i = 0; i < byteArray.length; ++i) {
            hexString.append(HEX_CHAR_LOOKUP[(byteArray[i] & 0xf0) >>> 4]);
            hexString.append(HEX_CHAR_LOOKUP[byteArray[i] & 0xf]);
            if (i < (byteArray.length - 1)) hexString.append(':');
        }
        return hexString.toString();
    }

    /** Called asynchronously by nativeVerifyOrigin. */
    @CalledByNative
    private void onOriginVerificationResult(int result) {
        switch (result) {
            case RelationshipCheckResult.SUCCESS:
                BrowserServicesMetrics.recordVerificationResult(
                        BrowserServicesMetrics.VerificationResult.ONLINE_SUCCESS);
                originVerified(true, true);
                break;
            case RelationshipCheckResult.FAILURE:
                BrowserServicesMetrics.recordVerificationResult(
                        BrowserServicesMetrics.VerificationResult.ONLINE_FAILURE);
                originVerified(false, true);
                break;
            case RelationshipCheckResult.NO_CONNECTION:
                Log.i(TAG, "Device is offline, checking saved verification result.");
                checkForSavedResult();
                break;
            default:
                assert false;
        }
    }

    /** Deal with the result of an Origin check. Will be called on UI Thread. */
    private void originVerified(boolean originVerified, Boolean online) {
        Log.i(TAG, "Verification %s.", (originVerified ? "succeeded" : "failed"));
        if (originVerified) {
            addVerifiedOriginForPackage(mPackageName, mOrigin, mRelation);
        }

        // We save the result even if there is a failure as a way of overwriting a previously
        // successfully verified result that fails on a subsequent check.
        saveVerificationResult(originVerified);

        if (mListener != null) {
            mListener.onOriginVerified(mPackageName, mOrigin, originVerified, online);
        }
        cleanUp();
    }

    /**
     * Saves the result of a verification to Preferences so we can reuse it when offline.
     */
    private void saveVerificationResult(boolean originVerified) {
        Relationship relationship = new Relationship(mPackageName, mOrigin, mRelation);
        if (originVerified) {
            VerificationResultStore.addRelationship(relationship);
        } else {
            VerificationResultStore.removeRelationship(relationship);
        }
    }

    /**
     * Checks for a previously saved verification result.
     */
    private void checkForSavedResult() {
        try (StrictModeContext unused = StrictModeContext.allowDiskReads()) {
            boolean verified = VerificationResultStore.isRelationshipSaved(
                    new Relationship(mPackageName, mOrigin, mRelation));

            BrowserServicesMetrics.recordVerificationResult(verified
                            ? BrowserServicesMetrics.VerificationResult.OFFLINE_SUCCESS
                            : BrowserServicesMetrics.VerificationResult.OFFLINE_FAILURE);

            originVerified(verified, false);
        }
    }

    /**
     * Removes any data about sites visited from static variables and Android Preferences.
     */
    @CalledByNative
    public static void clearBrowsingData() {
        // TODO(peconn): Move this over to VerificationResultStore.
        VerificationResultStore.clearStoredRelationships();
    }

    private native long nativeInit(Profile profile);
    private native boolean nativeVerifyOrigin(long nativeOriginVerifier, String packageName,
            String signatureFingerprint, String origin, String relationship);
    private native void nativeDestroy(long nativeOriginVerifier);
}
