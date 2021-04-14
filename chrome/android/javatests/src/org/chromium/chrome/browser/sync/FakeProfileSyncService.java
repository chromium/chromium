// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import androidx.annotation.AnyThread;

import org.chromium.base.ThreadUtils;
import org.chromium.components.signin.base.GoogleServiceAuthError;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.HashSet;
import java.util.Set;

/**
 * Fake some ProfileSyncService methods for testing.
 *
 * Only what has been needed for tests so far has been faked.
 */
public class FakeProfileSyncService extends ProfileSyncService {
    private boolean mEngineInitialized;
    private boolean mPassphraseRequiredForPreferredDataTypes;
    private boolean mTrustedVaultKeyRequired;
    private boolean mTrustedVaultKeyRequiredForPreferredDataTypes;
    private boolean mEncryptEverythingEnabled;
    private boolean mRequiresClientUpgrade;
    private Set<Integer> mChosenTypes = new HashSet<>();
    private boolean mCanSyncFeatureStart;
    @GoogleServiceAuthError.State
    private int mAuthError;

    public FakeProfileSyncService() {
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
    public boolean isUsingSecondaryPassphrase() {
        return true;
    }

    @Override
    public void setChosenDataTypes(boolean syncEverything, Set<Integer> enabledTypes) {
        mChosenTypes = enabledTypes;
    }

    @Override
    public Set<Integer> getPreferredDataTypes() {
        return mChosenTypes;
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
        return mTrustedVaultKeyRequired;
    }

    public void setTrustedVaultKeyRequired(boolean trustedVaultKeyRequired) {
        mTrustedVaultKeyRequired = trustedVaultKeyRequired;
    }

    @Override
    public boolean isTrustedVaultKeyRequiredForPreferredDataTypes() {
        return mTrustedVaultKeyRequiredForPreferredDataTypes;
    }

    public void setTrustedVaultKeyRequiredForPreferredDataTypes(
            boolean trustedVaultKeyRequiredForPreferredDataTypes) {
        mTrustedVaultKeyRequiredForPreferredDataTypes =
                trustedVaultKeyRequiredForPreferredDataTypes;
    }

    @Override
    public boolean isEncryptEverythingEnabled() {
        return mEncryptEverythingEnabled;
    }

    @Override
    public boolean canSyncFeatureStart() {
        return mCanSyncFeatureStart;
    }

    public void setCanSyncFeatureStart(boolean canSyncFeatureStart) {
        mCanSyncFeatureStart = canSyncFeatureStart;
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

    public void setEncryptEverythingEnabled(boolean encryptEverythingEnabled) {
        mEncryptEverythingEnabled = encryptEverythingEnabled;
    }
}
