// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import com.google.android.gms.common.api.ApiException;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

/**
 * Java-counterpart of the native PasswordSyncControllerDelegateBridgeImpl. It's part of
 * PasswordSyncControllerDelegate that propagates sync events to a downstream implementation.
 */
class PasswordSyncControllerDelegateBridgeImpl {
    private final PasswordSyncControllerDelegate mPasswordSyncControllerDelegate;
    private long mNativeDelegateBridgeImpl;

    PasswordSyncControllerDelegateBridgeImpl(
            long nativePasswordSyncControllerDelegateBridgeImpl,
            PasswordSyncControllerDelegate syncDelegate) {
        mNativeDelegateBridgeImpl = nativePasswordSyncControllerDelegateBridgeImpl;
        assert syncDelegate != null;
        mPasswordSyncControllerDelegate = syncDelegate;
    }

    @CalledByNative
    static PasswordSyncControllerDelegateBridgeImpl create(
            long nativePasswordSyncControllerDelegateBridgeImpl) {
        PasswordSyncControllerDelegateFactory factory =
                PasswordSyncControllerDelegateFactory.getInstance();
        return new PasswordSyncControllerDelegateBridgeImpl(
                nativePasswordSyncControllerDelegateBridgeImpl, factory.createDelegate());
    }

    /** Notifies credential manager of the currently syncing account. */
    @CalledByNative
    void notifyCredentialManagerWhenSyncing(String accountEmail) {
        mPasswordSyncControllerDelegate.notifyCredentialManagerWhenSyncing(
                accountEmail,
                () -> {
                    if (mNativeDelegateBridgeImpl == 0) return;
                    PasswordSyncControllerDelegateBridgeImplJni.get()
                            .onCredentialManagerNotified(mNativeDelegateBridgeImpl);
                },
                exception -> handleCredentialManagerException(exception));
    }

    /** Notifies credential manager of a local account, when not syncing. */
    @CalledByNative
    void notifyCredentialManagerWhenNotSyncing() {
        mPasswordSyncControllerDelegate.notifyCredentialManagerWhenNotSyncing(
                () -> {
                    if (mNativeDelegateBridgeImpl == 0) return;
                    PasswordSyncControllerDelegateBridgeImplJni.get()
                            .onCredentialManagerNotified(mNativeDelegateBridgeImpl);
                },
                exception -> handleCredentialManagerException(exception));
    }

    /**
     * Handles exceptions occurring in downstream implementation.
     *
     * @param exception PasswordStoreAndroidBackend or API exception.
     */
    private void handleCredentialManagerException(Exception exception) {
        if (mNativeDelegateBridgeImpl == 0) return;

        @AndroidBackendErrorType int error = AndroidBackendErrorType.UNCATEGORIZED;
        int apiErrorCode = 0; // '0' means SUCCESS.

        if (exception instanceof PasswordStoreAndroidBackend.BackendException) {
            error = ((PasswordStoreAndroidBackend.BackendException) exception).errorCode;
        }

        if (exception instanceof ApiException) {
            error = AndroidBackendErrorType.EXTERNAL_ERROR;
            apiErrorCode = ((ApiException) exception).getStatusCode();
        }

        PasswordSyncControllerDelegateBridgeImplJni.get()
                .onCredentialManagerError(mNativeDelegateBridgeImpl, error, apiErrorCode);
    }

    /** C++ method signatures. */
    @NativeMethods
    interface Natives {
        void onCredentialManagerNotified(long nativePasswordSyncControllerDelegateBridgeImpl);

        void onCredentialManagerError(
                long nativePasswordSyncControllerDelegateBridgeImpl,
                int errorType,
                int apiErrorCode);
    }
}
