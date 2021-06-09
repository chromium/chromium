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
 */
public class ProfileSyncService extends SyncService {
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
    public static @Nullable ProfileSyncService create() {
        ThreadUtils.assertOnUiThread();
        ProfileSyncService syncService = new ProfileSyncService();
        return syncService.mNativeProfileSyncServiceAndroid == 0 ? null : syncService;
    }

    protected ProfileSyncService() {
        // This may cause us to create ProfileSyncService even if sync has not
        // been set up, but ProfileSyncService won't actually start until
        // credentials are available.
        mNativeProfileSyncServiceAndroid = ProfileSyncServiceJni.get().init(this);
    }

    @Override
    public boolean isEngineInitialized() {
        return ProfileSyncServiceJni.get().isEngineInitialized(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isTransportStateActive() {
        return ProfileSyncServiceJni.get().isTransportStateActive(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean canSyncFeatureStart() {
        return ProfileSyncServiceJni.get().canSyncFeatureStart(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isSyncFeatureActive() {
        return ProfileSyncServiceJni.get().isSyncFeatureActive(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public @GoogleServiceAuthError.State int getAuthError() {
        int authErrorCode =
                ProfileSyncServiceJni.get().getAuthError(mNativeProfileSyncServiceAndroid);
        if (authErrorCode < 0 || authErrorCode >= GoogleServiceAuthError.State.NUM_ENTRIES) {
            throw new IllegalArgumentException("No state for code: " + authErrorCode);
        }
        return authErrorCode;
    }

    @Override
    public boolean isSyncDisabledByEnterprisePolicy() {
        return ProfileSyncServiceJni.get().isSyncDisabledByEnterprisePolicy(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean hasUnrecoverableError() {
        return ProfileSyncServiceJni.get().hasUnrecoverableError(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean requiresClientUpgrade() {
        return ProfileSyncServiceJni.get().requiresClientUpgrade(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setDecoupledFromAndroidMasterSync() {
        ProfileSyncServiceJni.get().setDecoupledFromAndroidMasterSync(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean getDecoupledFromAndroidMasterSync() {
        return ProfileSyncServiceJni.get().getDecoupledFromAndroidMasterSync(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public @Nullable CoreAccountInfo getAuthenticatedAccountInfo() {
        return ProfileSyncServiceJni.get().getAuthenticatedAccountInfo(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isAuthenticatedAccountPrimary() {
        return ProfileSyncServiceJni.get().isAuthenticatedAccountPrimary(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public Set<Integer> getActiveDataTypes() {
        int[] activeDataTypes =
                ProfileSyncServiceJni.get().getActiveDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(activeDataTypes);
    }

    @Override
    public Set<Integer> getChosenDataTypes() {
        int[] modelTypeArray =
                ProfileSyncServiceJni.get().getChosenDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(modelTypeArray);
    }

    @Override
    public boolean hasKeepEverythingSynced() {
        return ProfileSyncServiceJni.get().hasKeepEverythingSynced(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setChosenDataTypes(boolean syncEverything, Set<Integer> enabledTypes) {
        ProfileSyncServiceJni.get().setChosenDataTypes(mNativeProfileSyncServiceAndroid,
                syncEverything,
                syncEverything ? ALL_SELECTABLE_TYPES : modelTypeSetToArray(enabledTypes));
    }

    @Override
    public void setFirstSetupComplete(int syncFirstSetupCompleteSource) {
        ProfileSyncServiceJni.get().setFirstSetupComplete(
                mNativeProfileSyncServiceAndroid, syncFirstSetupCompleteSource);
    }

    @Override
    public boolean isFirstSetupComplete() {
        return ProfileSyncServiceJni.get().isFirstSetupComplete(mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setSyncRequested(boolean requested) {
        ProfileSyncServiceJni.get().setSyncRequested(mNativeProfileSyncServiceAndroid, requested);
    }

    @Override
    public boolean isSyncRequested() {
        return ProfileSyncServiceJni.get().isSyncRequested(mNativeProfileSyncServiceAndroid);
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
        ProfileSyncServiceJni.get().setSetupInProgress(
                mNativeProfileSyncServiceAndroid, inProgress);
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
        return ProfileSyncServiceJni.get().isSyncAllowedByPlatform(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setSyncAllowedByPlatform(boolean allowed) {
        ProfileSyncServiceJni.get().setSyncAllowedByPlatform(
                mNativeProfileSyncServiceAndroid, allowed);
    }

    @Override
    public @PassphraseType int getPassphraseType() {
        assert isEngineInitialized();
        int passphraseType =
                ProfileSyncServiceJni.get().getPassphraseType(mNativeProfileSyncServiceAndroid);
        if (passphraseType < 0 || passphraseType > PassphraseType.MAX_VALUE) {
            throw new IllegalArgumentException();
        }
        return passphraseType;
    }

    @Override
    public @Nullable Date getExplicitPassphraseTime() {
        assert isEngineInitialized();
        long timeInMilliseconds = ProfileSyncServiceJni.get().getExplicitPassphraseTime(
                mNativeProfileSyncServiceAndroid);
        return timeInMilliseconds != 0 ? new Date(timeInMilliseconds) : null;
    }

    @Override
    public boolean isUsingExplicitPassphrase() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isUsingExplicitPassphrase(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isPassphraseRequiredForPreferredDataTypes() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isPassphraseRequiredForPreferredDataTypes(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isTrustedVaultKeyRequired() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isTrustedVaultKeyRequired(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isTrustedVaultKeyRequiredForPreferredDataTypes() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isTrustedVaultKeyRequiredForPreferredDataTypes(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isTrustedVaultRecoverabilityDegraded() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isTrustedVaultRecoverabilityDegraded(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isCustomPassphraseAllowed() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isCustomPassphraseAllowed(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public boolean isEncryptEverythingEnabled() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isEncryptEverythingEnabled(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void setEncryptionPassphrase(String passphrase) {
        assert isEngineInitialized();
        ProfileSyncServiceJni.get().setEncryptionPassphrase(
                mNativeProfileSyncServiceAndroid, passphrase);
    }

    @Override
    public boolean setDecryptionPassphrase(String passphrase) {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().setDecryptionPassphrase(
                mNativeProfileSyncServiceAndroid, passphrase);
    }

    @Override
    public boolean isPassphrasePromptMutedForCurrentProductVersion() {
        return ProfileSyncServiceJni.get().isPassphrasePromptMutedForCurrentProductVersion(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void markPassphrasePromptMutedForCurrentProductVersion() {
        ProfileSyncServiceJni.get().markPassphrasePromptMutedForCurrentProductVersion(
                mNativeProfileSyncServiceAndroid);
    }

    @Override
    public void recordKeyRetrievalTrigger(@KeyRetrievalTriggerForUMA int keyRetrievalTrigger) {
        ProfileSyncServiceJni.get().recordKeyRetrievalTrigger(
                mNativeProfileSyncServiceAndroid, keyRetrievalTrigger);
    }

    @Override
    public boolean shouldOfferTrustedVaultOptIn() {
        return ProfileSyncServiceJni.get().shouldOfferTrustedVaultOptIn(
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
        return ProfileSyncServiceJni.get().getLastSyncedTimeForDebugging(
                mNativeProfileSyncServiceAndroid);
    }

    @VisibleForTesting
    @Override
    public void triggerRefresh() {
        ProfileSyncServiceJni.get().triggerRefresh(mNativeProfileSyncServiceAndroid);
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
        ProfileSyncServiceJni.get().getAllNodes(mNativeProfileSyncServiceAndroid, callback);
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
        return ProfileSyncServiceJni.get().getNativeSyncServiceImplForTest(
                mNativeProfileSyncServiceAndroid);
    }

    @NativeMethods
    interface Natives {
        long init(ProfileSyncService caller);

        // Please keep all methods below in the same order as profile_sync_service_android.h.
        boolean isSyncRequested(long nativeProfileSyncServiceAndroid);
        void setSyncRequested(long nativeProfileSyncServiceAndroid, boolean requested);
        boolean canSyncFeatureStart(long nativeProfileSyncServiceAndroid);
        boolean isSyncAllowedByPlatform(long nativeProfileSyncServiceAndroid);
        void setSyncAllowedByPlatform(long nativeProfileSyncServiceAndroid, boolean allowed);
        boolean isSyncFeatureActive(long nativeProfileSyncServiceAndroid);
        boolean isSyncDisabledByEnterprisePolicy(long nativeProfileSyncServiceAndroid);
        boolean isEngineInitialized(long nativeProfileSyncServiceAndroid);
        boolean isTransportStateActive(long nativeProfileSyncServiceAndroid);
        void setSetupInProgress(long nativeProfileSyncServiceAndroid, boolean inProgress);
        boolean isFirstSetupComplete(long nativeProfileSyncServiceAndroid);
        void setFirstSetupComplete(
                long nativeProfileSyncServiceAndroid, int syncFirstSetupCompleteSource);
        int[] getActiveDataTypes(long nativeProfileSyncServiceAndroid);
        int[] getChosenDataTypes(long nativeProfileSyncServiceAndroid);
        void setChosenDataTypes(
                long nativeProfileSyncServiceAndroid, boolean syncEverything, int[] modelTypeArray);
        boolean isCustomPassphraseAllowed(long nativeProfileSyncServiceAndroid);
        boolean isEncryptEverythingEnabled(long nativeProfileSyncServiceAndroid);
        boolean isPassphraseRequiredForPreferredDataTypes(long nativeProfileSyncServiceAndroid);
        boolean isTrustedVaultKeyRequired(long nativeProfileSyncServiceAndroid);
        boolean isTrustedVaultKeyRequiredForPreferredDataTypes(
                long nativeProfileSyncServiceAndroid);
        boolean isTrustedVaultRecoverabilityDegraded(long nativeProfileSyncServiceAndroid);
        boolean isUsingExplicitPassphrase(long nativeProfileSyncServiceAndroid);
        int getPassphraseType(long nativeProfileSyncServiceAndroid);
        void setEncryptionPassphrase(long nativeProfileSyncServiceAndroid, String passphrase);
        boolean setDecryptionPassphrase(long nativeProfileSyncServiceAndroid, String passphrase);
        long getExplicitPassphraseTime(long nativeProfileSyncServiceAndroid);
        void getAllNodes(long nativeProfileSyncServiceAndroid, Callback<JSONArray> callback);
        int getAuthError(long nativeProfileSyncServiceAndroid);
        boolean hasUnrecoverableError(long nativeProfileSyncServiceAndroid);
        boolean requiresClientUpgrade(long nativeProfileSyncServiceAndroid);
        void setDecoupledFromAndroidMasterSync(long nativeProfileSyncServiceAndroid);
        boolean getDecoupledFromAndroidMasterSync(long nativeProfileSyncServiceAndroid);
        @Nullable
        CoreAccountInfo getAuthenticatedAccountInfo(long nativeProfileSyncServiceAndroid);
        boolean isAuthenticatedAccountPrimary(long nativeProfileSyncServiceAndroid);
        boolean isPassphrasePromptMutedForCurrentProductVersion(
                long nativeProfileSyncServiceAndroid);
        void markPassphrasePromptMutedForCurrentProductVersion(
                long nativeProfileSyncServiceAndroid);
        boolean hasKeepEverythingSynced(long nativeProfileSyncServiceAndroid);
        void recordKeyRetrievalTrigger(
                long nativeProfileSyncServiceAndroid, int keyRetrievalTrigger);
        boolean shouldOfferTrustedVaultOptIn(long nativeProfileSyncServiceAndroid);
        void triggerRefresh(long nativeProfileSyncServiceAndroid);
        long getLastSyncedTimeForDebugging(long nativeProfileSyncServiceAndroid);
        long getNativeSyncServiceImplForTest(long nativeProfileSyncServiceAndroid);
    }
}
