// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.PendingIntent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Promise;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 * Client used to communicate with GmsCore about sync encryption keys.
 */
public class TrustedVaultClient {
    /**
     * Interface to downstream functionality.
     */
    public interface Backend {
        /**
         * Reads and returns available encryption keys without involving any user action.
         *
         * @param accountInfo Account representing the user.
         * @return a promise with known keys, if any, where the last one is the most recent.
         */
        Promise<List<byte[]>> fetchKeys(CoreAccountInfo accountInfo);

        /**
         * Gets a PendingIntent that can be used to display a UI that allows the user to
         * reauthenticate and retrieve the sync encryption keys.
         *
         * @param accountInfo Account representing the user.
         * @return a promise for a PendingIntent object. The promise will be rejected if no
         *         retrieval is actually required.
         */
        Promise<PendingIntent> createKeyRetrievalIntent(CoreAccountInfo accountInfo);

        /**
         * Invoked when the result of fetchKeys() represents keys that cannot decrypt Nigori, which
         * should only be possible if the provided keys are not up-to-date.
         *
         * @param accountInfo Account representing the user.
         * @return a promise which indicates completion and also represents whether the operation
         * took any effect (false positives acceptable).
         */
        Promise<Boolean> markLocalKeysAsStale(CoreAccountInfo accountInfo);

        /**
         * Returns whether recoverability of the keys is degraded and user action is required to add
         * a new method. This may be called frequently and implementations are responsible for
         * implementing caching and possibly throttling.
         *
         * @param accountInfo Account representing the user.
         * @return a promise which indicates completion and representing whether recoverability is
         *         actually degraded.
         */
        Promise<Boolean> getIsRecoverabilityDegraded(CoreAccountInfo accountInfo);

        /**
         * Gets a PendingIntent that can be used to display a UI that allows the user to resolve a
         * degraded recoverability state, usually involving reauthentication.
         *
         * @param accountInfo Account representing the user.
         * @return a promise for a PendingIntent object. The promise will be rejected if no
         *         user action is actually required.
         */
        Promise<PendingIntent> createRecoverabilityDegradedIntent(CoreAccountInfo accountInfo);

        /**
         * Gets a PendingIntent that can be used to display a UI that allows the user to opt into
         * trusted vault encryption.
         *
         * @param accountInfo Account representing the user.
         * @return a promise for a PendingIntent object.
         */
        Promise<PendingIntent> createOptInIntent(CoreAccountInfo accountInfo);
    }

    /**
     * Trivial backend implementation that is always empty.
     */
    public static class EmptyBackend implements Backend {
        @Override
        public Promise<List<byte[]>> fetchKeys(CoreAccountInfo accountInfo) {
            return Promise.fulfilled(Collections.emptyList());
        }

        @Override
        public Promise<PendingIntent> createKeyRetrievalIntent(CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }

        @Override
        public Promise<Boolean> markLocalKeysAsStale(CoreAccountInfo accountInfo) {
            return Promise.fulfilled(false);
        }

        @Override
        public Promise<Boolean> getIsRecoverabilityDegraded(CoreAccountInfo accountInfo) {
            return Promise.fulfilled(false);
        }

        @Override
        public Promise<PendingIntent> createRecoverabilityDegradedIntent(
                CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }

        @Override
        public Promise<PendingIntent> createOptInIntent(CoreAccountInfo accountInfo) {
            return Promise.rejected();
        }
    };

    private static TrustedVaultClient sInstance;

    private final Backend mBackend;

    // Registered native TrustedVaultClientAndroid instances. Usually exactly one.
    private final Set<Long> mNativeTrustedVaultClientAndroidSet = new TreeSet<Long>();

    @VisibleForTesting
    public TrustedVaultClient(Backend backend) {
        assert backend != null;
        mBackend = backend;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(TrustedVaultClient instance) {
        sInstance = instance;
    }

    /**
     * Displays a UI that allows the user to reauthenticate and retrieve the sync encryption keys.
     */
    public static TrustedVaultClient get() {
        if (sInstance == null) {
            sInstance =
                    new TrustedVaultClient(AppHooks.get().createSyncTrustedVaultClientBackend());
        }
        return sInstance;
    }

    /**
     * Creates an intent that launches an activity that triggers the key retrieval UI.
     *
     * @param accountInfo Account representing the user.
     * @return a promise with the intent for opening the key retrieval activity. The promise will be
     *         rejected if no retrieval is actually required.
     */
    public Promise<PendingIntent> createKeyRetrievalIntent(CoreAccountInfo accountInfo) {
        return mBackend.createKeyRetrievalIntent(accountInfo);
    }

    /**
     * Notifies all registered native clients (in practice, exactly one) that keys in the backend
     * may have changed, which usually leads to refetching the keys from the backend.
     */
    public void notifyKeysChanged() {
        for (long nativeTrustedVaultClientAndroid : mNativeTrustedVaultClientAndroidSet) {
            TrustedVaultClientJni.get().notifyKeysChanged(nativeTrustedVaultClientAndroid);
        }
    }

    /**
     * Notifies all registered native clients (in practice, exactly one) that the recoverability
     * state in the backend may have changed, meaning that the value retuned by
     * getIsRecoverabilityDegraded() may have changed.
     */
    public void notifyRecoverabilityChanged() {
        for (long nativeTrustedVaultClientAndroid : mNativeTrustedVaultClientAndroidSet) {
            TrustedVaultClientJni.get().notifyRecoverabilityChanged(
                    nativeTrustedVaultClientAndroid);
        }
    }

    /**
     * Creates an intent that launches an activity that triggers the degraded recoverability UI.
     *
     * @param accountInfo Account representing the user.
     * @return a promise with the intent for opening the degraded recoverability activity. The
     *         promise will be rejected if no user action is actually required.
     */
    public Promise<PendingIntent> createRecoverabilityDegradedIntent(CoreAccountInfo accountInfo) {
        return mBackend.createRecoverabilityDegradedIntent(accountInfo);
    }

    /**
     * Creates an intent that launches an activity that triggers the opt in flow for trusted vault.
     *
     * @param accountInfo Account representing the user.
     * @return a promise with the intent for opening the opt-in activity.
     */
    public Promise<PendingIntent> createOptInIntent(CoreAccountInfo accountInfo) {
        return mBackend.createOptInIntent(accountInfo);
    }

    /**
     * Registers a C++ client, which is a prerequisite before interacting with Java.
     */
    @VisibleForTesting
    @CalledByNative
    public static void registerNative(long nativeTrustedVaultClientAndroid) {
        assert !isNativeRegistered(nativeTrustedVaultClientAndroid);
        get().mNativeTrustedVaultClientAndroidSet.add(nativeTrustedVaultClientAndroid);
    }

    /**
     * Unregisters a previously-registered client, canceling any in-flight requests.
     */
    @VisibleForTesting
    @CalledByNative
    public static void unregisterNative(long nativeTrustedVaultClientAndroid) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);
        get().mNativeTrustedVaultClientAndroidSet.remove(nativeTrustedVaultClientAndroid);
    }

    /**
     * Convenience function to check if a native client has been registered.
     */
    private static boolean isNativeRegistered(long nativeTrustedVaultClientAndroid) {
        return get().mNativeTrustedVaultClientAndroidSet.contains(nativeTrustedVaultClientAndroid);
    }

    /**
     * Forwards calls to Backend.fetchKeys() and upon completion invokes native method
     * fetchKeysCompleted().
     */
    @CalledByNative
    private static void fetchKeys(
            long nativeTrustedVaultClientAndroid, int requestId, CoreAccountInfo accountInfo) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);

        get().mBackend.fetchKeys(accountInfo)
                .then(
                        (keys)
                                -> {
                            if (isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                                TrustedVaultClientJni.get().fetchKeysCompleted(
                                        nativeTrustedVaultClientAndroid, requestId,
                                        accountInfo.getGaiaId(), keys.toArray(new byte[0][]));
                            }
                        },
                        (exception) -> {
                            if (isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                                TrustedVaultClientJni.get().fetchKeysCompleted(
                                        nativeTrustedVaultClientAndroid, requestId,
                                        accountInfo.getGaiaId(), new byte[0][]);
                            }
                        });
    }

    /**
     * Forwards calls to Backend.markLocalKeysAsStale() and upon completion invokes native method
     * markLocalKeysAsStaleCompleted().
     */
    @CalledByNative
    private static void markLocalKeysAsStale(
            long nativeTrustedVaultClientAndroid, int requestId, CoreAccountInfo accountInfo) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);

        get().mBackend.markLocalKeysAsStale(accountInfo)
                .then(
                        (result)
                                -> {
                            if (isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                                TrustedVaultClientJni.get().markLocalKeysAsStaleCompleted(
                                        nativeTrustedVaultClientAndroid, requestId, result);
                            }
                        },
                        (exception) -> {
                            if (isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                                // There's no certainty about whether the operation made any
                                // difference so let's return true indicating that it might have,
                                // since false positives are allowed.
                                TrustedVaultClientJni.get().markLocalKeysAsStaleCompleted(
                                        nativeTrustedVaultClientAndroid, requestId, true);
                            }
                        });
    }

    /**
     * Forwards calls to Backend.getIsRecoverabilityDegraded() and upon completion invokes native
     * method getIsRecoverabilityDegradedCompleted().
     */
    @CalledByNative
    private static void getIsRecoverabilityDegraded(
            long nativeTrustedVaultClientAndroid, int requestId, CoreAccountInfo accountInfo) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);

        get().mBackend.getIsRecoverabilityDegraded(accountInfo)
                .then(
                        (result)
                                -> {
                            if (isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                                TrustedVaultClientJni.get().getIsRecoverabilityDegradedCompleted(
                                        nativeTrustedVaultClientAndroid, requestId, result);
                            }
                        },
                        (exception) -> {
                            if (isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                                // In doubt, let's not bother the user with a prompt.
                                TrustedVaultClientJni.get().getIsRecoverabilityDegradedCompleted(
                                        nativeTrustedVaultClientAndroid, requestId, false);
                            }
                        });
    }

    @NativeMethods
    interface Natives {
        void fetchKeysCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, String gaiaId, byte[][] keys);
        void markLocalKeysAsStaleCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, boolean result);
        void getIsRecoverabilityDegradedCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, boolean result);
        void notifyKeysChanged(long nativeTrustedVaultClientAndroid);
        void notifyRecoverabilityChanged(long nativeTrustedVaultClientAndroid);
    }
}
