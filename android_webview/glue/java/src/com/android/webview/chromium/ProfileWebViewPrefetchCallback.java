// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package com.android.webview.chromium;

import android.os.Bundle;

import androidx.annotation.NonNull;

import org.chromium.android_webview.AwPrefetchCallback;
import org.chromium.android_webview.common.Lifetime;

import java.util.concurrent.Executor;

/** The primary callback for resolving AndroidX prefetch requests going through {@link Profile}. */
@Lifetime.Temporary
public class ProfileWebViewPrefetchCallback implements AwPrefetchCallback {

    @NonNull private final Executor mCallbackExecutor;
    @NonNull private final PrefetchOperationCallback mCallback;

    public ProfileWebViewPrefetchCallback(
            @NonNull Executor callbackExecutor, @NonNull PrefetchOperationCallback callback) {
        mCallbackExecutor = callbackExecutor;
        mCallback = callback;
    }

    @Override
    public void onStatusUpdated(int statusCode, Bundle extras) {
        PrefetchOperationResult operationResult =
                PrefetchOperationResult.fromPrefetchStatusCode(statusCode, extras);
        if (operationResult.statusCode == PrefetchOperationStatusCode.SUCCESS) {
            mCallbackExecutor.execute(mCallback::onSuccess);
        } else {
            resolvePrefetchErrorCallback(operationResult);
        }
    }

    @Override
    public void onError(Throwable e) {
        mCallback.onError(PrefetchOperationStatusCode.FAILURE, e.getMessage(), 0);
    }

    private void resolvePrefetchErrorCallback(PrefetchOperationResult operationResult) {
        assert operationResult.statusCode != PrefetchOperationStatusCode.SUCCESS;
        int errorCode = 0;
        String message;
        switch (operationResult.statusCode) {
            case PrefetchOperationStatusCode.FAILURE -> message = "Prefetch request failed";
            case PrefetchOperationStatusCode.SERVER_FAILURE -> {
                message = "Server error";
                errorCode = operationResult.httpResponseStatusCode;
            }
            case PrefetchOperationStatusCode.DUPLICATE_REQUEST -> message =
                    "Duplicate prefetch request";
            default -> message = "Unexpected error occurred.";
        }
        mCallback.onError(operationResult.statusCode, message, errorCode);
    }
}
