// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import static org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge.usesSplitStoresAndUPMForLocal;

import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerHelper;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;

import java.lang.ref.WeakReference;
import java.util.concurrent.CompletableFuture;

/**
 * The implementation of {@link PasswordCheckController} which calls the Gms core API to perform the
 * password check and get breached credentials number.
 */
class GmsCorePasswordCheckController
        implements PasswordCheckController, PasswordStoreBridge.PasswordStoreObserver {
    private final SyncService mSyncService;
    private final PrefService mPrefService;
    private final PasswordStoreBridge mPasswordStoreBridge;
    private final PasswordManagerHelper mPasswordManagerHelper;
    private final CompletableFuture<Integer> mPasswordsCountAccountStorage;
    private final CompletableFuture<Integer> mPasswordsCountLocalStorage;

    GmsCorePasswordCheckController(
            SyncService syncService,
            PrefService prefService,
            PasswordStoreBridge passwordStoreBridge,
            PasswordManagerHelper passwordManagerHelper) {
        mSyncService = syncService;
        mPrefService = prefService;
        mPasswordStoreBridge = passwordStoreBridge;
        mPasswordManagerHelper = passwordManagerHelper;
        mPasswordsCountAccountStorage = new CompletableFuture<>();
        mPasswordsCountLocalStorage = new CompletableFuture<>();
        mPasswordStoreBridge.addObserver(this, true);
    }

    @Override
    public CompletableFuture<PasswordCheckResult> checkPasswords(
            @PasswordStorageType int passwordStorageType) {
        WeakReference<GmsCorePasswordCheckController> weakRef = new WeakReference(this);
        CompletableFuture<PasswordCheckResult> passwordCheckResult = new CompletableFuture<>();
        mPasswordManagerHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                PasswordCheckController.getAccountNameForPasswordStorageType(
                        passwordStorageType, mSyncService),
                unused -> {
                    GmsCorePasswordCheckController controller = weakRef.get();
                    if (controller == null) return;

                    controller.getBreachedCredentialsCount(
                            passwordStorageType, passwordCheckResult);
                },
                error -> {
                    GmsCorePasswordCheckController controller = weakRef.get();
                    if (controller == null) return;

                    controller.onPasswordCheckFailed(error, passwordCheckResult);
                });
        return passwordCheckResult;
    }

    @Override
    public CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            @PasswordStorageType int passwordStorageType) {
        return getBreachedCredentialsCount(passwordStorageType, new CompletableFuture<>());
    }

    private CompletableFuture<PasswordCheckResult> getBreachedCredentialsCount(
            @PasswordStorageType int passwordStorageType,
            CompletableFuture<PasswordCheckResult> passwordCheckResult) {
        WeakReference<GmsCorePasswordCheckController> weakRef = new WeakReference(this);
        mPasswordManagerHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                PasswordCheckController.getAccountNameForPasswordStorageType(
                        passwordStorageType, mSyncService),
                count -> {
                    GmsCorePasswordCheckController controller = weakRef.get();
                    if (controller == null) return;

                    controller.onBreachedCredentialsObtained(
                            passwordStorageType, count, passwordCheckResult);
                },
                error -> {
                    GmsCorePasswordCheckController controller = weakRef.get();
                    if (controller == null) return;
                    controller.onPasswordCheckFailed(error, passwordCheckResult);
                });
        return passwordCheckResult;
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
    private void onBreachedCredentialsObtained(
            @PasswordStorageType int passwordStorageType,
            int breachedCount,
            CompletableFuture<PasswordCheckResult> passwordCheckResult) {
        CompletableFuture<Integer> passwordsTotalCount =
                passwordStorageType == PasswordStorageType.ACCOUNT_STORAGE
                        ? mPasswordsCountAccountStorage
                        : mPasswordsCountLocalStorage;
        // If passwordsTotalCount is already completed, the code after `thenAccept` is executed
        // immediately.
        passwordsTotalCount.thenAccept(
                totalCount ->
                        passwordCheckResult.complete(
                                new PasswordCheckResult(totalCount, breachedCount)));
    }

    private void onPasswordCheckFailed(
            Exception error, CompletableFuture<PasswordCheckResult> passwordCheckResult) {
        passwordCheckResult.complete(new PasswordCheckResult(error));
    }

    @Override
    public void onSavedPasswordsChanged(int count) {
        // The count here is the total count for both account and local storage. If this method is
        // called, this means that the passwords for both account and profile stores have been
        // fetched and can be requested.
        mPasswordStoreBridge.removeObserver(this);

        // If using split stores and UPM for local passwords is enabled, the local passwords are
        // stored in the profile store.
        if (usesSplitStoresAndUPMForLocal(mPrefService)) {
            mPasswordsCountAccountStorage.complete(
                    mPasswordStoreBridge.getPasswordStoreCredentialsCountForAccountStore());
            mPasswordsCountLocalStorage.complete(
                    mPasswordStoreBridge.getPasswordStoreCredentialsCountForProfileStore());
            return;
        }
        // If using split stores is disabled, all passwords reside in the profile store.
        mPasswordsCountAccountStorage.complete(
                mPasswordStoreBridge.getPasswordStoreCredentialsCountForProfileStore());
        mPasswordsCountLocalStorage.complete(0);
    }

    /** Not relevant for this controller. */
    @Override
    public void onEdit(PasswordStoreCredential credential) {}
}
