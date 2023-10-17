// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static android.os.Looper.getMainLooper;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.robolectric.Shadows.shadowOf;

import android.app.PendingIntent;

import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.CommonStatusCodes;
import com.google.android.gms.common.api.Status;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerBackendException;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;

/** Tests for {@link FakeCredentialManagerLauncher}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class FakeCredentialManagerLauncherTest {
    private static final String TEST_EMAIL_ADDRESS = "test@email.com";
    private FakeCredentialManagerLauncher mFakeLauncher;

    @Before
    public void setUp() {
        mFakeLauncher = new FakeCredentialManagerLauncher();
        mFakeLauncher.setIntent(mock(PendingIntent.class));
    }

    @Test
    public void testGetAccountCredentialManagerIntentSucceeds() {
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeLauncher.getAccountCredentialManagerIntent(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                TEST_EMAIL_ADDRESS,
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was called.
        assertNotNull(successCallbackHelper.getOnlyPayloadBlocking());
        // Verify that failure callback was not called.
        assertEquals(0, failureCallbackHelper.getCallCount());
    }

    @Test
    public void testGetAccountCredentialManagerIntentWithNoAccountFails() {
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeLauncher.getAccountCredentialManagerIntent(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                null,
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(0, successCallbackHelper.getCallCount());
        // Verify that failure callback was called.

        assertTrue(
                failureCallbackHelper.getOnlyPayloadBlocking()
                        instanceof CredentialManagerBackendException);
        assertEquals(
                ((CredentialManagerBackendException) failureCallbackHelper.getOnlyPayloadBlocking())
                        .errorCode,
                CredentialManagerError.NO_ACCOUNT_NAME);
    }

    @Test
    public void testGetAccountCredentialManagerIntentDemonstratesAPIErrorFails() {
        mFakeLauncher.setCredentialManagerError(
                new ApiException(new Status(CommonStatusCodes.INTERNAL_ERROR)));
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeLauncher.getAccountCredentialManagerIntent(
                org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer
                        .CHROME_SETTINGS,
                TEST_EMAIL_ADDRESS,
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(0, successCallbackHelper.getCallCount());
        // Verify that failure callback was called.
        assertTrue(failureCallbackHelper.getOnlyPayloadBlocking() instanceof ApiException);
        assertEquals(
                ((ApiException) failureCallbackHelper.getOnlyPayloadBlocking()).getStatusCode(),
                CommonStatusCodes.INTERNAL_ERROR);
    }

    @Test
    public void testGetCredentialManagerIntentForLocalSucceeds() {
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeLauncher.getLocalCredentialManagerIntent(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was called.
        assertNotNull(successCallbackHelper.getOnlyPayloadBlocking());
        // Verify that failure callback was not called.
        assertEquals(0, failureCallbackHelper.getCallCount());
    }

    @Test
    public void testGetCredentialManagerIntentForLocalDemonstratesAPIErrorFails() {
        mFakeLauncher.setCredentialManagerError(
                new ApiException(new Status(CommonStatusCodes.INTERNAL_ERROR)));
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeLauncher.getLocalCredentialManagerIntent(
                ManagePasswordsReferrer.CHROME_SETTINGS,
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(0, successCallbackHelper.getCallCount());
        // Verify that failure callback was called.
        assertTrue(failureCallbackHelper.getOnlyPayloadBlocking() instanceof ApiException);
        assertEquals(
                ((ApiException) failureCallbackHelper.getOnlyPayloadBlocking()).getStatusCode(),
                CommonStatusCodes.INTERNAL_ERROR);
    }
}
