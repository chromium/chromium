// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.AnyThread;

import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Fake some SyncService methods for testing.
 *
 * Only what has been needed for tests so far has been faked.
 */
public class FakeSyncServiceImpl extends SyncServiceImpl {
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
        super();
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
            syncStateChanged();
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
            syncStateChanged();
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
            syncStateChanged();
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
            syncStateChanged();
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
            syncStateChanged();
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
            syncStateChanged();
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
            syncStateChanged();
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
            syncStateChanged();
        });
    }

    @AnyThread
    public void setEncryptEverythingEnabled(boolean encryptEverythingEnabled) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mEncryptEverythingEnabled = encryptEverythingEnabled; });
    }
}
