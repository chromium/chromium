// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;

import java.lang.ref.WeakReference;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;

/**
 * The implementation of {@link PasswordCheckController} which calls the Gms core API to perform the
 * password check and get breached credentials number.
 */
class GmsCorePasswordCheckController
        implements PasswordCheckController, PasswordStoreBridge.PasswordStoreObserver {
    private SyncService mSyncService;
    private PasswordStoreBridge mPasswordStoreBridge;
    private CompletableFuture<Integer> mPasswordsTotalCount;
    private CompletableFuture<PasswordCheckResult> mPasswordCheckResult;

    public GmsCorePasswordCheckController(
            SyncService syncService, PasswordStoreBridge passwordStoreBridge) {
        mSyncService = syncService;
        mPasswordStoreBridge = passwordStoreBridge;
        // TODO(b/312930046): Is it enough to fetch passwords only once here or is it needed to be
        // done on each checkPasswords() call?
        mPasswordsTotalCount = new CompletableFuture<>();
        mPasswordStoreBridge.addObserver(this, true);
    }

    @Override
    public CompletableFuture<PasswordCheckResult> checkPasswords(
            @PasswordStorageType int passwordStorageType) {
        WeakReference<GmsCorePasswordCheckController> weakRef = new WeakReference(this);
        mPasswordCheckResult = new CompletableFuture<>();
        PasswordManagerHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                getAccountNameForStorageType(passwordStorageType),
                unused -> {
                    GmsCorePasswordCheckController controller = weakRef.get();
                    if (controller == null) return;

                    controller.getBreachedCredentialsCount(passwordStorageType);
                },
                error -> {
                    GmsCorePasswordCheckController controller = weakRef.get();
                    if (controller == null) return;

                    controller.onPasswordCheckFailed(error);
                });
        return mPasswordCheckResult;
    }

    @Override
    public CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            @PasswordStorageType int passwordStorageType) {
        WeakReference<GmsCorePasswordCheckController> weakRef = new WeakReference(this);
        if (mPasswordCheckResult == null || mPasswordCheckResult.isDone()) {
            mPasswordCheckResult = new CompletableFuture<>();
        }

        PasswordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                getAccountNameForStorageType(passwordStorageType),
                count -> {
                    GmsCorePasswordCheckController controller = weakRef.get();
                    if (controller == null) return;

                    controller.onBreachedCredentialsObtained(count);
                },
                error -> {
                    GmsCorePasswordCheckController controller = weakRef.get();
                    if (controller == null) return;
                    controller.onPasswordCheckFailed(error);
                });
        return mPasswordCheckResult;
    }

    @Override
    public void destroy() {
        mPasswordStoreBridge.removeObserver(this);
        mPasswordStoreBridge.destroy();
    }

    /**
     * Combines the result of the password check and passwords loading from the store and provides
     * the password check result.
     *
     * @param breachedCount the number of breached credentials
     */
    private void onBreachedCredentialsObtained(int breachedCount) {
        mPasswordsTotalCount.thenAccept(
                totalCount ->
                        mPasswordCheckResult.complete(
                                new PasswordCheckResult(totalCount, breachedCount)));
    }

    private void onPasswordCheckFailed(Exception error) {
        mPasswordCheckResult.complete(new PasswordCheckResult(error));
    }

    private Optional<String> getAccountNameForStorageType(
            @PasswordStorageType int passwordStoreType) {
        switch (passwordStoreType) {
            case PasswordStorageType.LOCAL_STORAGE:
                return Optional.empty();
            case PasswordStorageType.ACCOUNT_STORAGE:
                return Optional.of(CoreAccountInfo.getEmailFrom(mSyncService.getAccountInfo()));
        }
        assert false : "Unknown PasswordStorageType: " + passwordStoreType;
        return null;
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        // TODO(b/312930046): Password store bridge needs to be changed to return per-store password
        // count. Right now it returns the total count from all password stores combined.
        mPasswordStoreBridge.removeObserver(this);
        mPasswordsTotalCount.complete(count);
    }

    /** Not relevant for this controller. */
    @Override
    public void onEdit(PasswordStoreCredential credential) {}
}
