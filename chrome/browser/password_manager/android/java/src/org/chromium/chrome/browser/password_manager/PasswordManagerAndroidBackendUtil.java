// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import android.app.PendingIntent;

import androidx.annotation.Nullable;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.ResolvableApiException;

import org.chromium.base.Log;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;

/**
 * Collection of utilities used by classes interacting with the password manager backend
 * in Google Mobile Services.
 */
class PasswordManagerAndroidBackendUtil {
    private static final String TAG = "PwdManagerBackend";

    private PasswordManagerAndroidBackendUtil() {}

    static @AndroidBackendErrorType int getBackendError(Exception exception) {
        if (exception instanceof PasswordStoreAndroidBackend.BackendException) {
            return ((PasswordStoreAndroidBackend.BackendException) exception).errorCode;
        }
        if (exception instanceof ApiException) {
            return AndroidBackendErrorType.EXTERNAL_ERROR;
        }
        return AndroidBackendErrorType.UNCATEGORIZED;
    }

    static @CredentialManagerError int getPasswordCheckupBackendError(Exception exception) {
        if (exception instanceof PasswordCheckupClientHelper.PasswordCheckBackendException) {
            return ((PasswordCheckupClientHelper.PasswordCheckBackendException) exception)
                    .errorCode;
        }
        if (exception instanceof ApiException) {
            return CredentialManagerError.API_EXCEPTION;
        }
        return CredentialManagerError.OTHER_API_ERROR;
    }

    static int getApiErrorCode(Exception exception) {
        if (exception instanceof ApiException) {
            return ((ApiException) exception).getStatusCode();
        }
        return 0; // '0' means SUCCESS.
    }

    static @Nullable Integer getConnectionResultCode(Exception exception) {
        if (!(exception instanceof ApiException)) return null;

        ConnectionResult connectionResult =
                ((ApiException) exception).getStatus().getConnectionResult();
        if (connectionResult == null) return null;

        return connectionResult.getErrorCode();
    }

    static void handleResolvableApiException(ResolvableApiException exception) {
        // No special resolution for the authentication errors is needed since the user has already
        // been prompted to reauthenticate by Google services and Sync in Chrome.
        if (exception.getStatusCode() == ChromeSyncStatusCode.AUTH_ERROR_RESOLVABLE) return;

        // For all other resolvable errors, an intent is launched allowing the user to fix the
        // broken state.
        PendingIntent pendingIntent = exception.getResolution();
        try {
            pendingIntent.send();
        } catch (PendingIntent.CanceledException e) {
            Log.e(TAG, "Can not launch error resolution intent", e);
        }
    }
}
