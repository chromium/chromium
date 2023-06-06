// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.annotation.SuppressLint;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.json.JSONArray;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.base.CoreAccountInfo;

import java.util.Date;
import java.util.Set;

/**
 * Java version of the native SyncService interface. Must only be used on the UI thread.
 * TODO(crbug.com/1158816): Document the remaining methods.
 */
// TODO(crbug.com/1380925): Delete this class once all clients migrate to the version in
//                          //components/sync.
public abstract class SyncService extends org.chromium.components.sync.SyncService {
    /**
     * DEPRECATED. Use {@link SyncServiceFactory#getForProfile(Profile)}
     */
    @Nullable
    @Deprecated
    public static SyncService get() {
        ThreadUtils.assertOnUiThread();
        if (SyncServiceFactory.sSyncServiceForTest != null) {
            return wrapIfNecessary(SyncServiceFactory.sSyncServiceForTest);
        }
        return wrapIfNecessary(SyncServiceFactory.get());
    }

    /**
     * Overrides the initialization for tests. The tests should call resetForTests() at shutdown.
     */
    @VisibleForTesting
    public static void overrideForTests(org.chromium.components.sync.SyncService syncService) {
        SyncServiceFactory.overrideForTests(syncService);
    }

    /**
     * Resets the SyncService instance. Calling get() next time will initialize with a new
     * instance.
     */
    @VisibleForTesting
    public static void resetForTests() {
        SyncServiceFactory.resetForTests();
    }

    private static SyncService wrapIfNecessary(org.chromium.components.sync.SyncService service) {
        if (service instanceof SyncService) return (SyncService) service;
        return new SyncServiceWrapper(service);
    }

    /** Wraps a SyncService instance and delegates all calls to it. */
    @SuppressLint("VisibleForTests")
    static class SyncServiceWrapper extends SyncService {
        protected final org.chromium.components.sync.SyncService mDelegate;

        protected SyncServiceWrapper(org.chromium.components.sync.SyncService delegateService) {
            mDelegate = delegateService;
        }

        @Override
        public boolean isTransportStateActive() {
            return mDelegate.isTransportStateActive();
        }

        @Override
        public boolean isEngineInitialized() {
            return mDelegate.isEngineInitialized();
        }

        @Override
        public boolean canSyncFeatureStart() {
            return mDelegate.canSyncFeatureStart();
        }

        @Override
        public boolean isSyncFeatureEnabled() {
            return mDelegate.isSyncFeatureEnabled();
        }

        @Override
        public boolean isSyncFeatureActive() {
            return mDelegate.isSyncFeatureActive();
        }

        @Override
        public int getAuthError() {
            return mDelegate.getAuthError();
        }

        @Override
        public boolean isSyncDisabledByEnterprisePolicy() {
            return mDelegate.isSyncDisabledByEnterprisePolicy();
        }

        @Override
        public boolean hasUnrecoverableError() {
            return mDelegate.hasUnrecoverableError();
        }

        @Override
        public boolean requiresClientUpgrade() {
            return mDelegate.requiresClientUpgrade();
        }

        @Nullable
        @Override
        public CoreAccountInfo getAccountInfo() {
            return mDelegate.getAccountInfo();
        }

        @Override
        public boolean hasSyncConsent() {
            return mDelegate.hasSyncConsent();
        }

        @Override
        public Set<Integer> getActiveDataTypes() {
            return mDelegate.getActiveDataTypes();
        }

        @Override
        public Set<Integer> getSelectedTypes() {
            return mDelegate.getSelectedTypes();
        }

        @Override
        public boolean hasKeepEverythingSynced() {
            return mDelegate.hasKeepEverythingSynced();
        }

        @Override
        public boolean isTypeManagedByPolicy(int type) {
            return mDelegate.isTypeManagedByPolicy(type);
        }

        @Override
        public void setSelectedTypes(boolean syncEverything, Set<Integer> enabledTypes) {
            mDelegate.setSelectedTypes(syncEverything, enabledTypes);
        }

        @Override
        public void setInitialSyncFeatureSetupComplete(int syncFirstSetupCompleteSource) {
            mDelegate.setInitialSyncFeatureSetupComplete(syncFirstSetupCompleteSource);
        }

        @Override
        public boolean isInitialSyncFeatureSetupComplete() {
            return mDelegate.isInitialSyncFeatureSetupComplete();
        }

        @Override
        public void setSyncRequested() {
            mDelegate.setSyncRequested();
        }

        @Override
        public SyncSetupInProgressHandle getSetupInProgressHandle() {
            return mDelegate.getSetupInProgressHandle();
        }

        @Override
        public void addSyncStateChangedListener(SyncStateChangedListener listener) {
            mDelegate.addSyncStateChangedListener(listener);
        }

        @Override
        public void removeSyncStateChangedListener(SyncStateChangedListener listener) {
            mDelegate.removeSyncStateChangedListener(listener);
        }

        @Override
        public int getPassphraseType() {
            return mDelegate.getPassphraseType();
        }

        @Nullable
        @Override
        public Date getExplicitPassphraseTime() {
            return mDelegate.getExplicitPassphraseTime();
        }

        @Override
        public boolean isUsingExplicitPassphrase() {
            return mDelegate.isUsingExplicitPassphrase();
        }

        @Override
        public boolean isPassphraseRequiredForPreferredDataTypes() {
            return mDelegate.isPassphraseRequiredForPreferredDataTypes();
        }

        @Override
        public boolean isTrustedVaultKeyRequired() {
            return mDelegate.isTrustedVaultKeyRequired();
        }

        @Override
        public boolean isTrustedVaultKeyRequiredForPreferredDataTypes() {
            return mDelegate.isTrustedVaultKeyRequiredForPreferredDataTypes();
        }

        @Override
        public boolean isTrustedVaultRecoverabilityDegraded() {
            return mDelegate.isTrustedVaultRecoverabilityDegraded();
        }

        @Override
        public boolean isCustomPassphraseAllowed() {
            return mDelegate.isCustomPassphraseAllowed();
        }

        @Override
        public boolean isEncryptEverythingEnabled() {
            return mDelegate.isEncryptEverythingEnabled();
        }

        @Override
        public void setEncryptionPassphrase(String passphrase) {
            mDelegate.setEncryptionPassphrase(passphrase);
        }

        @Override
        public boolean setDecryptionPassphrase(String passphrase) {
            return mDelegate.setDecryptionPassphrase(passphrase);
        }

        @Override
        public boolean isPassphrasePromptMutedForCurrentProductVersion() {
            return mDelegate.isPassphrasePromptMutedForCurrentProductVersion();
        }

        @Override
        public void markPassphrasePromptMutedForCurrentProductVersion() {
            mDelegate.markPassphrasePromptMutedForCurrentProductVersion();
        }

        @Override
        public boolean shouldOfferTrustedVaultOptIn() {
            return mDelegate.shouldOfferTrustedVaultOptIn();
        }

        @Override
        public boolean isSyncingUnencryptedUrls() {
            return mDelegate.isSyncingUnencryptedUrls();
        }

        @Override
        public long getLastSyncedTimeForDebugging() {
            return mDelegate.getLastSyncedTimeForDebugging();
        }

        @Override
        public void triggerRefresh() {
            mDelegate.triggerRefresh();
        }

        @Override
        public void getAllNodes(Callback<JSONArray> callback) {
            mDelegate.getAllNodes(callback);
        }
    }
}
