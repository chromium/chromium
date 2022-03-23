// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerHelper.usesUnifiedPasswordManagerUI;

import android.app.PendingIntent;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.ResolvableApiException;

import org.chromium.base.Log;

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

    static int getApiErrorCode(Exception exception) {
        if (exception instanceof ApiException) {
            return ((ApiException) exception).getStatusCode();
        }
        return 0; // '0' means SUCCESS.
    }

    static void handleResolvableApiException(ResolvableApiException exception) {
        if (!usesUnifiedPasswordManagerUI()) return;

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