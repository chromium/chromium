// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import android.annotation.SuppressLint;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsService.Relation;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.browserservices.BrowserServicesMetrics;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.cert.CertificateEncodingException;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.X509Certificate;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.atomic.AtomicReference;

import javax.inject.Inject;

import dagger.Reusable;

/**
 * Use to check that an app has a Digital Asset Link relationship with the given origin.
 *
 * Multiple instances of this object share a static cache, and as such the static
 * {@link #wasPreviouslyVerified} can be used to check whether any verification has been carried
 * out.
 *
 * One instance of this object should be created per package, but {@link #start} may be called
 * multiple times to verify different origins. This object has a native counterpart that will be
 * kept alive as it is serving requests, but destroyed once all requests are finished.
 */
@JNINamespace("customtabs")
public class OriginVerifier {
    private static final String TAG = "OriginVerifier";
    private static final char[] HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray();
    private static final String USE_AS_ORIGIN = "delegate_permission/common.use_as_origin";
    private static final String HANDLE_ALL_URLS = "delegate_permission/common.handle_all_urls";

    private final String mPackageName;
    private final String mSignatureFingerprint;
    private final @Relation int mRelation;
    private long mNativeOriginVerifier;
    private final Map<Origin, Set<OriginVerificationListener>> mListeners = new HashMap<>();
    private long mVerificationStartTime;
    @Nullable
    private WebContents mWebContents;
    @Nullable
    private ExternalAuthUtils mExternalAuthUtils;

    /**
     * A collection of Relationships (stored as Strings, with the signature set to an empty String)
     * that we override verifications to succeed for. It is threadsafe.
     */
    private static final AtomicReference<Set<String>> sVerificationOverrides =
            new AtomicReference<>();

    /**
     * Factory that can be injected by Dagger.
     */
    @Reusable
    public static class Factory {
        @Inject
        public Factory() {}

        public OriginVerifier create(String packageName, @Relation int relation,
                @Nullable WebContents webContents, @Nullable ExternalAuthUtils externalAuthUtils) {
            return new OriginVerifier(packageName, relation, webContents, externalAuthUtils);
        }
    }

    /** Small helper class to post a result of origin verification. */
    private class VerifiedCallback implements Runnable {
        private final Origin mOrigin;
        private final boolean mResult;
        private final Boolean mOnline;

        public VerifiedCallback(Origin origin, boolean result, Boolean online) {
            mOrigin = origin;
            mResult = result;
            mOnline = online;
        }

        @Override
        public void run() {
            originVerified(mOrigin, mResult, mOnline);
        }
    }

    public static Uri getPostMessageUriFromVerifiedOrigin(
            String packageName, Origin verifiedOrigin) {
        return Uri.parse(IntentUtils.ANDROID_APP_REFERRER_SCHEME + "://"
                + verifiedOrigin.uri().getHost() + "/" + packageName);
    }

    /** Clears all known relations. */
    @VisibleForTesting
    public static void clearCachedVerificationsForTesting() {
        VerificationResultStore.clearStoredRelationships();
        if (sVerificationOverrides.get() != null) {
            sVerificationOverrides.get().clear();
        }
    }

    /**
     * Ensures that subsequent calls to {@link OriginVerifier#start} result in a success without
     * performing the full check.
     */
    public static void addVerificationOverride(
            String packageName, Origin origin, int relationship) {
        if (sVerificationOverrides.get() == null) {
            sVerificationOverrides.compareAndSet(
                    null, Collections.synchronizedSet(new HashSet<>()));
        }
        sVerificationOverrides.get().add(
                new Relationship(packageName, "", origin, relationship).toString());
    }

    /**
     * Checks whether the origin was verified for that origin with a call to {@link #start}.
     */
    public boolean wasPreviouslyVerified(Origin origin) {
        return wasPreviouslyVerified(mPackageName, mSignatureFingerprint, origin, mRelation);
    }

    /**
     * Returns whether an origin is first-party relative to a given package name.
     *
     * This only returns data from previously cached relations, and does not trigger an asynchronous
     * validation. This cache is persisted across Chrome restarts. If you have an instance of
     * OriginVerifier, use {@link #wasPreviouslyVerified(Origin)} instead as that avoids recomputing
     * the signatureFingerprint of the package.
     *
     * @param packageName The package name.
     * @param origin The origin to verify.
     * @param relation The Digital Asset Links relation to verify for.
     */
    public static boolean wasPreviouslyVerified(
            String packageName, Origin origin, @Relation int relation) {
        return wasPreviouslyVerified(packageName,
                getCertificateSHA256FingerprintForPackage(packageName), origin, relation);
    }

    /**
     * Returns whether an origin is first-party relative to a given package name.
     *
     * This only returns data from previously cached relations, and does not trigger an asynchronous
     * validation. This cache is persisted across Chrome restarts.
     *
     * @param packageName The package name.
     * @param signatureFingerprint The signature of the package.
     * @param origin The origin to verify.
     * @param relation The Digital Asset Links relation to verify for.
     */
    private static boolean wasPreviouslyVerified(String packageName, String signatureFingerprint,
            Origin origin, @Relation int relation) {
        return shouldOverrideVerification(packageName, origin, relation)
                || VerificationResultStore.isRelationshipSaved(
                        new Relationship(packageName, signatureFingerprint, origin, relation));
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
     * Use {@link OriginVerifier#start}
     * @param packageName The package for the Android application for verification.
     * @param relation Digital Asset Links {@link Relation} to use during verification.
     * @param webContents The web contents of the tab used for reporting errors to DevTools. Can be
     *         null if unavailable.
     * @param externalAuthUtils The auth utils used to check if an origin is allowlisted to bypass
     *         verification. Can be null.
     */
    public OriginVerifier(String packageName, @Relation int relation,
            @Nullable WebContents webContents, @Nullable ExternalAuthUtils externalAuthUtils) {
        mPackageName = packageName;
        mSignatureFingerprint = getCertificateSHA256FingerprintForPackage(mPackageName);
        mRelation = relation;
        mWebContents = webContents;
        mExternalAuthUtils = externalAuthUtils;
    }

    /**
     * Verify the claimed origin for the cached package name asynchronously. This will end up
     * making a network request for non-cached origins with a URLFetcher using the last used
     * profile as context.
     * @param origin The postMessage origin the application is claiming to have. Can't be null.
     * @param listener The listener who will get the verification result.
     */
    public void start(@NonNull OriginVerificationListener listener, @NonNull Origin origin) {
        ThreadUtils.assertOnUiThread();
        if (mListeners.containsKey(origin)) {
            // We already have an ongoing verification for that origin, just add the listener.
            mListeners.get(origin).add(listener);
            return;
        } else {
            mListeners.put(origin, new HashSet<>());
            mListeners.get(origin).add(listener);
        }

        // Website to app Digital Asset Link verification can be skipped for a specific URL by
        // passing a command line flag to ease development.
        String disableDalUrl = CommandLine.getInstance().getSwitchValue(
                ChromeSwitches.DISABLE_DIGITAL_ASSET_LINK_VERIFICATION);
        if (!TextUtils.isEmpty(disableDalUrl) && origin.equals(Origin.create(disableDalUrl))) {
            Log.i(TAG, "Verification skipped for %s due to command line flag.", origin);
            PostTask.runOrPostTask(
                    UiThreadTaskTraits.DEFAULT, new VerifiedCallback(origin, true, null));
            return;
        }

        String scheme = origin.uri().getScheme();
        if (TextUtils.isEmpty(scheme)
                || !UrlConstants.HTTPS_SCHEME.equals(scheme.toLowerCase(Locale.US))) {
            Log.i(TAG, "Verification failed for %s as not https.", origin);
            BrowserServicesMetrics.recordVerificationResult(
                    BrowserServicesMetrics.VerificationResult.HTTPS_FAILURE);
            PostTask.runOrPostTask(
                    UiThreadTaskTraits.DEFAULT, new VerifiedCallback(origin, false, null));
            return;
        }

        if (shouldOverrideVerification(mPackageName, origin, mRelation)) {
            Log.i(TAG, "Verification succeeded for %s, it was overridden.", origin);
            PostTask.runOrPostTask(
                    UiThreadTaskTraits.DEFAULT, new VerifiedCallback(origin, true, null));
            return;
        }

        if (isAllowlisted(mPackageName, origin, mRelation)) {
            Log.i(TAG, "Verification succeeded for %s, %s, it was allowlisted.", mPackageName,
                    origin);
            PostTask.runOrPostTask(
                    UiThreadTaskTraits.DEFAULT, new VerifiedCallback(origin, true, null));
            return;
        }

        // Early return for testing without native.
        if (!BrowserStartupController.getInstance().isFullBrowserStarted()) return;

        if (mWebContents != null && mWebContents.isDestroyed()) mWebContents = null;

        // If the native side doesn't exist, create it.
        if (mNativeOriginVerifier == 0) {
            mNativeOriginVerifier = OriginVerifierJni.get().init(
                    OriginVerifier.this, mWebContents, Profile.getLastUsedRegularProfile());
            assert mNativeOriginVerifier != 0;
        }

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

        mVerificationStartTime = SystemClock.uptimeMillis();
        boolean requestSent =
                OriginVerifierJni.get().verifyOrigin(mNativeOriginVerifier, OriginVerifier.this,
                        mPackageName, mSignatureFingerprint, origin.toString(), relationship);
        if (!requestSent) {
            BrowserServicesMetrics.recordVerificationResult(
                    BrowserServicesMetrics.VerificationResult.REQUEST_FAILURE);
            PostTask.runOrPostTask(
                    UiThreadTaskTraits.DEFAULT, new VerifiedCallback(origin, false, false));
        }
    }

    private static boolean shouldOverrideVerification(
            String packageName, Origin origin, int relation) {
        if (sVerificationOverrides.get() == null) return false;

        return sVerificationOverrides.get().contains(
                new Relationship(packageName, "", origin, relation).toString());
    }

    private boolean isAllowlisted(String packageName, Origin origin, int relation) {
        if (mExternalAuthUtils == null) return false;

        if (relation != CustomTabsService.RELATION_HANDLE_ALL_URLS) return false;

        return mExternalAuthUtils.isAllowlistedForTwaVerification(packageName, origin);
    }

    /**
     * Cleanup native dependencies on this object.
     */
    public void cleanUp() {
        // Only destroy native once we have no other pending verifications.
        if (!mListeners.isEmpty()) return;
        if (mNativeOriginVerifier == 0) return;
        OriginVerifierJni.get().destroy(mNativeOriginVerifier, OriginVerifier.this);
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
        String hexString = null;
        try {
            X509Certificate certificate =
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

    /** Called asynchronously by OriginVerifierJni.get().verifyOrigin. */
    @CalledByNative
    private void onOriginVerificationResult(String originAsString, int result) {
        Origin origin = Origin.createOrThrow(originAsString);
        switch (result) {
            case RelationshipCheckResult.SUCCESS:
                BrowserServicesMetrics.recordVerificationResult(
                        BrowserServicesMetrics.VerificationResult.ONLINE_SUCCESS);
                originVerified(origin, true, true);
                break;
            case RelationshipCheckResult.FAILURE:
                BrowserServicesMetrics.recordVerificationResult(
                        BrowserServicesMetrics.VerificationResult.ONLINE_FAILURE);
                originVerified(origin, false, true);
                break;
            case RelationshipCheckResult.NO_CONNECTION:
                Log.i(TAG, "Device is offline, checking saved verification result.");
                checkForSavedResult(origin);
                break;
            default:
                assert false;
        }
    }

    /** Deal with the result of an Origin check. Will be called on UI Thread. */
    private void originVerified(Origin origin, boolean originVerified, Boolean online) {
        if (originVerified) {
            Log.d(TAG, "Adding: %s for %s", mPackageName, origin);
            VerificationResultStore.addRelationship(
                    new Relationship(mPackageName, mSignatureFingerprint, origin, mRelation));
        } else {
            Log.d(TAG,
                    "Digital Asset Link verification failed for package %s with "
                            + "fingerprint %s.",
                    mPackageName, mSignatureFingerprint);
        }

        // We save the result even if there is a failure as a way of overwriting a previously
        // successfully verified result that fails on a subsequent check.
        saveVerificationResult(origin, originVerified);

        if (mListeners.containsKey(origin)) {
            Set<OriginVerificationListener> listeners = mListeners.get(origin);
            for (OriginVerificationListener listener : listeners) {
                listener.onOriginVerified(mPackageName, origin, originVerified, online);
            }
            mListeners.remove(origin);
        }

        if (online != null) {
            long duration = SystemClock.uptimeMillis() - mVerificationStartTime;
            BrowserServicesMetrics.recordVerificationTime(duration, online);
        }

        cleanUp();
    }

    /**
     * Saves the result of a verification to Preferences so we can reuse it when offline.
     */
    private void saveVerificationResult(Origin origin, boolean originVerified) {
        Relationship relationship =
                new Relationship(mPackageName, mSignatureFingerprint, origin, mRelation);
        if (originVerified) {
            VerificationResultStore.addRelationship(relationship);
        } else {
            VerificationResultStore.removeRelationship(relationship);
        }
    }

    /**
     * Checks for a previously saved verification result.
     */
    private void checkForSavedResult(Origin origin) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            boolean verified = VerificationResultStore.isRelationshipSaved(
                    new Relationship(mPackageName, mSignatureFingerprint, origin, mRelation));

            BrowserServicesMetrics.recordVerificationResult(verified
                            ? BrowserServicesMetrics.VerificationResult.OFFLINE_SUCCESS
                            : BrowserServicesMetrics.VerificationResult.OFFLINE_FAILURE);

            originVerified(origin, verified, false);
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

    @NativeMethods
    interface Natives {
        long init(OriginVerifier caller, @Nullable WebContents webContents, Profile profile);
        boolean verifyOrigin(long nativeOriginVerifier, OriginVerifier caller, String packageName,
                String signatureFingerprint, String origin, String relationship);
        void destroy(long nativeOriginVerifier, OriginVerifier caller);
    }
}
