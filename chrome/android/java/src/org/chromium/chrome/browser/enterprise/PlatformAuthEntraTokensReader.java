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
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.content.pm.SigningInfo;
import android.os.Bundle;
import android.util.Base64;

import androidx.annotation.VisibleForTesting;

import com.google.common.base.Preconditions;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback2;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.enterprise.platform_auth.entra_provider_android.Status;
import org.chromium.chrome.browser.flags.ChromeSwitches;

import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;

@NullMarked
@JNINamespace("enterprise_auth")
public class PlatformAuthEntraTokensReader {
    private static final String AUTH_TOKEN_TYPE = "sso_header";
    private static final String BROKER_ACCOUNT_TYPE = "com.microsoft.entra";
    private static final String BUNDLE_RESULT_KEY = "sso_header_result";
    private static final long TIMEOUT_SECONDS = 10;

    // Maps trusted package provider names to their respective signature thumbprint in SHA-512.
    private static final Map<String, byte[]> TRUSTED_PROVIDERS = new HashMap<>();
    private static final Map<String, byte[]> DEBUG_PROVIDERS = new HashMap<>();

    private static @Nullable ResultOverride sReadTokensOverride;
    @VisibleForTesting public static byte @Nullable [] sSignatureSha512BytesForTesting;

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
            callback.onResult(sReadTokensOverride.status, sReadTokensOverride.result);
            return;
        }

        // This function makes a blocking call and should never be run on the UI thread.
        ThreadUtils.assertOnBackgroundThread();

        try {
            final Context context = ContextUtils.getApplicationContext();
            final AccountManager accountManager = AccountManager.get(context);

            final String providerPackageName = getProviderPackageName(accountManager);
            if (providerPackageName == null) {
                callback.onResult(Status.NO_BROKER_REGISTERED, "");
                return;
            }

            final byte @Nullable [] expectedSignature = getPackageSignature(providerPackageName);
            if (expectedSignature == null) {
                if (!debugProvidersEnabled() && DEBUG_PROVIDERS.containsKey(providerPackageName)) {
                    callback.onResult(
                            Status.DISALLOWED_DEBUG_PACKAGE_PROVIDER,
                            "attempt to get authentication headers from a non-production"
                                    + " authentication broker blocked because --"
                                    + ChromeSwitches.ANDROID_ENTRA_SSO_ALLOW_DEBUG_BROKERS
                                    + " was not enabled");
                } else {
                    callback.onResult(
                            Status.UNEXPECTED_PACKAGE_PROVIDER,
                            providerPackageName
                                    + " is not a trusted provider for the authentication headers.");
                }
                return;
            }

            if (!verifyPackageSignature(providerPackageName, context, expectedSignature)) {
                callback.onResult(
                        Status.SIGNATURE_VERIFICATION_FAILED,
                        "could not verify signature of " + providerPackageName);
                return;
            }

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
                callback.onResult(Status.NO_BUNDLE_RESULT, "null bundle result");
                return;
            }

            // The returned Bundle follows a specific contract defined by the Entra broker:
            //
            // 1. SUCCESS:
            //    - "sso_header_result" (String): Contains a JSON array of SSO headers.
            //      Note: If the user is not signed in, this will be an empty JSON array "[]".
            //
            // 2. EXPECTED BROKER ERRORS:
            //    - "error_code" (String): Machine-readable identifier.
            //      Possible values: "caller_not_allowed", "url_not_allowed",
            //      "missing_sso_url", "invalid_token_type", "generation_failed".
            //    - "error_message" (String): Human-readable description of the error.
            //
            // 3. STANDARD ACCOUNT MANAGER ERRORS:
            //    - AccountManager.KEY_ERROR_CODE ("errorCode", int): OS-level error code.
            //      If "error_code" is present will be set to ERROR_CODE_BAD_REQUEST (8).
            //    - AccountManager.KEY_ERROR_MESSAGE ("errorMessage", String): Error description.
            //      if "error_code" is present will be set to "error_message".
            if (bundleResult.containsKey("error_code")) {
                String errorCode = bundleResult.getString("error_code");
                String errorMessage = bundleResult.getString("error_message", "Unknown error");
                callback.onResult(
                        Status.BUNDLE_RESULT_CONTAINS_ENTRA_ERROR,
                        "Returned bundle result reported " + errorCode + ": " + errorMessage);
                return;
            }

            if (bundleResult.containsKey(AccountManager.KEY_ERROR_CODE)) {
                int standardErrorCode = bundleResult.getInt(AccountManager.KEY_ERROR_CODE);
                String standardErrorMsg = bundleResult.getString(AccountManager.KEY_ERROR_MESSAGE);

                callback.onResult(
                        Status.BUNDLE_RESULT_CONTAINS_OS_ERROR,
                        "Returned bundle result did not report errors but contains"
                                + " AccountManager.KEY_ERROR_CODE "
                                + standardErrorCode
                                + ": "
                                + standardErrorMsg);
                return;
            }

            String stringResult = bundleResult.getString(BUNDLE_RESULT_KEY);
            if (stringResult == null) {
                callback.onResult(Status.INVALID_BUNDLE_FORMAT, "null headers entry");
                return;
            }

            callback.onResult(Status.OK, stringResult);
        } catch (AuthenticatorException
                | IOException
                | PackageManager.NameNotFoundException
                | NoSuchAlgorithmException e) {
            callback.onResult(Status.UNEXPECTED_ERROR, e.toString());
        } catch (OperationCanceledException e) {
            callback.onResult(Status.TIMEOUT, e.toString());
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

    private static byte @Nullable [] getPackageSignature(String providerPackageName) {
        // Certain packages have both a production and a debug signature.
        // If the switch is enabled the debug signature will take priority.
        if (debugProvidersEnabled() && DEBUG_PROVIDERS.containsKey(providerPackageName)) {
            return DEBUG_PROVIDERS.get(providerPackageName);
        }

        if (TRUSTED_PROVIDERS.containsKey(providerPackageName)) {
            return TRUSTED_PROVIDERS.get(providerPackageName);
        }

        return null;
    }

    private static boolean debugProvidersEnabled() {
        return CommandLine.getInstance()
                .hasSwitch(ChromeSwitches.ANDROID_ENTRA_SSO_ALLOW_DEBUG_BROKERS);
    }

    private static boolean verifyPackageSignature(
            String packageName, Context context, byte[] expectedSignature)
            throws PackageManager.NameNotFoundException, NoSuchAlgorithmException {
        final PackageManager packageManager = context.getPackageManager();
        final PackageInfo packageInfo =
                packageManager.getPackageInfo(packageName, PackageManager.GET_SIGNING_CERTIFICATES);
        final SigningInfo signingInfo = packageInfo.signingInfo;
        if (signingInfo == null) {
            return false;
        }

        Signature[] signatures;
        if (signingInfo.hasMultipleSigners()) {
            // Only a single signer is expected.
            return false;
        }
        signatures = signingInfo.getSigningCertificateHistory();

        for (Signature signature : signatures) {
            MessageDigest digest = MessageDigest.getInstance("SHA-512");
            byte[] currentSha512 = digest.digest(signature.toByteArray());
            if (currentSha512 == null) {
                continue;
            }
            if (Arrays.equals(currentSha512, expectedSignature)) {
                return true;
            }
            if (sSignatureSha512BytesForTesting != null
                    && Arrays.equals(currentSha512, sSignatureSha512BytesForTesting)) {

                return true;
            }
        }

        return false;
    }

    static {
        TRUSTED_PROVIDERS.put(
                "com.azure.authenticator",
                Base64.decode(
                        "Gu8CuaYmSV5CHWd6dz3tGPXIE+YTalCVIXi5lEBXpvUgsMKoHbU9Rqou3WNRNU1tsz8pvEADTCCJ5f02fbw9qw==",
                        Base64.DEFAULT));
        TRUSTED_PROVIDERS.put(
                "com.microsoft.windowsintune.companyportal",
                Base64.decode(
                        "jPpMoaNvcxSLMX4yG4C3Gf86rtTqh33SqpuRKg4WOP+MnnpA52zZgvKLW76U4Cqqf68iaBk9W7k/jhciiSAtgQ==",
                        Base64.DEFAULT));
        TRUSTED_PROVIDERS.put(
                "com.microsoft.appmanager",
                Base64.decode(
                        "WhUdh04ZkQLmNb//lKmohyqDdPMWXHcI0O3AvoLMtgF/smnED4r+Vguvgj6d4QG77Jl3avUKt6LeqF2TJPZVzg==",
                        Base64.DEFAULT));

        // Note: Certain package names overlap, but unlike `TRUSTED_PROVIDERS` these represent
        // non-production builds with separate signatures.
        DEBUG_PROVIDERS.put(
                "com.azure.authenticator",
                Base64.decode(
                        "pdAtoxfsEwbpQsIaua5Uobl5AQEjqt40aPXI7UY1lIW0NTmg0G4jHQ5T5mujSjjU06q4mEHs5hb6z/Mr0PNlmQ==",
                        Base64.DEFAULT));
        DEBUG_PROVIDERS.put(
                "com.microsoft.windowsintune.companyportal",
                Base64.decode(
                        "oIuNoUwMsxC10VneTQXnt/GXN+Pjqd6mpOKEMF/cH3i06K93TZMBWq+fHN/zt4zUe/W6zGj6YLymd1/tGuypNQ==",
                        Base64.DEFAULT));
        DEBUG_PROVIDERS.put(
                "com.microsoft.appmanager",
                Base64.decode(
                        "5PAhhZNSRRvq7vpTT5vrYJbSLh05AU8USf7oUTS239PEltebX87uGN7GhAe5244lJepwZ5RU4vu8N6ospXVOlg==",
                        Base64.DEFAULT));
        DEBUG_PROVIDERS.put(
                "com.microsoft.identity.testuserapp",
                Base64.decode(
                        "xxAk8S05zu0Nkce+X2J6IKJ2e7YE4F9ZorZj0YnYUQ2vw8vLc8VGGOqJdTnVySbbcy9VY8UDbOfeOETSErYllw==",
                        Base64.DEFAULT));
        DEBUG_PROVIDERS.put(
                "com.microsoft.mockauthapp",
                Base64.decode(
                        "QhjKSYYD31K7+C4q4Mpd08crE0LN/3GgnKVVuej4JWckUTc0Wp/i//LWLQnANaWiAjdESJJrjavu0cE6hkQihQ==",
                        Base64.DEFAULT));
        DEBUG_PROVIDERS.put(
                "com.microsoft.mockcp",
                Base64.decode(
                        "EZ2RCcsmf869Ec41PgHHnFdI0MgmVsADFFy8AtcfEKsjD1YAPtKxCMZVdT+y+K1IWRnPk4Lf2PUAcL5N49OqAA==",
                        Base64.DEFAULT));
        DEBUG_PROVIDERS.put(
                "com.microsoft.mockltw",
                Base64.decode(
                        "felxzv/rpqa69dOADXVVKnawk5x8snBW2k/kDxzQLVkbcdzAvrGm8gcBRItzUGIQTupHCTWksN6WBGbn+b0KIA==",
                        Base64.DEFAULT));
    }

    private static class ResultOverride {
        ResultOverride(Integer status, String result) {
            this.status = status;
            this.result = result;
        }

        public final Integer status;
        public final String result;
    }

    @CalledByNativeForTesting
    public static void setResultOverrideForTesting(
            @JniType("int") Integer status, @JniType("std::string") String result) {
        Preconditions.checkState(
                PlatformAuthEntraTokensReader.sReadTokensOverride == null,
                "Token override already set");
        PlatformAuthEntraTokensReader.sReadTokensOverride = new ResultOverride(status, result);
    }

    @CalledByNativeForTesting
    public static void resetResultOverrideForTesting() {
        Preconditions.checkState(
                PlatformAuthEntraTokensReader.sReadTokensOverride != null,
                "Token override not set");
        PlatformAuthEntraTokensReader.sReadTokensOverride = null;
    }
}
