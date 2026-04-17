// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.JniOnceCallback2;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.enterprise.platform_auth.entra_provider_android.Status;
import org.chromium.chrome.browser.flags.ChromeSwitches;

import java.io.IOException;
import java.security.MessageDigest;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
public class PlatformAuthEntraTokensReaderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TEST_URL = "https://example.com";
    private static final String BROKER_ACCOUNT_TYPE = "com.microsoft.entra";
    private static final String BUNDLE_RESULT_KEY = "sso_header_result";
    private static final String TEST_HEADERS = "{\"Authorization\":\"Bearer token\"}";

    private static final String TRUSTED_PACKAGE_NAME = "com.azure.authenticator";
    private static final String UNTRUSTED_PACKAGE_NAME = "com.malicious.app";
    private static final byte[] VALID_TEST_SIGNATURE = "valid_test_signature".getBytes();
    private static final byte[] INVALID_TEST_SIGNATURE = "invalid_test_signature".getBytes();

    @Mock private Context mContext;
    @Mock private AccountManager mAccountManager;
    @Mock private AccountManagerFuture<Bundle> mMockFuture;
    @Mock private JniOnceCallback2<Integer, String> mCallback;

    @Mock private PackageManager mPackageManager;
    @Mock private PackageInfo mPackageInfo;
    @Mock private SigningInfo mSigningInfo;
    @Mock private Signature mSignature;

    @Before
    public void setUp() throws Exception {
        ContextUtils.initApplicationContextForTests(mContext);

        when(mContext.getSystemService(Context.ACCOUNT_SERVICE)).thenReturn(mAccountManager);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);

        // Setup default PackageInfo and SigningInfo mocks
        when(mPackageManager.getPackageInfo(
                        anyString(), eq(PackageManager.GET_SIGNING_CERTIFICATES)))
                .thenReturn(mPackageInfo);
        mPackageInfo.signingInfo = mSigningInfo;
        when(mSigningInfo.hasMultipleSigners()).thenReturn(false);
        when(mSigningInfo.getSigningCertificateHistory()).thenReturn(new Signature[] {mSignature});

        // Set up the testing signature override
        MessageDigest md = MessageDigest.getInstance("SHA-512");
        byte[] expectedHash = md.digest(VALID_TEST_SIGNATURE);
        PlatformAuthEntraTokensReader.sSignatureSha512BytesForTesting = expectedHash;

        setupBroker(TRUSTED_PACKAGE_NAME);
        when(mSignature.toByteArray()).thenReturn(VALID_TEST_SIGNATURE);
    }

    @After
    public void tearDown() {
        PlatformAuthEntraTokensReader.sSignatureSha512BytesForTesting = null;
    }

    private void runReadTokensOnBackgroundThread(
            String url, JniOnceCallback2<Integer, String> callback) throws InterruptedException {
        CountDownLatch latch = new CountDownLatch(1);

        PostTask.postTask(
                TaskTraits.BEST_EFFORT_MAY_BLOCK,
                () -> {
                    PlatformAuthEntraTokensReader.readTokens(url, callback);
                    latch.countDown();
                });

        RobolectricUtil.runAllBackgroundAndUiAllowBlocking();

        if (!latch.await(5, TimeUnit.SECONDS)) {
            throw new RuntimeException("Test timed out waiting for background task.");
        }
    }

    @Test
    public void testReadTokens_noBrokerRegistered() throws Exception {
        setupBroker(null);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.NO_BROKER_REGISTERED), anyString());
    }

    @Test
    public void testReadTokens_untrustedPackageProvider() throws Exception {
        setupBroker(UNTRUSTED_PACKAGE_NAME);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.UNEXPECTED_PACKAGE_PROVIDER), anyString());
    }

    @Test
    public void testReadTokens_signatureVerificationFailed() throws Exception {
        when(mSignature.toByteArray()).thenReturn(INVALID_TEST_SIGNATURE);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.SIGNATURE_VERIFICATION_FAILED), anyString());
    }

    @Test
    public void testReadTokens_packageManagerThrowsException() throws Exception {
        when(mPackageManager.getPackageInfo(
                        anyString(), eq(PackageManager.GET_SIGNING_CERTIFICATES)))
                .thenThrow(new PackageManager.NameNotFoundException());

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.UNEXPECTED_ERROR), anyString());
    }

    @Test
    public void testReadTokens_success() throws Exception {
        Bundle resultBundle = new Bundle();
        resultBundle.putString(BUNDLE_RESULT_KEY, TEST_HEADERS);
        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class))).thenReturn(resultBundle);

        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.OK), eq(TEST_HEADERS));
    }

    @Test
    public void testReadTokens_nullBundleResult() throws Exception {
        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class))).thenReturn(null);
        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.NO_BUNDLE_RESULT), anyString());
    }

    @Test
    public void testReadTokens_nullHeadersEntry() throws Exception {
        Bundle emptyBundle = new Bundle();
        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class))).thenReturn(emptyBundle);
        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.INVALID_BUNDLE_FORMAT), anyString());
    }

    @Test
    public void testReadTokens_authenticatorException() throws Exception {
        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class)))
                .thenThrow(new AuthenticatorException("Mock auth error"));
        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.UNEXPECTED_ERROR), anyString());
    }

    @Test
    public void testReadTokens_bundleContainsCustomError() throws Exception {
        Bundle errorBundle = new Bundle();
        errorBundle.putString("error_code", "url_not_allowed");
        errorBundle.putString("error_message", "The URL is outside the allowed SSO domain list");

        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class))).thenReturn(errorBundle);
        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.BUNDLE_RESULT_CONTAINS_ENTRA_ERROR), anyString());
    }

    @Test
    public void testReadTokens_bundleContainsStandardError() throws Exception {
        Bundle errorBundle = new Bundle();
        errorBundle.putInt(AccountManager.KEY_ERROR_CODE, AccountManager.ERROR_CODE_BAD_REQUEST);
        errorBundle.putString(AccountManager.KEY_ERROR_MESSAGE, "Bad Request");

        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class))).thenReturn(errorBundle);
        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.BUNDLE_RESULT_CONTAINS_OS_ERROR), anyString());
    }

    @Test
    public void testReadTokens_ioException() throws Exception {
        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class)))
                .thenThrow(new IOException("Mock IO error"));
        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.UNEXPECTED_ERROR), anyString());
    }

    @Test
    public void testReadTokens_timeoutException() throws Exception {
        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class)))
                .thenThrow(new OperationCanceledException("Mock timeout"));
        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.TIMEOUT), anyString());
    }

    @Test
    public void testReadTokens_debugProvider_withoutFlag_blocked() throws Exception {
        // Setup a debug-only package (exists in DEBUG_PROVIDERS, but not TRUSTED_PROVIDERS)
        setupBroker("com.microsoft.mockauthapp");

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        // Without the CLI flag, expectedSignature is null, but we recognize the package
        // as a debug provider, so it gets explicitly blocked.
        verify(mCallback).onResult(eq(Status.DISALLOWED_DEBUG_PACKAGE_PROVIDER), anyString());
    }

    @Test
    @CommandLineFlags.Add(ChromeSwitches.ANDROID_ENTRA_SSO_ALLOW_DEBUG_BROKERS)
    public void testReadTokens_debugProvider_withFlag_success() throws Exception {
        // Setup a debug-only package
        setupBroker("com.microsoft.mockauthapp");

        // Setup a successful token read. Because the flag is present, getPackageSignature
        // will return the debug signature, avoiding the null check.
        Bundle resultBundle = new Bundle();
        resultBundle.putString(BUNDLE_RESULT_KEY, TEST_HEADERS);
        when(mMockFuture.getResult(anyLong(), any(TimeUnit.class))).thenReturn(resultBundle);

        when(mAccountManager.getAuthToken(
                        any(Account.class),
                        eq("sso_header"),
                        any(Bundle.class),
                        anyBoolean(),
                        any(),
                        any()))
                .thenReturn(mMockFuture);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(Status.OK), eq(TEST_HEADERS));
    }

    private void setupBroker(String packageName) {
        if (packageName == null) {
            when(mAccountManager.getAuthenticatorTypes())
                    .thenReturn(new AuthenticatorDescription[0]);
        } else {
            AuthenticatorDescription entraBroker =
                    new AuthenticatorDescription(BROKER_ACCOUNT_TYPE, packageName, 0, 0, 0, 0);
            when(mAccountManager.getAuthenticatorTypes())
                    .thenReturn(new AuthenticatorDescription[] {entraBroker});
        }
    }
}
