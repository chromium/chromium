// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static android.os.Looper.getMainLooper;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.robolectric.Shadows.shadowOf;

import android.app.PendingIntent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.PayloadCallbackHelper;
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
    public void testGetCredentialManagerIntentForAccountSucceeds() {
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Integer> failureCallbackHelper = new PayloadCallbackHelper<>();

        mFakeLauncher.getCredentialManagerIntentForAccount(ManagePasswordsReferrer.CHROME_SETTINGS,
                TEST_EMAIL_ADDRESS, successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was called.
        assertNotNull(successCallbackHelper.getOnlyPayloadBlocking());
        // Verify that failure callback was not called.
        assertEquals(0, failureCallbackHelper.getCallCount());
    }

    @Test
    public void testGetCredentialManagerIntentForAccountWithNoAccountFails() {
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Integer> failureCallbackHelper = new PayloadCallbackHelper<>();

        mFakeLauncher.getCredentialManagerIntentForAccount(ManagePasswordsReferrer.CHROME_SETTINGS,
                null, successCallbackHelper::notifyCalled, failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(0, successCallbackHelper.getCallCount());
        // Verify that failure callback was called.
        assertEquals(failureCallbackHelper.getOnlyPayloadBlocking().intValue(),
                CredentialManagerError.NO_ACCOUNT_NAME);
    }

    @Test
    public void testGetCredentialManagerIntentForAccountDemonstratesAPIErrorFails() {
        mFakeLauncher.setCredentialManagerError(CredentialManagerError.API_ERROR);
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Integer> failureCallbackHelper = new PayloadCallbackHelper<>();

        mFakeLauncher.getCredentialManagerIntentForAccount(ManagePasswordsReferrer.CHROME_SETTINGS,
                TEST_EMAIL_ADDRESS, successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(0, successCallbackHelper.getCallCount());
        // Verify that failure callback was called.
        assertEquals(failureCallbackHelper.getOnlyPayloadBlocking().intValue(),
                CredentialManagerError.API_ERROR);
    }

    @Test
    public void testGetCredentialManagerIntentForLocalSucceeds() {
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Integer> failureCallbackHelper = new PayloadCallbackHelper<>();

        mFakeLauncher.getCredentialManagerIntentForLocal(ManagePasswordsReferrer.CHROME_SETTINGS,
                successCallbackHelper::notifyCalled, failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was called.
        assertNotNull(successCallbackHelper.getOnlyPayloadBlocking());
        // Verify that failure callback was not called.
        assertEquals(0, failureCallbackHelper.getCallCount());
    }

    @Test
    public void testGetCredentialManagerIntentForLocalDemonstratesAPIErrorFails() {
        mFakeLauncher.setCredentialManagerError(CredentialManagerError.API_ERROR);
        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Integer> failureCallbackHelper = new PayloadCallbackHelper<>();

        mFakeLauncher.getCredentialManagerIntentForLocal(ManagePasswordsReferrer.CHROME_SETTINGS,
                successCallbackHelper::notifyCalled, failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(0, successCallbackHelper.getCallCount());
        // Verify that failure callback was called.
        assertEquals(failureCallbackHelper.getOnlyPayloadBlocking().intValue(),
                CredentialManagerError.API_ERROR);
    }
}
