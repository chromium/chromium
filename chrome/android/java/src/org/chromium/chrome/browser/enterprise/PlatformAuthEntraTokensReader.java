// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorDescription;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.Context;
import android.os.Bundle;

import com.google.common.base.Preconditions;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback2;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.enterprise.platform_auth.entra_provider_android.TokenReadResult;

import java.io.IOException;
import java.util.concurrent.TimeUnit;

@NullMarked
@JNINamespace("enterprise_auth")
public class PlatformAuthEntraTokensReader {
    private static final String AUTH_TOKEN_TYPE = "sso_header";
    private static final String BROKER_ACCOUNT_TYPE = "com.microsoft.entra";
    private static final String BUNDLE_RESULT_KEY = "sso_header_result";
    private static final long TIMEOUT_SECONDS = 10;

    private static @Nullable ResultOverride sReadTokensOverride;

    /**
     * Reads authentication tokens from the Entra broker via the Android {@link AccountManager}.
     *
     * <p>This method requests an SSO auth token for the provided URL. Because it relies on {@link
     * AccountManagerFuture#getResult()}, <b>this is a blocking call</b>. The native C++ caller must
     * ensure it is executed on a background thread pool to prevent blocking the UI thread.
     *
     * @param url The target URL for which the authentication tokens are requested.
     * @param callback A callback invoked with the result once the operation completes. The first
     *     argument is a {@link TokenReadResult} status code, and the second is the raw JSON string
     *     containing headers (on success) or an error message/empty string (on failure).
     */
    @CalledByNative
    public static void readTokens(
            @JniType("std::string") String url,
            @JniType("base::OnceCallback<void(int, std::string)>&&")
                    JniOnceCallback2<Integer, String> callback) {
        if (sReadTokensOverride != null) {
            callback.onResult(sReadTokensOverride.resultCode, sReadTokensOverride.result);
            return;
        }

        // This function makes a blocking call and should never be run on the UI thread.
        ThreadUtils.assertOnBackgroundThread();

        try {
            final Context context = ContextUtils.getApplicationContext();
            final AccountManager accountManager = AccountManager.get(context);

            final String providerPackageName = getProviderPackageName(accountManager);
            if (providerPackageName == null) {
                callback.onResult(TokenReadResult.NO_BROKER_REGISTERED, "");
                return;
            }

            // TODO: b:484014627 - verify signature of the provider package.

            Bundle parameters = new Bundle();
            parameters.putString("url", url);
            // The account name does not matter here, it can be any non-empty string.
            final Account fakeAccount = new Account("fake_account", BROKER_ACCOUNT_TYPE);
            AccountManagerFuture<Bundle> future =
                    accountManager.getAuthToken(
                            fakeAccount,
                            AUTH_TOKEN_TYPE,
                            parameters,
                            /* notifyAuthFailure= */ false,
                            /* callback= */ null,
                            /* handler= */ null);

            // This is a blocking call. Throws an exception on timeout.
            Bundle bundleResult = future.getResult(TIMEOUT_SECONDS, TimeUnit.SECONDS);

            if (bundleResult == null) {
                callback.onResult(TokenReadResult.INVALID_BUNDLE_FORMAT, "null bundle result");
                return;
            }

            String stringResult = bundleResult.getString(BUNDLE_RESULT_KEY);
            if (stringResult == null) {
                callback.onResult(TokenReadResult.INVALID_BUNDLE_FORMAT, "null headers entry");
                return;
            }

            callback.onResult(TokenReadResult.OK, stringResult);
        } catch (AuthenticatorException | OperationCanceledException | IOException e) {
            callback.onResult(TokenReadResult.UNEXPECTED_ERROR, e.toString());
        }
    }

    private static @Nullable String getProviderPackageName(AccountManager accountManager) {
        final AuthenticatorDescription[] authenticators = accountManager.getAuthenticatorTypes();
        String providerPackageName = null;
        for (AuthenticatorDescription authenticator : authenticators) {
            if (BROKER_ACCOUNT_TYPE.equals(authenticator.type)) {
                providerPackageName = authenticator.packageName;
                break;
            }
        }
        return providerPackageName;
    }

    private static class ResultOverride {
        ResultOverride(Integer resultCode, String result) {
            this.resultCode = resultCode;
            this.result = result;
        }

        public final Integer resultCode;
        public final String result;
    }

    @CalledByNativeForTesting
    public static void setResultOverrideForTesting( // IN-TEST
            @JniType("int") Integer resultCode, @JniType("std::string") String result) {
        Preconditions.checkState(
                PlatformAuthEntraTokensReader.sReadTokensOverride == null,
                "Token override already set");
        PlatformAuthEntraTokensReader.sReadTokensOverride = new ResultOverride(resultCode, result);
    }

    @CalledByNativeForTesting
    public static void resetResultOverrideForTesting() {
        Preconditions.checkState(
                PlatformAuthEntraTokensReader.sReadTokensOverride != null,
                "Token override not set");
        PlatformAuthEntraTokensReader.sReadTokensOverride = null;
    }
}
