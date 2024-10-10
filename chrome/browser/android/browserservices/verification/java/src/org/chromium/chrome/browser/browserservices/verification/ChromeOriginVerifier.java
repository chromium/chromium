// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.verification;

import static org.chromium.chrome.browser.browserservices.metrics.OriginVerifierMetricsRecorder.recordVerificationResult;
import static org.chromium.chrome.browser.browserservices.metrics.OriginVerifierMetricsRecorder.recordVerificationTime;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.CustomTabsService.Relation;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.browserservices.metrics.OriginVerifierMetricsRecorder.VerificationResult;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.content_relationship_verification.OriginVerifier;
import org.chromium.components.content_relationship_verification.Relationship;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/**
 * Most classes that are Activity-scoped should take an {@link ChromeOriginVerifierFactory} and use
 * that to get instances of this.
 * Added functionality over {@link OriginVerifier}:
 *  - Parsing of {@link Relation} to String which is used in {@link OriginVerifier}.
 *  - Check for `ChromeSwitches.DISABLE_DIGITAL_ASSET_LINK_VERIFICATION` command line switch to skip
 * the verification.
 *  - Implementation of {@link wasPreviouslyVerified} using {@link ChromeVerificationResultStore}.
 *  - Clearing of data in {@link ChromeVerificationResultStore} as this safes data in
 * SharedPreferences.
 *  - Implementation of {@link isAllowlisted} for bypassing verification of TWA for {@code
 * mPackageName}.
 *  - Chrome specific metric logging.
 */
@JNINamespace("customtabs")
public class ChromeOriginVerifier extends OriginVerifier {
    private static final String TAG = "ChromeOriginVerifier";

    @Nullable private ExternalAuthUtils mExternalAuthUtils;

    static String relationToRelationship(@Relation int relation) {
        switch (relation) {
            case CustomTabsService.RELATION_USE_AS_ORIGIN:
                return OriginVerifier.USE_AS_ORIGIN;
            case CustomTabsService.RELATION_HANDLE_ALL_URLS:
                return OriginVerifier.HANDLE_ALL_URLS;
            default:
                assert false;
        }
        return null;
    }

    /**
     * Main constructor.
     * Use {@link ChromeOriginVerifier#start}
     * @param packageName The package for the Android application for verification.
     * @param relation Digital Asset Links {@link Relation} to use during verification.
     * @param webContents The web contents of the tab used for reporting errors to DevTools. Can be
     *         null if unavailable.
     * @param externalAuthUtils The auth utils used to check if an origin is allowlisted to bypass/
     * @param verificationResultStore The {@link ChromeVerificationResultStore} for persisting
     *         results.
     */
    public ChromeOriginVerifier(
            String packageName,
            @Relation int relation,
            @Nullable WebContents webContents,
            @Nullable ExternalAuthUtils externalAuthUtils,
            ChromeVerificationResultStore verificationResultStore) {
        super(
                packageName,
                relationToRelationship(relation),
                webContents,
                null,
                verificationResultStore);
        mExternalAuthUtils = externalAuthUtils;
    }

    /**
     * Verify the claimed origin for the cached package name asynchronously. This will end up
     * making a network request for non-cached origins with a URLFetcher using the last used
     * profile as context.
     * @param listener The listener who will get the verification result.
     * @param origin The postMessage origin the application is claiming to have. Can't be null.
     */
    @Override
    public void start(@NonNull OriginVerificationListener listener, @NonNull Origin origin) {
        ThreadUtils.assertOnUiThread();
        if (!isNativeOriginVerifierInitialized()) {
            initNativeOriginVerifier(ProfileManager.getLastUsedRegularProfile());
        }
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
        String disableDalUrl =
                CommandLine.getInstance()
                        .getSwitchValue(ChromeSwitches.DISABLE_DIGITAL_ASSET_LINK_VERIFICATION);
        if (!TextUtils.isEmpty(disableDalUrl) && origin.equals(Origin.create(disableDalUrl))) {
            Log.i(TAG, "Verification skipped for %s due to command line flag.", origin);
            PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, new VerifiedCallback(origin, true, null));
            return;
        }
        validate(origin);
    }

    @Override
    public boolean isAllowlisted(String packageName, Origin origin, String relation) {
        if (mExternalAuthUtils == null) return false;

        if (!relation.equals(HANDLE_ALL_URLS)) return false;

        return mExternalAuthUtils.isAllowlistedForTwaVerification(packageName, origin);
    }

    @Override
    public boolean wasPreviouslyVerified(Origin origin) {
        return wasPreviouslyVerified(mPackageName, mSignatureFingerprints, origin, mRelation);
    }

    /**
     * Returns whether an origin is first-party relative to a given package name.
     *
     * <p>This only returns data from previously cached relations, and does not trigger an
     * asynchronous validation. This cache is persisted across Chrome restarts. If you have an
     * instance of OriginVerifier, use {@link #wasPreviouslyVerified(Origin)} instead as that avoids
     * recomputing the signatureFingerprint of the package.
     *
     * @param packageName The package name.
     * @param origin The origin to verify.
     * @param relation The Digital Asset Links relation to verify for.
     */
    public static boolean wasPreviouslyVerified(
            String packageName, Origin origin, @Relation int relation) {
        List<String> fingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(packageName);

        // Some tests rely on passing in a package name that doesn't exist on the device, so the
        // fingerprints returned will be null. In this case, the package name will be overridden
        // with a call to OriginVerifier.addVerificationOverride, which is dealt with in
        // wasPreviouslyVerified.
        String fingerprint = fingerprints == null ? null : fingerprints.get(0);

        return wasPreviouslyVerified(
                packageName, fingerprint, origin, relationToRelationship(relation));
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
    private static boolean wasPreviouslyVerified(
            String packageName, String signatureFingerprint, Origin origin, String relation) {
        ChromeVerificationResultStore resultStore = ChromeVerificationResultStore.getInstance();
        return resultStore.shouldOverride(packageName, origin, relation)
                || resultStore.isRelationshipSaved(
                        new Relationship(
                                packageName,
                                Arrays.asList(signatureFingerprint),
                                origin,
                                relation));
    }

    /**
     * Returns whether an origin is first-party relative to a given package name.
     *
     * This only returns data from previously cached relations, and does not trigger an asynchronous
     * validation. This cache is persisted across Chrome restarts.
     *
     * @param packageName The package name.
     * @param signatureFingerprints The signatures of the package.
     * @param origin The origin to verify.
     * @param relation The Digital Asset Links relation to verify for.
     */
    private static boolean wasPreviouslyVerified(
            String packageName,
            List<String> signatureFingerprints,
            Origin origin,
            String relation) {
        ChromeVerificationResultStore resultStore = ChromeVerificationResultStore.getInstance();
        return resultStore.shouldOverride(packageName, origin, relation)
                || resultStore.isRelationshipSaved(
                        new Relationship(packageName, signatureFingerprints, origin, relation));
    }

    @Override
    public void recordResultMetrics(OriginVerifier.VerifierResult result) {
        switch (result) {
            case ONLINE_SUCCESS:
                recordVerificationResult(VerificationResult.ONLINE_SUCCESS);
                break;
            case ONLINE_FAILURE:
                recordVerificationResult(VerificationResult.ONLINE_FAILURE);
                break;
            case OFFLINE_SUCCESS:
                recordVerificationResult(VerificationResult.OFFLINE_SUCCESS);
                break;
            case OFFLINE_FAILURE:
                recordVerificationResult(VerificationResult.OFFLINE_FAILURE);
                break;
            case HTTPS_FAILURE:
                recordVerificationResult(VerificationResult.HTTPS_FAILURE);
                break;
            case REQUEST_FAILURE:
                recordVerificationResult(VerificationResult.REQUEST_FAILURE);
                break;
        }
    }

    public static void addVerificationOverride(
            String packageName, Origin origin, @Relation int relation) {
        ChromeVerificationResultStore.getInstance()
                .addOverride(packageName, origin, relationToRelationship(relation));
    }

    @Override
    public void initNativeOriginVerifier(BrowserContextHandle browserContextHandle) {
        setNativeOriginVerifier(
                ChromeOriginVerifierJni.get()
                        .init(ChromeOriginVerifier.this, browserContextHandle));
    }

    @Override
    public void recordVerificationTimeMetrics(long duration, boolean online) {
        recordVerificationTime(duration, online);
    }

    /** Clears all known relations. */
    public static void clearCachedVerificationsForTesting() {
        ChromeVerificationResultStore.getInstance().clearStoredRelationships();
    }

    /** Removes any data about sites visited from static variables and Android Preferences. */
    @CalledByNative
    public static void clearBrowsingData() {
        ChromeVerificationResultStore.getInstance().clearStoredRelationships();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        long init(ChromeOriginVerifier caller, BrowserContextHandle browserContextHandle);
    }
}
