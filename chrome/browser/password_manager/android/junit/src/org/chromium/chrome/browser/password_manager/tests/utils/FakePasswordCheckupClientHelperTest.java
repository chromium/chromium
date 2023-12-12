// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.tests.utils;

import static android.os.Looper.getMainLooper;

import static org.junit.Assert.assertEquals;
import static org.robolectric.Shadows.shadowOf;

import android.app.PendingIntent;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncher.CredentialManagerError;
import org.chromium.chrome.browser.password_manager.FakePasswordCheckupClientHelper;
import org.chromium.chrome.browser.password_manager.PasswordCheckReferrer;
import org.chromium.chrome.browser.password_manager.PasswordCheckupClientHelper.PasswordCheckBackendException;

import java.util.Optional;

/** Tests for {@link FakePasswordCheckupClientHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class FakePasswordCheckupClientHelperTest {
    private static final String TEST_ACCOUNT = "test@example.com";
    private FakePasswordCheckupClientHelper mFakeHelper;
    @Mock private PendingIntent mPendingIntentForLocalCheckupMock;
    @Mock private PendingIntent mPendingIntentForAccountCheckupMock;

    private final PayloadCallbackHelper<PendingIntent> mSuccessCallbackHelper =
            new PayloadCallbackHelper<>();
    private final PayloadCallbackHelper<Exception> mFailureCallbackHelper =
            new PayloadCallbackHelper<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mFakeHelper = new FakePasswordCheckupClientHelper();
        mFakeHelper.setIntentForLocalCheckup(mPendingIntentForLocalCheckupMock);
        mFakeHelper.setIntentForAccountCheckup(mPendingIntentForAccountCheckupMock);
    }

    @Test
    public void testGetPasswordCheckupIntentForLocalCheckupSucceeds() {
        mFakeHelper.getPasswordCheckupIntent(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.empty(),
                mSuccessCallbackHelper::notifyCalled,
                mFailureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was called.
        assertEquals(
                mSuccessCallbackHelper.getOnlyPayloadBlocking(), mPendingIntentForLocalCheckupMock);
        // Verify that failure callback was not called.
        assertEquals(mFailureCallbackHelper.getCallCount(), 0);
    }

    @Test
    public void testGetPasswordCheckupIntentForAccountCheckupSucceeds() {
        mFakeHelper.getPasswordCheckupIntent(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_ACCOUNT),
                mSuccessCallbackHelper::notifyCalled,
                mFailureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was called.
        assertEquals(
                mSuccessCallbackHelper.getOnlyPayloadBlocking(),
                mPendingIntentForAccountCheckupMock);
        // Verify that failure callback was not called.
        assertEquals(mFailureCallbackHelper.getCallCount(), 0);
    }

    @Test
    public void testGetPasswordCheckupIntentReturnsError() {
        final Exception expectedException =
                new PasswordCheckBackendException("test", CredentialManagerError.UNCATEGORIZED);
        mFakeHelper.setError(expectedException);

        final PayloadCallbackHelper<PendingIntent> successCallbackHelper =
                new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeHelper.getPasswordCheckupIntent(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_ACCOUNT),
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(successCallbackHelper.getCallCount(), 0);
        // Verify that failure callback was called.
        assertEquals(failureCallbackHelper.getOnlyPayloadBlocking(), expectedException);
    }

    @Test
    public void testRunPasswordCheckupInBackgroundSucceeds() {
        final PayloadCallbackHelper<Void> successCallbackHelper = new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_ACCOUNT),
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was called.
        assertEquals(successCallbackHelper.getCallCount(), 1);
        // Verify that failure callback was not called.
        assertEquals(failureCallbackHelper.getCallCount(), 0);
    }

    @Test
    public void testRunPasswordCheckupInBackgroundReturnsError() {
        final Exception expectedException =
                new PasswordCheckBackendException("test", CredentialManagerError.UNCATEGORIZED);
        mFakeHelper.setError(expectedException);

        final PayloadCallbackHelper<Void> successCallbackHelper = new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeHelper.runPasswordCheckupInBackground(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_ACCOUNT),
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(successCallbackHelper.getCallCount(), 0);
        // Verify that failure callback was called.
        assertEquals(failureCallbackHelper.getOnlyPayloadBlocking(), expectedException);
    }

    @Test
    public void testGetBreachedCredentialsCountSucceeds() {
        final Integer breachedCount = 123;
        mFakeHelper.setBreachedCredentialsCount(breachedCount);

        final PayloadCallbackHelper<Integer> successCallbackHelper = new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_ACCOUNT),
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was called.
        assertEquals(successCallbackHelper.getOnlyPayloadBlocking(), breachedCount);
        // Verify that failure callback was not called.
        assertEquals(failureCallbackHelper.getCallCount(), 0);
    }

    @Test
    public void testGetBreachedCredentialsCountReturnsError() {
        final Exception expectedException =
                new PasswordCheckBackendException("test", CredentialManagerError.UNCATEGORIZED);
        mFakeHelper.setError(expectedException);

        final PayloadCallbackHelper<Integer> successCallbackHelper = new PayloadCallbackHelper<>();
        final PayloadCallbackHelper<Exception> failureCallbackHelper =
                new PayloadCallbackHelper<>();

        mFakeHelper.getBreachedCredentialsCount(
                PasswordCheckReferrer.SAFETY_CHECK,
                Optional.of(TEST_ACCOUNT),
                successCallbackHelper::notifyCalled,
                failureCallbackHelper::notifyCalled);

        // Move the clock forward
        shadowOf(getMainLooper()).idle();
        // Verify that success callback was not called.
        assertEquals(successCallbackHelper.getCallCount(), 0);
        // Verify that failure callback was called.
        assertEquals(failureCallbackHelper.getOnlyPayloadBlocking(), expectedException);
    }
}
