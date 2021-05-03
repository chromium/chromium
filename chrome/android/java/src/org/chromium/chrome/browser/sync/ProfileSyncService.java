// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;
import org.json.JSONException;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
 * UI thread. See components/sync/driver/profile_sync_service.h for more details.
 */
public class ProfileSyncService {

    /**
     * Listener for the underlying sync status.
     */
    public interface SyncStateChangedListener {
        // Invoked when the status has changed.
        public void syncStateChanged();
    }

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

    private static ProfileSyncService sProfileSyncService;
    private static boolean sInitialized;

    // Sync state changes more often than listeners are added/removed, so using CopyOnWrite.
    private final List<SyncStateChangedListener> mListeners =
            new CopyOnWriteArrayList<SyncStateChangedListener>();

    /**
     * Native ProfileSyncServiceAndroid object. Cannot be final because it is initialized in
     * {@link init()}.
     */
    private long mNativeProfileSyncServiceAndroid;

    private int mSetupInProgressCounter;

    /**
     * Retrieves or creates the ProfileSyncService singleton instance. Returns null if sync is
     * disabled (via flag or variation).
     *
     * Can only be accessed on the main thread.
     */
    @Nullable
    public static ProfileSyncService get() {
        ThreadUtils.assertOnUiThread();
        if (!sInitialized) {
            sProfileSyncService = new ProfileSyncService();
            if (sProfileSyncService.mNativeProfileSyncServiceAndroid == 0) {
                sProfileSyncService = null;
            }
            sInitialized = true;
        }
        return sProfileSyncService;
    }

    /**
     * Overrides the initialization for tests. The tests should call resetForTests() at shutdown.
     */
    @VisibleForTesting
    public static void overrideForTests(ProfileSyncService profileSyncService) {
        ThreadUtils.assertOnUiThread();
        sProfileSyncService = profileSyncService;
        sInitialized = true;
    }

    /**
     * Resets the ProfileSyncService instance. Calling get() next time will initialize with a new
     * instance.
     */
    @VisibleForTesting
    public static void resetForTests() {
        sInitialized = false;
        sProfileSyncService = null;
    }

    protected ProfileSyncService() {
        ThreadUtils.assertOnUiThread();

        // This may cause us to create ProfileSyncService even if sync has not
        // been set up, but ProfileSyncService won't actually start until
        // credentials are available.
        mNativeProfileSyncServiceAndroid =
                ProfileSyncServiceJni.get().init(ProfileSyncService.this);
    }

    /**
     * Checks if the sync engine is initialized. Note that this refers to
     * Sync-the-transport, i.e. it can be true even if the user has *not*
     * enabled Sync-the-feature.
     * This mostly needs to be checked as a precondition for the various
     * encryption-related methods (see below).
     *
     * @return true if the sync engine is initialized.
     */
    public boolean isEngineInitialized() {
        return ProfileSyncServiceJni.get().isEngineInitialized(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks whether sync machinery is active.
     *
     * @return true if the transport state is active.
     */
    @VisibleForTesting
    public boolean isTransportStateActive() {
        return ProfileSyncServiceJni.get().isTransportStateActive(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks whether Sync-the-feature can (attempt to) start. This means that there is a primary
     * account and no disable reasons. Note that the Sync machinery may start up in transport-only
     * mode even if this is false.
     *
     * @return true if Sync can start, false otherwise.
     */
    public boolean canSyncFeatureStart() {
        return ProfileSyncServiceJni.get().canSyncFeatureStart(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks whether Sync-the-feature is currently active. Note that Sync-the-transport may be
     * active even if this is false.
     *
     * @return true if Sync is active, false otherwise.
     */
    public boolean isSyncFeatureActive() {
        return ProfileSyncServiceJni.get().isSyncFeatureActive(mNativeProfileSyncServiceAndroid);
    }

    public @GoogleServiceAuthError.State int getAuthError() {
        int authErrorCode =
                ProfileSyncServiceJni.get().getAuthError(mNativeProfileSyncServiceAndroid);
        if (authErrorCode < 0 || authErrorCode >= GoogleServiceAuthError.State.NUM_ENTRIES) {
            throw new IllegalArgumentException("No state for code: " + authErrorCode);
        }
        return authErrorCode;
    }

    /**
     * Checks whether Sync is disabled by enterprise policy (through prefs) or account policy
     * received from the sync server.
     *
     * @return true if Sync is disabled, false otherwise.
     */
    public boolean isSyncDisabledByEnterprisePolicy() {
        return ProfileSyncServiceJni.get().isSyncDisabledByEnterprisePolicy(
                mNativeProfileSyncServiceAndroid);
    }

    public boolean hasUnrecoverableError() {
        return ProfileSyncServiceJni.get().hasUnrecoverableError(mNativeProfileSyncServiceAndroid);
    }

    public boolean requiresClientUpgrade() {
        return ProfileSyncServiceJni.get().requiresClientUpgrade(mNativeProfileSyncServiceAndroid);
    }

    public void setDecoupledFromAndroidMasterSync() {
        ProfileSyncServiceJni.get().setDecoupledFromAndroidMasterSync(
                mNativeProfileSyncServiceAndroid);
    }

    public boolean getDecoupledFromAndroidMasterSync() {
        return ProfileSyncServiceJni.get().getDecoupledFromAndroidMasterSync(
                mNativeProfileSyncServiceAndroid);
    }

    public @Nullable CoreAccountInfo getAuthenticatedAccountInfo() {
        return ProfileSyncServiceJni.get().getAuthenticatedAccountInfo(
                mNativeProfileSyncServiceAndroid);
    }

    public boolean isAuthenticatedAccountPrimary() {
        return ProfileSyncServiceJni.get().isAuthenticatedAccountPrimary(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * DEPRECATED: Use getChosenDataTypes() instead.
     *
     * Gets the set of data types that are "preferred" in sync. Those are the
     * chosen ones (see getChosenDataTypes), plus any that are implied by them.
     *
     * NOTE: This returns "all types" by default, even if the user has never
     *       enabled Sync, or if only Sync-the-transport is running.
     *
     * @return Set of preferred data types.
     */
    public Set<Integer> getPreferredDataTypes() {
        int[] modelTypeArray =
                ProfileSyncServiceJni.get().getPreferredDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(modelTypeArray);
    }

    /**
     * Gets the set of data types that are currently syncing.
     *
     * This is affected by whether sync is on.
     *
     * @return Set of active data types.
     */
    public Set<Integer> getActiveDataTypes() {
        int[] activeDataTypes =
                ProfileSyncServiceJni.get().getActiveDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(activeDataTypes);
    }

    /**
     * Gets the set of data types that the user has chosen to enable. This
     * corresponds to the native GetSelectedTypes() / UserSelectableTypeSet, but
     * every UserSelectableType is mapped to the corresponding canonical
     * ModelType.
     * TODO(crbug.com/985290): Expose UserSelectableType to Java and return that
     * instead.
     *
     * NOTE: This returns "all types" by default, even if the user has never
     *       enabled Sync, or if only Sync-the-transport is running.
     *
     * @return Set of chosen types.
     */
    public Set<Integer> getChosenDataTypes() {
        int[] modelTypeArray =
                ProfileSyncServiceJni.get().getChosenDataTypes(mNativeProfileSyncServiceAndroid);
        return modelTypeArrayToSet(modelTypeArray);
    }

    public boolean hasKeepEverythingSynced() {
        return ProfileSyncServiceJni.get().hasKeepEverythingSynced(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * Enables syncing for the passed data types.
     *
     * @param syncEverything Set to true if the user wants to sync all data types
     *                       (including new data types we add in the future).
     * @param enabledTypes   The set of types to enable. Ignored (can be null) if
     *                       syncEverything is true.
     */
    public void setChosenDataTypes(boolean syncEverything, Set<Integer> enabledTypes) {
        ProfileSyncServiceJni.get().setChosenDataTypes(mNativeProfileSyncServiceAndroid,
                syncEverything,
                syncEverything ? ALL_SELECTABLE_TYPES : modelTypeSetToArray(enabledTypes));
    }

    public void setFirstSetupComplete(int syncFirstSetupCompleteSource) {
        ProfileSyncServiceJni.get().setFirstSetupComplete(
                mNativeProfileSyncServiceAndroid, syncFirstSetupCompleteSource);
    }

    public boolean isFirstSetupComplete() {
        return ProfileSyncServiceJni.get().isFirstSetupComplete(mNativeProfileSyncServiceAndroid);
    }

    public void setSyncRequested(boolean requested) {
        ProfileSyncServiceJni.get().setSyncRequested(mNativeProfileSyncServiceAndroid, requested);
    }

    /**
     * Checks whether syncing is requested by the user, i.e. the user has at least started a Sync
     * setup flow, and has not disabled syncing in settings. Note that even if this is true, other
     * reasons might prevent Sync from actually starting up.
     *
     * @return true if the user wants to sync, false otherwise.
     */
    public boolean isSyncRequested() {
        return ProfileSyncServiceJni.get().isSyncRequested(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Instances of this class keep sync paused until {@link #close} is called. Use
     * {@link ProfileSyncService#getSetupInProgressHandle} to create. Please note that
     * {@link #close} should be called on every instance of this class.
     */
    public final class SyncSetupInProgressHandle {
        private boolean mClosed;

        private SyncSetupInProgressHandle() {
            ThreadUtils.assertOnUiThread();
            if (++mSetupInProgressCounter == 1) {
                setSetupInProgress(true);
            }
        }

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

    /**
     * Called by the UI to prevent changes in sync settings from taking effect while these settings
     * are being modified by the user. When sync settings UI is no longer visible,
     * {@link SyncSetupInProgressHandle#close} has to be invoked for sync settings to be applied.
     * Sync settings will remain paused as long as there are unclosed objects returned by this
     * method. Please note that the behavior of SyncSetupInProgressHandle is slightly different from
     * the equivalent C++ object, as Java instances don't commit sync settings as soon as any
     * instance of SyncSetupInProgressHandle is closed.
     */
    public SyncSetupInProgressHandle getSetupInProgressHandle() {
        return new SyncSetupInProgressHandle();
    }

    private void setSetupInProgress(boolean inProgress) {
        ProfileSyncServiceJni.get().setSetupInProgress(
                mNativeProfileSyncServiceAndroid, inProgress);
    }

    public void addSyncStateChangedListener(SyncStateChangedListener listener) {
        ThreadUtils.assertOnUiThread();
        mListeners.add(listener);
    }

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

    public boolean isSyncAllowedByPlatform() {
        return ProfileSyncServiceJni.get().isSyncAllowedByPlatform(
                mNativeProfileSyncServiceAndroid);
    }

    public void setSyncAllowedByPlatform(boolean allowed) {
        ProfileSyncServiceJni.get().setSyncAllowedByPlatform(
                mNativeProfileSyncServiceAndroid, allowed);
    }

    /**
     * Returns the actual passphrase type being used for encryption. The sync engine must be
     * running (isEngineInitialized() returns true) before calling this function.
     * <p/>
     * This method should only be used if you want to know the raw value. For checking whether
     * we should ask the user for a passphrase, use isPassphraseRequiredForPreferredDataTypes().
     */
    public @PassphraseType int getPassphraseType() {
        assert isEngineInitialized();
        int passphraseType =
                ProfileSyncServiceJni.get().getPassphraseType(mNativeProfileSyncServiceAndroid);
        if (passphraseType < 0 || passphraseType > PassphraseType.MAX_VALUE) {
            throw new IllegalArgumentException();
        }
        return passphraseType;
    }

    /**
     * Returns the time the current explicit passphrase was set (if any). Null if no explicit
     * passphrase is in use, or no time is available.
     */
    public @Nullable Date getExplicitPassphraseTime() {
        assert isEngineInitialized();
        long timeInMilliseconds = ProfileSyncServiceJni.get().getExplicitPassphraseTime(
                mNativeProfileSyncServiceAndroid);
        return timeInMilliseconds != 0 ? new Date(timeInMilliseconds) : null;
    }

    /**
     * Checks if sync is currently set to use a custom passphrase (or the similar -and legacy-
     * frozen implicit passphrase). The sync engine must be running (isEngineInitialized() returns
     * true) before calling this function.
     *
     * @return true if sync is using a custom passphrase.
     */
    public boolean isUsingExplicitPassphrase() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isUsingExplicitPassphrase(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if we need a passphrase to decrypt a currently-enabled data type. This returns false
     * if a passphrase is needed for a type that is not currently enabled.
     *
     * @return true if we need a passphrase.
     */
    public boolean isPassphraseRequiredForPreferredDataTypes() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isPassphraseRequiredForPreferredDataTypes(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if trusted vault encryption keys are needed, independently of the currently-enabled
     * data types.
     *
     * @return true if we need an encryption key.
     */
    public boolean isTrustedVaultKeyRequired() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isTrustedVaultKeyRequired(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if trusted vault encryption keys are needed to decrypt a currently-enabled data type.
     *
     * @return true if we need an encryption key for a type that is currently enabled.
     */
    public boolean isTrustedVaultKeyRequiredForPreferredDataTypes() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isTrustedVaultKeyRequiredForPreferredDataTypes(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * @return Whether setting a custom passphrase is allowed.
     */
    public boolean isCustomPassphraseAllowed() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isCustomPassphraseAllowed(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * Checks if the user has chosen to encrypt all data types. Note that some data types (e.g.
     * DEVICE_INFO) are never encrypted.
     *
     * @return true if all data types are encrypted, false if only passwords are encrypted.
     */
    public boolean isEncryptEverythingEnabled() {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().isEncryptEverythingEnabled(
                mNativeProfileSyncServiceAndroid);
    }

    public void setEncryptionPassphrase(String passphrase) {
        assert isEngineInitialized();
        ProfileSyncServiceJni.get().setEncryptionPassphrase(
                mNativeProfileSyncServiceAndroid, passphrase);
    }

    public boolean setDecryptionPassphrase(String passphrase) {
        assert isEngineInitialized();
        return ProfileSyncServiceJni.get().setDecryptionPassphrase(
                mNativeProfileSyncServiceAndroid, passphrase);
    }

    /**
     * Returns whether this client has previously prompted the user for a
     * passphrase error via the android system notifications for the current
     * product major version (i.e. gets reset upon browser upgrade). More
     * specifically, it returns whether the method
     * markPassphrasePromptMutedForCurrentProductVersion() has been invoked
     * before, since the last time the browser was upgraded to a new major
     * version.
     *
     * Can be called whether or not sync is initialized.
     *
     * @return Whether client has prompted for a passphrase error previously for
     * the current product major version.
     */
    public boolean isPassphrasePromptMutedForCurrentProductVersion() {
        return ProfileSyncServiceJni.get().isPassphrasePromptMutedForCurrentProductVersion(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * Mutes passphrase error via the android system notifications until the
     * browser is upgraded to a new major version.
     *
     * Can be called whether or not sync is initialized.
     */
    public void markPassphrasePromptMutedForCurrentProductVersion() {
        ProfileSyncServiceJni.get().markPassphrasePromptMutedForCurrentProductVersion(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * Records TrustedVaultKeyRetrievalTrigger histogram.
     */
    public void recordKeyRetrievalTrigger(@KeyRetrievalTriggerForUMA int keyRetrievalTrigger) {
        ProfileSyncServiceJni.get().recordKeyRetrievalTrigger(
                mNativeProfileSyncServiceAndroid, keyRetrievalTrigger);
    }

    /**
     * @return Whether sync is enabled to sync urls or open tabs with a non custom passphrase.
     */
    public boolean isSyncingUrlsWithKeystorePassphrase() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            return isEngineInitialized() && getActiveDataTypes().contains(ModelType.TYPED_URLS)
                    && (getPassphraseType() == PassphraseType.KEYSTORE_PASSPHRASE
                            || getPassphraseType() == PassphraseType.TRUSTED_VAULT_PASSPHRASE);
        }
        return isEngineInitialized() && getPreferredDataTypes().contains(ModelType.TYPED_URLS)
                && (getPassphraseType() == PassphraseType.KEYSTORE_PASSPHRASE
                        || getPassphraseType() == PassphraseType.TRUSTED_VAULT_PASSPHRASE);
    }

    @VisibleForTesting
    public long getNativeProfileSyncServiceForTest() {
        return ProfileSyncServiceJni.get().getProfileSyncServiceForTest(
                mNativeProfileSyncServiceAndroid);
    }

    /**
     * Returns the time when the last sync cycle was completed.
     *
     * @return The difference measured in microseconds, between last sync cycle completion time
     * and 1 January 1970 00:00:00 UTC.
     */
    @VisibleForTesting
    public long getLastSyncedTimeForTest() {
        return ProfileSyncServiceJni.get().getLastSyncedTimeForTest(
                mNativeProfileSyncServiceAndroid);
    }

    @VisibleForTesting
    public void triggerRefresh() {
        ProfileSyncServiceJni.get().triggerRefresh(mNativeProfileSyncServiceAndroid);
    }

    /**
     * Callback for getAllNodes.
     */
    public static class GetAllNodesCallback {
        private String mNodesString;

        // Invoked when getAllNodes completes.
        public void onResult(String nodesString) {
            mNodesString = nodesString;
        }

        // Returns the result of GetAllNodes as a JSONArray.
        @VisibleForTesting
        public JSONArray getNodesAsJsonArray() throws JSONException {
            return new JSONArray(mNodesString);
        }
    }

    /**
     * Invokes the onResult method of the callback from native code.
     */
    @CalledByNative
    private static void onGetAllNodesResult(GetAllNodesCallback callback, String nodes) {
        callback.onResult(nodes);
    }

    /**
     * Retrieves a JSON version of local Sync data via the native GetAllNodes method.
     * This method is asynchronous; the result will be sent to the callback.
     */
    @VisibleForTesting
    public void getAllNodes(GetAllNodesCallback callback) {
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

    @NativeMethods
    interface Natives {
        long init(ProfileSyncService caller);

        void setSyncRequested(long nativeProfileSyncServiceAndroid, boolean requested);
        boolean isSyncAllowedByPlatform(long nativeProfileSyncServiceAndroid);
        void setSyncAllowedByPlatform(long nativeProfileSyncServiceAndroid, boolean allowed);
        int getAuthError(long nativeProfileSyncServiceAndroid);
        boolean requiresClientUpgrade(long nativeProfileSyncServiceAndroid);
        void setDecoupledFromAndroidMasterSync(long nativeProfileSyncServiceAndroid);
        boolean getDecoupledFromAndroidMasterSync(long nativeProfileSyncServiceAndroid);
        @Nullable
        CoreAccountInfo getAuthenticatedAccountInfo(long nativeProfileSyncServiceAndroid);
        boolean isAuthenticatedAccountPrimary(long nativeProfileSyncServiceAndroid);
        boolean isEngineInitialized(long nativeProfileSyncServiceAndroid);
        boolean isCustomPassphraseAllowed(long nativeProfileSyncServiceAndroid);
        boolean isEncryptEverythingEnabled(long nativeProfileSyncServiceAndroid);
        boolean isTransportStateActive(long nativeProfileSyncServiceAndroid);
        boolean isPassphraseRequiredForPreferredDataTypes(long nativeProfileSyncServiceAndroid);
        boolean isTrustedVaultKeyRequired(long nativeProfileSyncServiceAndroid);
        boolean isTrustedVaultKeyRequiredForPreferredDataTypes(
                long nativeProfileSyncServiceAndroid);
        boolean isUsingExplicitPassphrase(long nativeProfileSyncServiceAndroid);
        boolean setDecryptionPassphrase(long nativeProfileSyncServiceAndroid, String passphrase);
        void setEncryptionPassphrase(long nativeProfileSyncServiceAndroid, String passphrase);
        int getPassphraseType(long nativeProfileSyncServiceAndroid);
        long getExplicitPassphraseTime(long nativeProfileSyncServiceAndroid);
        int[] getActiveDataTypes(long nativeProfileSyncServiceAndroid);
        int[] getChosenDataTypes(long nativeProfileSyncServiceAndroid);
        int[] getPreferredDataTypes(long nativeProfileSyncServiceAndroid);
        void setChosenDataTypes(
                long nativeProfileSyncServiceAndroid, boolean syncEverything, int[] modelTypeArray);
        void triggerRefresh(long nativeProfileSyncServiceAndroid);
        void setSetupInProgress(long nativeProfileSyncServiceAndroid, boolean inProgress);
        void setFirstSetupComplete(
                long nativeProfileSyncServiceAndroid, int syncFirstSetupCompleteSource);
        boolean isFirstSetupComplete(long nativeProfileSyncServiceAndroid);
        boolean isSyncRequested(long nativeProfileSyncServiceAndroid);
        boolean canSyncFeatureStart(long nativeProfileSyncServiceAndroid);
        boolean isSyncFeatureActive(long nativeProfileSyncServiceAndroid);
        boolean isSyncDisabledByEnterprisePolicy(long nativeProfileSyncServiceAndroid);
        boolean hasKeepEverythingSynced(long nativeProfileSyncServiceAndroid);
        boolean hasUnrecoverableError(long nativeProfileSyncServiceAndroid);
        boolean isPassphrasePromptMutedForCurrentProductVersion(
                long nativeProfileSyncServiceAndroid);
        void markPassphrasePromptMutedForCurrentProductVersion(
                long nativeProfileSyncServiceAndroid);
        long getProfileSyncServiceForTest(long nativeProfileSyncServiceAndroid);
        long getLastSyncedTimeForTest(long nativeProfileSyncServiceAndroid);
        void getAllNodes(long nativeProfileSyncServiceAndroid, GetAllNodesCallback callback);
        void recordKeyRetrievalTrigger(
                long nativeProfileSyncServiceAndroid, int keyRetrievalTrigger);
    }
}
