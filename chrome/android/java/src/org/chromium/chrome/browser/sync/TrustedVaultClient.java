// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.PendingIntent;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Promise;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;
import java.util.function.Consumer;

/** Client used to communicate with GmsCore about sync encryption keys. */
public class TrustedVaultClient {
    /** Interface to downstream functionality. */
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
         * Registers a new trusted recovery method that can be used to retrieve keys,
         * usually for the purpose of resolving a recoverability-degraded case.
         *
         * @param accountInfo Account representing the user.
         * @param publicKey Public key representing the recovery method.
         * @param methodTypeHint Opaque value provided by the server (e.g. via Javascript).
         * @return a promise which indicates completion.
         */
        Promise<Void> addTrustedRecoveryMethod(
                CoreAccountInfo accountInfo, byte[] publicKey, int methodTypeHint);

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

    /** Trivial backend implementation that is always empty. */
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
        public Promise<Void> addTrustedRecoveryMethod(
                CoreAccountInfo accountInfo, byte[] publicKey, int methodTypeHint) {
            return Promise.rejected();
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
    }

    private static TrustedVaultClient sInstance;

    private final Backend mBackend;

    // Registered native TrustedVaultClientAndroid instances. Usually exactly one.
    private final Set<Long> mNativeTrustedVaultClientAndroidSet = new TreeSet<Long>();

    @VisibleForTesting
    public TrustedVaultClient(Backend backend) {
        assert backend != null;
        mBackend = backend;
    }

    public static void setInstanceForTesting(TrustedVaultClient instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    /**
     * Displays a UI that allows the user to reauthenticate and retrieve the sync encryption keys.
     */
    public static TrustedVaultClient get() {
        if (sInstance == null) {
            TrustedVaultClient.Backend backend =
                    ServiceLoaderUtil.maybeCreate(TrustedVaultClient.Backend.class);
            if (backend == null) {
                backend = new TrustedVaultClient.EmptyBackend();
            }
            sInstance = new TrustedVaultClient(backend);
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
     * state in the backend may have changed, meaning that the value returned by
     * getIsRecoverabilityDegraded() may have changed.
     */
    public void notifyRecoverabilityChanged() {
        for (long nativeTrustedVaultClientAndroid : mNativeTrustedVaultClientAndroidSet) {
            TrustedVaultClientJni.get()
                    .notifyRecoverabilityChanged(nativeTrustedVaultClientAndroid);
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
     * Registers a C++ client, which is a prerequisite before interacting with Java. Must not be
     * called if the client is already registered.
     */
    @VisibleForTesting
    @CalledByNative
    public static void registerNative(long nativeTrustedVaultClientAndroid) {
        assert !isNativeRegistered(nativeTrustedVaultClientAndroid);
        get().mNativeTrustedVaultClientAndroidSet.add(nativeTrustedVaultClientAndroid);
    }

    /**
     * Unregisters a previously-registered client, canceling any in-flight requests. Must be called
     * only if the client is currently registered.
     */
    @VisibleForTesting
    @CalledByNative
    public static void unregisterNative(long nativeTrustedVaultClientAndroid) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);
        get().mNativeTrustedVaultClientAndroidSet.remove(nativeTrustedVaultClientAndroid);
    }

    /** Records TrustedVaultKeyRetrievalTrigger histogram. */
    public void recordKeyRetrievalTrigger(@TrustedVaultUserActionTriggerForUMA int trigger) {
        TrustedVaultClientJni.get().recordKeyRetrievalTrigger(trigger);
    }

    /** Records TrustedVaultRecoverabilityDegradedFixTrigger histogram. */
    public void recordRecoverabilityDegradedFixTrigger(
            @TrustedVaultUserActionTriggerForUMA int trigger) {
        TrustedVaultClientJni.get().recordRecoverabilityDegradedFixTrigger(trigger);
    }

    /** Convenience function to check if a native client has been registered. */
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

        Consumer<List<byte[]>> responseCb =
                keys -> {
                    if (!isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                        // Native already unregistered, no response needed.
                        return;
                    }
                    TrustedVaultClientJni.get()
                            .fetchKeysCompleted(
                                    nativeTrustedVaultClientAndroid,
                                    requestId,
                                    accountInfo.getGaiaId(),
                                    keys.toArray(new byte[0][]));
                };
        get().mBackend
                .fetchKeys(accountInfo)
                .then(responseCb::accept, exception -> responseCb.accept(new ArrayList<byte[]>()));
    }

    /**
     * Forwards calls to Backend.markLocalKeysAsStale() and upon completion invokes native method
     * markLocalKeysAsStaleCompleted().
     */
    @CalledByNative
    private static void markLocalKeysAsStale(
            long nativeTrustedVaultClientAndroid, int requestId, CoreAccountInfo accountInfo) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);

        Consumer<Boolean> responseCallback =
                succeeded -> {
                    if (!isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                        // Native already unregistered, no response needed.
                        return;
                    }
                    TrustedVaultClientJni.get()
                            .markLocalKeysAsStaleCompleted(
                                    nativeTrustedVaultClientAndroid, requestId, succeeded);
                };
        get().mBackend
                .markLocalKeysAsStale(accountInfo)
                // If an exception occurred, it's unknown whether the operation made any
                // difference. In doubt return true, since false positives are allowed.
                .then(responseCallback::accept, exception -> responseCallback.accept(true));
    }

    /**
     * Forwards calls to Backend.getIsRecoverabilityDegraded() and upon completion invokes native
     * method getIsRecoverabilityDegradedCompleted().
     */
    @CalledByNative
    private static void getIsRecoverabilityDegraded(
            long nativeTrustedVaultClientAndroid, int requestId, CoreAccountInfo accountInfo) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);

        Consumer<Boolean> responseCallback =
                isDegraded -> {
                    if (!isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                        // Native already unregistered, no response needed.
                        return;
                    }
                    TrustedVaultClientJni.get()
                            .getIsRecoverabilityDegradedCompleted(
                                    nativeTrustedVaultClientAndroid, requestId, isDegraded);
                };

        get().mBackend
                .getIsRecoverabilityDegraded(accountInfo)
                // If an exception occurred, it's unknown whether recoverability is degraded. In
                // doubt reply with `false`, so the user isn't bothered with a prompt.
                .then(responseCallback::accept, exception -> responseCallback.accept(false));
    }

    /**
     * Forwards calls to Backend.addTrustedRecoveryMethod() and upon completion invokes native
     * method addTrustedRecoveryMethodCompleted().
     */
    @CalledByNative
    private static void addTrustedRecoveryMethod(
            long nativeTrustedVaultClientAndroid,
            int requestId,
            CoreAccountInfo accountInfo,
            byte[] publicKey,
            int methodTypeHint) {
        assert isNativeRegistered(nativeTrustedVaultClientAndroid);

        Consumer<Boolean> responseCallback =
                success -> {
                    if (!isNativeRegistered(nativeTrustedVaultClientAndroid)) {
                        // Native already unregistered, no response needed.
                        return;
                    }
                    RecordHistogram.recordBooleanHistogram(
                            "Sync.TrustedVaultJavascriptAddRecoveryMethodSucceeded", success);
                    TrustedVaultClientJni.get()
                            .addTrustedRecoveryMethodCompleted(
                                    nativeTrustedVaultClientAndroid, requestId);
                };

        get().mBackend
                .addTrustedRecoveryMethod(accountInfo, publicKey, methodTypeHint)
                .then(
                        unused -> responseCallback.accept(true),
                        exception -> responseCallback.accept(false));
    }

    @NativeMethods
    interface Natives {
        void fetchKeysCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, String gaiaId, byte[][] keys);

        void markLocalKeysAsStaleCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, boolean succeeded);

        void getIsRecoverabilityDegradedCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, boolean isDegraded);

        void addTrustedRecoveryMethodCompleted(long nativeTrustedVaultClientAndroid, int requestId);

        void notifyKeysChanged(long nativeTrustedVaultClientAndroid);

        void notifyRecoverabilityChanged(long nativeTrustedVaultClientAndroid);

        void recordKeyRetrievalTrigger(@TrustedVaultUserActionTriggerForUMA int trigger);

        void recordRecoverabilityDegradedFixTrigger(
                @TrustedVaultUserActionTriggerForUMA int trigger);
    }
}
