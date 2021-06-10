// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.sync.KeyRetrievalTriggerForUMA;
import org.chromium.components.sync.ModelType;
import org.chromium.components.sync.PassphraseType;

import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * JNI wrapper for the native ProfileSyncService.
 *
 * This class mostly makes calls to native and contains a minimum of business logic. It is only
 * usable from the UI thread as the native ProfileSyncService requires its access to be on the
 * UI thread. See components/sync/driver/sync_service_impl.h for more details.
 * TODO(crbug.com/1201272): Rename mNativeProfileSyncServiceAndroida and ProfileSyncService in docs.
 */
public class SyncServiceImpl extends SyncService {
    private final long mNativeProfileSyncServiceAndroid;

    private int mSetupInProgressCounter;

    // Sync state changes more often than listeners are added/removed, so using CopyOnWrite.
    private final List<SyncStateChangedListener> mListeners =
            new CopyOnWriteArrayList<SyncStateChangedListener>();

    /**
     * ModelTypes that the user can directly select in settings.
     * Logically, this is a subset of the native UserSelectableTypeSet, but it
     * uses values from the ModelType enum instead.
     * TODO(crbug.com/985290): Resolve this inconsistency.
     */
    private static final int[] ALL_SELECTABLE_TYPES = new int[] {
        ModelType.AUTOFILL,
        ModelType.BOOKMARKS,
        ModelType.PASSWORDS,
        ModelType.PREFERENCES,
        ModelType.PROXY_TABS,
        ModelType.TYPED_URLS
    };

    /** SyncService should be the only caller of this method. */
    public static @Nullable SyncServiceImpl create() {
        ThreadUtils.assertOnUiThread();
        SyncServiceImpl syncService = new SyncServiceImpl();
        return syncService.mNativeProfileSyncServiceAndroid == 0 ? null : syncService;
    }

    protected SyncServiceImpl() {
        // This may cause us to create ProfileSyncService even if sync has not
        // been set up, but ProfileSyncService won't actually start until
        // credentials are available.
        mNativeProfileSyncServiceAndroid = SyncServiceImplJni.get().init(this);
    }

    @Override
    public boolean isEngineInitialized() {
        return SyncServiceImplJni.get().isEngineInitialized(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isTransportStateActive() {
        return SyncServiceImplJni.get().isTransportStateActive(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean canSyncFeatureStart() {
        return SyncServiceImplJni.get().canSyncFeatureStart(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isSyncFeatureActive() {
        return SyncServiceImplJni.get().isSyncFeatureActive(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public @GoogleServiceAuthError.State int getAuthError() {
        int authErrorCode = SyncServiceImplJni.get().getAuthError(mNativeProfileSyncServiceAndroid);
        if (authErrorCode < 0 || authErrorCode >= GoogleServiceAuthError.State.NUM_ENTRIES) {
            throw new IllegalArgumentException("No state for code: " + authErrorCode);
        }
        return authErrorCode;
    }

    @Override
    public boolean isSyncDisabledByEnterprisePolicy() {
        return SyncServiceImplJni.get().isSyncDisabledByEnterprisePolicy(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean hasUnrecoverableError() {
        return SyncServiceImplJni.get().hasUnrecoverableError(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean requiresClientUpgrade() {
        return SyncServiceImplJni.get().requiresClientUpgrade(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setDecoupledFromAndroidMasterSync() {
        SyncServiceImplJni.get().setDecoupledFromAndroidMasterSync(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean getDecoupledFromAndroidMasterSync() {
        return SyncServiceImplJni.get().getDecoupledFromAndroidMasterSync(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public @Nullable CoreAccountInfo getAuthenticatedAccountInfo() {
        return SyncServiceImplJni.get().getAuthenticatedAccountInfo(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isAuthenticatedAccountPrimary() {
        return SyncServiceImplJni.get().isAuthenticatedAccountPrimary(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public Set<Integer> getActiveDataTypes() {
        int[] activeDataTypes =
                SyncServiceImplJni.get().getActiveDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(activeDataTypes);
    }

    @Override
    public Set<Integer> getChosenDataTypes() {
        int[] modelTypeArray =
                SyncServiceImplJni.get().getChosenDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(modelTypeArray);
    }

    @Override
    public boolean hasKeepEverythingSynced() {
        return SyncServiceImplJni.get().hasKeepEverythingSynced(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setChosenDataTypes(boolean syncEverything, Set<Integer> enabledTypes) {
        SyncServiceImplJni.get().setChosenDataTypes(mNativeProfileSyncServiceAndroid,
                syncEverything,
                syncEverything ? ALL_SELECTABLE_TYPES : modelTypeSetToArray(enabledTypes));
    }

    @Override
    public void setFirstSetupComplete(int syncFirstSetupCompleteSource) {
        SyncServiceImplJni.get().setFirstSetupComplete(
                mNativeProfileSyncServiceAndroid, syncFirstSetupCompleteSource);
    }

    @Override
    public boolean isFirstSetupComplete() {
        return SyncServiceImplJni.get().isFirstSetupComplete(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setSyncRequested(boolean requested) {
        SyncServiceImplJni.get().setSyncRequested(mNativeProfileSyncServiceAndroid, requested);
    }

    @Override
    public boolean isSyncRequested() {
        return SyncServiceImplJni.get().isSyncRequested(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Instances of this class keep sync paused until {@link #close} is called. Use
     * {@link SyncService#getSetupInProgressHandle} to create. Please note that
     * {@link #close} should be called on every instance of this class.
     */
    private class SyncSetupInProgressHandleImpl implements SyncSetupInProgressHandle {
        private boolean mClosed;

        public SyncSetupInProgressHandleImpl() {
            ThreadUtils.assertOnUiThread();
            if (++mSetupInProgressCounter == 1) {
                setSetupInProgress(true);
            }
        }

        @Override
        public void close() {
            ThreadUtils.assertOnUiThread();
            if (mClosed) return;
            mClosed = true;

            assert mSetupInProgressCounter > 0;
            if (--mSetupInProgressCounter == 0) {
                setSetupInProgress(false);
            }
        }
    }

    @Override
    public SyncSetupInProgressHandle getSetupInProgressHandle() {
        return new SyncSetupInProgressHandleImpl();
    }

    private void setSetupInProgress(boolean inProgress) {
        SyncServiceImplJni.get().setSetupInProgress(mNativeProfileSyncServiceAndroid, inProgress);
    }

    @Override
    public void addSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.add(listener);
    }

    @Override
    public void removeSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.remove(listener);
    }

    /**
     * Called when the state of the native sync engine has changed, so various
     * UI elements can update themselves.
     */
    @CalledByNative
    protected void syncStateChanged() {
        for (SyncStateChangedListener listener : mListeners) {
            listener.syncStateChanged();
        }
    }

    @Override
    public boolean isSyncAllowedByPlatform() {
        return SyncServiceImplJni.get().isSyncAllowedByPlatform(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setSyncAllowedByPlatform(boolean allowed) {
        SyncServiceImplJni.get().setSyncAllowedByPlatform(
                mNativeProfileSyncServiceAndroid, allowed);
    }

    @Override
    public @PassphraseType int getPassphraseType() {
        assert isEngineInitialized();
        int passphraseType =
                SyncServiceImplJni.get().getPassphraseType(mNativeProfileSyncServiceAndroid);
        if (passphraseType < 0 || passphraseType > PassphraseType.MAX_VALUE) {
            throw new IllegalArgumentException();
        }
        return passphraseType;
    }

    @Override
    public @Nullable Date getExplicitPassphraseTime() {
        assert isEngineInitialized();
        long timeInMilliseconds = SyncServiceImplJni.get().getExplicitPassphraseTime(
                mNativeProfileSyncServiceAndroid);
        return timeInMilliseconds != 0 ? new Date(timeInMilliseconds) : null;
    }

    @Override
    public boolean isUsingExplicitPassphrase() {
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isUsingExplicitPassphrase(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isPassphraseRequiredForPreferredDataTypes() {
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isPassphraseRequiredForPreferredDataTypes(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isTrustedVaultKeyRequired() {
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isTrustedVaultKeyRequired(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isTrustedVaultKeyRequiredForPreferredDataTypes() {
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isTrustedVaultKeyRequiredForPreferredDataTypes(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isTrustedVaultRecoverabilityDegraded() {
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isTrustedVaultRecoverabilityDegraded(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isCustomPassphraseAllowed() {
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isCustomPassphraseAllowed(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isEncryptEverythingEnabled() {
        assert isEngineInitialized();
        return SyncServiceImplJni.get().isEncryptEverythingEnabled(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setEncryptionPassphrase(String passphrase) {
        assert isEngineInitialized();
        SyncServiceImplJni.get().setEncryptionPassphrase(
                mNativeProfileSyncServiceAndroid, passphrase);
    }

    @Override
    public boolean setDecryptionPassphrase(String passphrase) {
        assert isEngineInitialized();
        return SyncServiceImplJni.get().setDecryptionPassphrase(
                mNativeProfileSyncServiceAndroid, passphrase);
    }

    @Override
    public boolean isPassphrasePromptMutedForCurrentProductVersion() {
        return SyncServiceImplJni.get().isPassphrasePromptMutedForCurrentProductVersion(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void markPassphrasePromptMutedForCurrentProductVersion() {
        SyncServiceImplJni.get().markPassphrasePromptMutedForCurrentProductVersion(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void recordKeyRetrievalTrigger(@KeyRetrievalTriggerForUMA int keyRetrievalTrigger) {
        SyncServiceImplJni.get().recordKeyRetrievalTrigger(
                mNativeProfileSyncServiceAndroid, keyRetrievalTrigger);
    }

    @Override
    public boolean shouldOfferTrustedVaultOptIn() {
        return SyncServiceImplJni.get().shouldOfferTrustedVaultOptIn(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isSyncingUrlsWithKeystorePassphrase() {
        return isEngineInitialized() && getActiveDataTypes().contains(ModelType.TYPED_URLS)
                && (getPassphraseType() == PassphraseType.KEYSTORE_PASSPHRASE
                        || getPassphraseType() == PassphraseType.TRUSTED_VAULT_PASSPHRASE);
    }

    @VisibleForTesting
    @Override
    public long getLastSyncedTimeForDebugging() {
        return SyncServiceImplJni.get().getLastSyncedTimeForDebugging(
                mNativeProfileSyncServiceAndroid);
    }

    @VisibleForTesting
    @Override
    public void triggerRefresh() {
        SyncServiceImplJni.get().triggerRefresh(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Invokes the onResult method of the callback from native code.
     */
    @CalledByNative
    private static void onGetAllNodesResult(Callback<JSONArray> callback, String serializedNodes) {
        try {
            callback.onResult(new JSONArray(serializedNodes));
        } catch (JSONException e) {
            callback.onResult(new JSONArray());
        }
    }

    @VisibleForTesting
    @Override
    public void getAllNodes(Callback<JSONArray> callback) {
        SyncServiceImplJni.get().getAllNodes(mNativeProfileSyncServiceAndroid, callback);
    }

    private static Set<Integer> modelTypeArrayToSet(int[] modelTypeArray) {
        Set<Integer> modelTypeSet = new HashSet<Integer>();
        for (int i = 0; i < modelTypeArray.length; i++) {
            modelTypeSet.add(modelTypeArray[i]);
        }
        return modelTypeSet;
    }

    private static int[] modelTypeSetToArray(Set<Integer> modelTypeSet) {
        int[] modelTypeArray = new int[modelTypeSet.size()];
        int i = 0;
        for (int modelType : modelTypeSet) {
            modelTypeArray[i++] = modelType;
        }
        return modelTypeArray;
    }

    @VisibleForTesting
    @Override
    public long getNativeSyncServiceImplForTest() {
        return SyncServiceImplJni.get().getNativeSyncServiceImplForTest(
                mNativeProfileSyncServiceAndroid);
    }

    @NativeMethods
    interface Natives {
        long init(SyncServiceImpl caller);

        // Please keep all methods below in the same order as sync_service_android_bridge.h.
        boolean isSyncRequested(long nativeSyncServiceAndroidBridge);
        void setSyncRequested(long nativeSyncServiceAndroidBridge, boolean requested);
        boolean canSyncFeatureStart(long nativeSyncServiceAndroidBridge);
        boolean isSyncAllowedByPlatform(long nativeSyncServiceAndroidBridge);
        void setSyncAllowedByPlatform(long nativeSyncServiceAndroidBridge, boolean allowed);
        boolean isSyncFeatureActive(long nativeSyncServiceAndroidBridge);
        boolean isSyncDisabledByEnterprisePolicy(long nativeSyncServiceAndroidBridge);
        boolean isEngineInitialized(long nativeSyncServiceAndroidBridge);
        boolean isTransportStateActive(long nativeSyncServiceAndroidBridge);
        void setSetupInProgress(long nativeSyncServiceAndroidBridge, boolean inProgress);
        boolean isFirstSetupComplete(long nativeSyncServiceAndroidBridge);
        void setFirstSetupComplete(
                long nativeSyncServiceAndroidBridge, int syncFirstSetupCompleteSource);
        int[] getActiveDataTypes(long nativeSyncServiceAndroidBridge);
        int[] getChosenDataTypes(long nativeSyncServiceAndroidBridge);
        void setChosenDataTypes(
                long nativeSyncServiceAndroidBridge, boolean syncEverything, int[] modelTypeArray);
        boolean isCustomPassphraseAllowed(long nativeSyncServiceAndroidBridge);
        boolean isEncryptEverythingEnabled(long nativeSyncServiceAndroidBridge);
        boolean isPassphraseRequiredForPreferredDataTypes(long nativeSyncServiceAndroidBridge);
        boolean isTrustedVaultKeyRequired(long nativeSyncServiceAndroidBridge);
        boolean isTrustedVaultKeyRequiredForPreferredDataTypes(long nativeSyncServiceAndroidBridge);
        boolean isTrustedVaultRecoverabilityDegraded(long nativeSyncServiceAndroidBridge);
        boolean isUsingExplicitPassphrase(long nativeSyncServiceAndroidBridge);
        int getPassphraseType(long nativeSyncServiceAndroidBridge);
        void setEncryptionPassphrase(long nativeSyncServiceAndroidBridge, String passphrase);
        boolean setDecryptionPassphrase(long nativeSyncServiceAndroidBridge, String passphrase);
        long getExplicitPassphraseTime(long nativeSyncServiceAndroidBridge);
        void getAllNodes(long nativeSyncServiceAndroidBridge, Callback<JSONArray> callback);
        int getAuthError(long nativeSyncServiceAndroidBridge);
        boolean hasUnrecoverableError(long nativeSyncServiceAndroidBridge);
        boolean requiresClientUpgrade(long nativeSyncServiceAndroidBridge);
        void setDecoupledFromAndroidMasterSync(long nativeSyncServiceAndroidBridge);
        boolean getDecoupledFromAndroidMasterSync(long nativeSyncServiceAndroidBridge);
        @Nullable
        CoreAccountInfo getAuthenticatedAccountInfo(long nativeSyncServiceAndroidBridge);
        boolean isAuthenticatedAccountPrimary(long nativeSyncServiceAndroidBridge);
        boolean isPassphrasePromptMutedForCurrentProductVersion(
                long nativeSyncServiceAndroidBridge);
        void markPassphrasePromptMutedForCurrentProductVersion(long nativeSyncServiceAndroidBridge);
        boolean hasKeepEverythingSynced(long nativeSyncServiceAndroidBridge);
        void recordKeyRetrievalTrigger(
                long nativeSyncServiceAndroidBridge, int keyRetrievalTrigger);
        boolean shouldOfferTrustedVaultOptIn(long nativeSyncServiceAndroidBridge);
        void triggerRefresh(long nativeSyncServiceAndroidBridge);
        long getLastSyncedTimeForDebugging(long nativeSyncServiceAndroidBridge);
        long getNativeSyncServiceImplForTest(long nativeSyncServiceAndroidBridge);
    }
}
