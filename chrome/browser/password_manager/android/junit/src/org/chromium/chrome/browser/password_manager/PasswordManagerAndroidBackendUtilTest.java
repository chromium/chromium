// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;

import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.ResolvableApiException;
import com.google.android.gms.common.api.Status;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.password_manager.PasswordStoreAndroidBackend.BackendException;

/**
 * Tests for the utility methods used by various parts of the password manager backend (e.g. the
 * password store, the settings accessor).
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.PER_CLASS)
public class PasswordManagerAndroidBackendUtilTest {

    @Test
    public void testUtilsForBackendException() {
        BackendException exception =
                new BackendException(
                        "Cannot call API without context.", AndroidBackendErrorType.NO_CONTEXT);
        Assert.assertEquals(
                AndroidBackendErrorType.NO_CONTEXT,
                PasswordManagerAndroidBackendUtil.getBackendError(exception));
        Assert.assertEquals(0, PasswordManagerAndroidBackendUtil.getApiErrorCode(exception));
    }

    @Test
    public void testUtilsForApiException() {
        ApiException apiException = new ApiException(new Status(CommonStatusCodes.ERROR, ""));
        Assert.assertEquals(
                AndroidBackendErrorType.EXTERNAL_ERROR,
                PasswordManagerAndroidBackendUtil.getBackendError(apiException));
        Assert.assertEquals(
                CommonStatusCodes.ERROR,
                PasswordManagerAndroidBackendUtil.getApiErrorCode(apiException));
        Assert.assertNull(PasswordManagerAndroidBackendUtil.getConnectionResultCode(apiException));
    }

    @Test
    public void testUtilsForApiExceptionWithConnectionResult() {
        ApiException apiException =
                new ApiException(
                        new Status(new ConnectionResult(ConnectionResult.API_UNAVAILABLE), ""));
        Assert.assertEquals(
                AndroidBackendErrorType.EXTERNAL_ERROR,
                PasswordManagerAndroidBackendUtil.getBackendError(apiException));
        Assert.assertEquals(
                CommonStatusCodes.API_NOT_CONNECTED,
                PasswordManagerAndroidBackendUtil.getApiErrorCode(apiException));
        Assert.assertEquals(
                ConnectionResult.API_UNAVAILABLE,
                PasswordManagerAndroidBackendUtil.getConnectionResultCode(apiException).intValue());
    }

    @Test
    public void testUtilsReturnNullConnectionResultForNonApiException() {
        BackendException exception =
                new BackendException(
                        "Cannot call API without context.", AndroidBackendErrorType.NO_CONTEXT);
        Assert.assertNull(PasswordManagerAndroidBackendUtil.getConnectionResultCode(exception));
    }

    @Test
    public void testUtilsForUncategorizedException() {
        Exception exception = new Exception();
        Assert.assertEquals(
                AndroidBackendErrorType.UNCATEGORIZED,
                PasswordManagerAndroidBackendUtil.getBackendError(exception));
        Assert.assertEquals(0, PasswordManagerAndroidBackendUtil.getApiErrorCode(exception));
    }

    @Test
    public void testUtilsForResolvableApiExceptionAuth() throws CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        ResolvableApiException apiException =
                new ResolvableApiException(
                        new Status(
                                ChromeSyncStatusCode.AUTH_ERROR_RESOLVABLE, "", pendingIntentMock));
        PasswordManagerAndroidBackendUtil.handleResolvableApiException(apiException);
        verify(pendingIntentMock, never()).send();
    }

    @Test
    public void testUtilsForResolvableApiExceptionNonAuth() throws CanceledException {
        PendingIntent pendingIntentMock = mock(PendingIntent.class);
        ResolvableApiException apiException =
                new ResolvableApiException(
                        new Status(CommonStatusCodes.RESOLUTION_REQUIRED, "", pendingIntentMock));
        PasswordManagerAndroidBackendUtil.handleResolvableApiException(apiException);
        verify(pendingIntentMock).send();
    }
}
