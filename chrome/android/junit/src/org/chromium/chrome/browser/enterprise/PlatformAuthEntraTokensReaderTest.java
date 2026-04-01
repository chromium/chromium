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
import android.os.Bundle;

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
import org.chromium.chrome.browser.enterprise.platform_auth.entra_provider_android.TokenReadResult;

import java.io.IOException;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(BaseRobolectricTestRunner.class)
public class PlatformAuthEntraTokensReaderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TEST_URL = "https://example.com";
    private static final String BROKER_ACCOUNT_TYPE = "com.microsoft.entra";
    private static final String BUNDLE_RESULT_KEY = "sso_header_result";
    private static final String TEST_HEADERS = "{\"Authorization\":\"Bearer token\"}";

    @Mock private Context mContext;
    @Mock private AccountManager mAccountManager;
    @Mock private AccountManagerFuture<Bundle> mMockFuture;
    @Mock private JniOnceCallback2<Integer, String> mCallback;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(mContext);
        when(mContext.getSystemService(Context.ACCOUNT_SERVICE)).thenReturn(mAccountManager);
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
        when(mAccountManager.getAuthenticatorTypes()).thenReturn(new AuthenticatorDescription[0]);

        runReadTokensOnBackgroundThread(TEST_URL, mCallback);

        verify(mCallback).onResult(eq(TokenReadResult.NO_BROKER_REGISTERED), anyString());
    }

    @Test
    public void testReadTokens_success() throws Exception {
        setupValidBroker();

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

        verify(mCallback).onResult(TokenReadResult.OK, TEST_HEADERS);
    }

    @Test
    public void testReadTokens_nullBundleResult() throws Exception {
        setupValidBroker();

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

        verify(mCallback).onResult(eq(TokenReadResult.INVALID_BUNDLE_FORMAT), anyString());
    }

    @Test
    public void testReadTokens_nullHeadersEntry() throws Exception {
        setupValidBroker();

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

        verify(mCallback).onResult(eq(TokenReadResult.INVALID_BUNDLE_FORMAT), anyString());
    }

    @Test
    public void testReadTokens_authenticatorException() throws Exception {
        setupValidBroker();

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

        verify(mCallback).onResult(eq(TokenReadResult.UNEXPECTED_ERROR), anyString());
    }

    @Test
    public void testReadTokens_ioException() throws Exception {
        setupValidBroker();

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

        verify(mCallback).onResult(eq(TokenReadResult.UNEXPECTED_ERROR), anyString());
    }

    @Test
    public void testReadTokens_timeoutException() throws Exception {
        setupValidBroker();

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

        verify(mCallback).onResult(eq(TokenReadResult.UNEXPECTED_ERROR), anyString());
    }

    private void setupValidBroker() {
        AuthenticatorDescription entraBroker =
                new AuthenticatorDescription(
                        BROKER_ACCOUNT_TYPE, "com.microsoft.entra", 0, 0, 0, 0);
        when(mAccountManager.getAuthenticatorTypes())
                .thenReturn(new AuthenticatorDescription[] {entraBroker});
    }
}
