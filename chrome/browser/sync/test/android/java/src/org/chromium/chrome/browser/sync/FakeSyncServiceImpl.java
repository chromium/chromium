// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.AnyThread;
import androidx.annotation.Nullable;

import org.json.JSONArray;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncServiceImpl;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Date;
import java.util.Set;

/**
 * Fake some SyncService methods for testing.
 *
 * Only what has been needed for tests so far has been faked.
 */
public class FakeSyncServiceImpl implements SyncService {
    private final SyncService mDelegate;

    private boolean mEngineInitialized;
    private boolean mPassphraseRequiredForPreferredDataTypes;
    private boolean mTrustedVaultKeyRequired;
    private boolean mTrustedVaultKeyRequiredForPreferredDataTypes;
    private boolean mTrustedVaultRecoverabilityDegraded;
    private boolean mEncryptEverythingEnabled;
    private boolean mRequiresClientUpgrade;
    private boolean mCanSyncFeatureStart;
    @GoogleServiceAuthError.State
    private int mAuthError;

    public FakeSyncServiceImpl() {
        mDelegate = SyncServiceFactory.getForProfile(Profile.getLastUsedRegularProfile());
    }

    @Override
    public boolean isEngineInitialized() {
        ThreadUtils.assertOnUiThread();
        return mEngineInitialized;
    }

    @AnyThread
    public void setEngineInitialized(boolean engineInitialized) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mEngineInitialized = engineInitialized;
            notifySyncStateChanged();
        });
    }

    @Override
    public @GoogleServiceAuthError.State int getAuthError() {
        ThreadUtils.assertOnUiThread();
        return mAuthError;
    }

    @AnyThread
    public void setAuthError(@GoogleServiceAuthError.State int authError) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAuthError = authError;
            notifySyncStateChanged();
        });
    }

    @Override
    public boolean isUsingExplicitPassphrase() {
        ThreadUtils.assertOnUiThread();
        return true;
    }

    @Override
    public boolean isPassphraseRequiredForPreferredDataTypes() {
        ThreadUtils.assertOnUiThread();
        return mPassphraseRequiredForPreferredDataTypes;
    }

    @AnyThread
    public void setPassphraseRequiredForPreferredDataTypes(
            boolean passphraseRequiredForPreferredDataTypes) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPassphraseRequiredForPreferredDataTypes = passphraseRequiredForPreferredDataTypes;
            notifySyncStateChanged();
        });
    }

    @Override
    public boolean isTrustedVaultKeyRequired() {
        ThreadUtils.assertOnUiThread();
        return mTrustedVaultKeyRequired;
    }

    @AnyThread
    public void setTrustedVaultKeyRequired(boolean trustedVaultKeyRequired) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTrustedVaultKeyRequired = trustedVaultKeyRequired;
            notifySyncStateChanged();
        });
    }

    @Override
    public boolean isTrustedVaultKeyRequiredForPreferredDataTypes() {
        ThreadUtils.assertOnUiThread();
        return mTrustedVaultKeyRequiredForPreferredDataTypes;
    }

    @AnyThread
    public void setTrustedVaultKeyRequiredForPreferredDataTypes(
            boolean trustedVaultKeyRequiredForPreferredDataTypes) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTrustedVaultKeyRequiredForPreferredDataTypes =
                    trustedVaultKeyRequiredForPreferredDataTypes;
            notifySyncStateChanged();
        });
    }

    @Override
    public boolean isTrustedVaultRecoverabilityDegraded() {
        ThreadUtils.assertOnUiThread();
        return mTrustedVaultRecoverabilityDegraded;
    }

    @AnyThread
    public void setTrustedVaultRecoverabilityDegraded(boolean recoverabilityDegraded) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTrustedVaultRecoverabilityDegraded = recoverabilityDegraded;
            notifySyncStateChanged();
        });
    }

    @Override
    public boolean isEncryptEverythingEnabled() {
        ThreadUtils.assertOnUiThread();
        return mEncryptEverythingEnabled;
    }

    @Override
    public boolean canSyncFeatureStart() {
        ThreadUtils.assertOnUiThread();
        return mCanSyncFeatureStart;
    }

    @AnyThread
    public void setCanSyncFeatureStart(boolean canSyncFeatureStart) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mCanSyncFeatureStart = canSyncFeatureStart;
            notifySyncStateChanged();
        });
    }

    @Override
    public boolean requiresClientUpgrade() {
        ThreadUtils.assertOnUiThread();
        return mRequiresClientUpgrade;
    }

    @AnyThread
    public void setRequiresClientUpgrade(boolean requiresClientUpgrade) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mRequiresClientUpgrade = requiresClientUpgrade;
            notifySyncStateChanged();
        });
    }

    @AnyThread
    public void setEncryptEverythingEnabled(boolean encryptEverythingEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mEncryptEverythingEnabled = encryptEverythingEnabled; });
    }

    private void notifySyncStateChanged() {
        ((SyncServiceImpl) mDelegate).syncStateChanged();
    }

    @Override
    public boolean isTransportStateActive() {
        return mDelegate.isTransportStateActive();
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
    public boolean isSyncDisabledByEnterprisePolicy() {
        return mDelegate.isSyncDisabledByEnterprisePolicy();
    }

    @Override
    public boolean hasUnrecoverableError() {
        return mDelegate.hasUnrecoverableError();
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
    public boolean isTypeManagedByCustodian(int type) {
        return mDelegate.isTypeManagedByCustodian(type);
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
    public boolean isCustomPassphraseAllowed() {
        return mDelegate.isCustomPassphraseAllowed();
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
