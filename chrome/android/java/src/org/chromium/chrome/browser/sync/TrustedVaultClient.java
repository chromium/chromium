// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.PendingIntent;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Consumer;
import org.chromium.base.Promise;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

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

    // Set at most once by registerNative(), reset by unregisterNative(). May remain null (0) in
    // tests.
    private long mNativeTrustedVaultClientAndroid;

    // Set to true on the first call to registerNative(). Used to prevent a second instance from
    // being registered after unregistration.
    private boolean mRegisteredNative;

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
     * Notifies the registered native client (if any) that keys in the backend may have changed,
     * which usually leads to refetching the keys from the backend.
     */
    public void notifyKeysChanged() {
        if (mNativeTrustedVaultClientAndroid != 0) {
            TrustedVaultClientJni.get().notifyKeysChanged(mNativeTrustedVaultClientAndroid);
        }
    }

    /**
     * Notifies the registered native client (if any) that the recoverability state in the backend
     * may have changed, meaning that the value returned by getIsRecoverabilityDegraded() may have
     * changed.
     */
    public void notifyRecoverabilityChanged() {
        if (mNativeTrustedVaultClientAndroid != 0) {
            TrustedVaultClientJni.get().notifyRecoverabilityChanged(
                    mNativeTrustedVaultClientAndroid);
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
     * Registers a C++ client, which is a prerequisite before interacting with Java. Must be called
     * at most once.
     */
    @VisibleForTesting
    @CalledByNative
    public static void registerNative(long nativeTrustedVaultClientAndroid) {
        assert !get().mRegisteredNative
            : "Only one native client can be registered, even if the previous one was unregistered";
        get().mNativeTrustedVaultClientAndroid = nativeTrustedVaultClientAndroid;
        get().mRegisteredNative = true;
    }

    /**
     * Unregisters the previously-registered client, canceling any in-flight requests. Must be
     * called only if there is a registered client.
     * TODO(crbug.com/1081643): nativeTrustedVaultClientAndroid is only passed to assert that it
     * matches get().mNativeTrustedVaultClientAndroid. Is this really worth it?
     */
    @VisibleForTesting
    @CalledByNative
    public static void unregisterNative(long nativeTrustedVaultClientAndroid) {
        assert get().mNativeTrustedVaultClientAndroid != 0;
        assert get().mNativeTrustedVaultClientAndroid == nativeTrustedVaultClientAndroid;
        get().mNativeTrustedVaultClientAndroid = 0;
    }

    /**
     * Records TrustedVaultKeyRetrievalTrigger histogram.
     */
    public void recordKeyRetrievalTrigger(@TrustedVaultUserActionTriggerForUMA int trigger) {
        TrustedVaultClientJni.get().recordKeyRetrievalTrigger(trigger);
    }

    /**
     * Records TrustedVaultRecoverabilityDegradedFixTrigger histogram.
     */
    public void recordRecoverabilityDegradedFixTrigger(
            @TrustedVaultUserActionTriggerForUMA int trigger) {
        TrustedVaultClientJni.get().recordRecoverabilityDegradedFixTrigger(trigger);
    }

    /**
     * Forwards calls to Backend.fetchKeys() and upon completion invokes native method
     * fetchKeysCompleted().
     */
    @CalledByNative
    private static void fetchKeys(int requestId, CoreAccountInfo accountInfo) {
        Consumer<List<byte[]>> responseCb = keys -> {
            if (get().mNativeTrustedVaultClientAndroid == 0) {
                // Native already unregistered, no response needed.
                return;
            }
            TrustedVaultClientJni.get().fetchKeysCompleted(get().mNativeTrustedVaultClientAndroid,
                    requestId, accountInfo.getGaiaId(), keys.toArray(new byte[0][]));
        };
        get().mBackend.fetchKeys(accountInfo)
                .then(responseCb::accept, exception -> responseCb.accept(new ArrayList<byte[]>()));
    }

    /**
     * Forwards calls to Backend.markLocalKeysAsStale() and upon completion invokes native method
     * markLocalKeysAsStaleCompleted().
     */
    @CalledByNative
    private static void markLocalKeysAsStale(int requestId, CoreAccountInfo accountInfo) {
        Consumer<Boolean> responseCallback = succeeded -> {
            if (get().mNativeTrustedVaultClientAndroid == 0) {
                // Native already unregistered, no response needed.
                return;
            }
            TrustedVaultClientJni.get().markLocalKeysAsStaleCompleted(
                    get().mNativeTrustedVaultClientAndroid, requestId, succeeded);
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
    private static void getIsRecoverabilityDegraded(int requestId, CoreAccountInfo accountInfo) {
        Consumer<Boolean> responseCallback = isDegraded -> {
            if (get().mNativeTrustedVaultClientAndroid == 0) {
                // Native already unregistered, no response needed.
                return;
            }
            TrustedVaultClientJni.get().getIsRecoverabilityDegradedCompleted(
                    get().mNativeTrustedVaultClientAndroid, requestId, isDegraded);
        };

        get().mBackend
                .getIsRecoverabilityDegraded(accountInfo)
                // If an exception occurred, it's unknown whether recoverability is degraded. In
                // doubt reply with `false`, so the user isn't bothered with a prompt.
                .then(responseCallback::accept, exception -> responseCallback.accept(false));
    }

    @NativeMethods
    interface Natives {
        void fetchKeysCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, String gaiaId, byte[][] keys);
        void markLocalKeysAsStaleCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, boolean succeeded);
        void getIsRecoverabilityDegradedCompleted(
                long nativeTrustedVaultClientAndroid, int requestId, boolean isDegraded);
        void notifyKeysChanged(long nativeTrustedVaultClientAndroid);
        void notifyRecoverabilityChanged(long nativeTrustedVaultClientAndroid);
        void recordKeyRetrievalTrigger(@TrustedVaultUserActionTriggerForUMA int trigger);
        void recordRecoverabilityDegradedFixTrigger(
                @TrustedVaultUserActionTriggerForUMA int trigger);
    }
}
