// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_check_wrapper;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordCheckResult;
import org.chromium.chrome.browser.pwd_check_wrapper.PasswordCheckController.PasswordStorageType;

import java.util.OptionalInt;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;

/** Unit tests for {@link ChromeNativePasswordCheckController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeNativePasswordCheckControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private PasswordCheck mPasswordCheck;
    private ChromeNativePasswordCheckController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        mController = new ChromeNativePasswordCheckController();
    }

    /**
     * The flow: checkPasswords is called -> as a result of password check 0 breached credentials
     * are obtained -> passwords loading has finished.
     */
    @Test
    public void passwordCheckReturnsNoBreachedPasswords()
            throws ExecutionException, InterruptedException {
        // Set fake to return 0 breached credentials.
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(10);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(0);

        CompletableFuture<PasswordCheckResult> passwordCheckResultFuture =
                mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE);
        verify(mPasswordCheck).startCheck();

        mController.onCompromisedCredentialsFetchCompleted();
        mController.onSavedPasswordsFetchCompleted();

        PasswordCheckResult passwordCheckResult = passwordCheckResultFuture.get();
        assertEquals(OptionalInt.of(0), passwordCheckResult.getBreachedCount());
        assertEquals(OptionalInt.of(10), passwordCheckResult.getTotalPasswordsCount());
        assertEquals(null, passwordCheckResult.getError());
    }

    /**
     * The flow: passwords loading has finished and there are 0 passwords -> as a result of password
     * check 0 breached credentials are obtained.
     */
    @Test
    public void passwordCheckReturnsNoAnyPasswords()
            throws ExecutionException, InterruptedException {
        // Set fake to return 0 breached credentials.
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(0);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(0);

        CompletableFuture<PasswordCheckResult> passwordCheckResultFuture =
                mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE);
        verify(mPasswordCheck).startCheck();

        mController.onSavedPasswordsFetchCompleted();
        mController.onCompromisedCredentialsFetchCompleted();
        mController.onPasswordCheckStatusChanged(PasswordCheckUIStatus.ERROR_NO_PASSWORDS);

        PasswordCheckResult passwordCheckResult = passwordCheckResultFuture.get();
        assertEquals(OptionalInt.of(0), passwordCheckResult.getBreachedCount());
        assertEquals(OptionalInt.of(0), passwordCheckResult.getTotalPasswordsCount());
        assertEquals(null, passwordCheckResult.getError());
    }

    /**
     * The flow: passwords loading has finished -> checkPasswords is called -> obtained 1 breached
     * credential.
     */
    @Test
    public void passwordCheckReturnsBreachedPassword()
            throws ExecutionException, InterruptedException {
        // Set fake to return 1 breached credential.
        final int breachedCount = 1;
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(breachedCount);
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(10);

        CompletableFuture<PasswordCheckResult> passwordCheckResultFuture =
                mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE);
        verify(mPasswordCheck).startCheck();

        mController.onCompromisedCredentialsFetchCompleted();
        mController.onSavedPasswordsFetchCompleted();

        PasswordCheckResult passwordCheckResult = passwordCheckResultFuture.get();
        assertEquals(OptionalInt.of(breachedCount), passwordCheckResult.getBreachedCount());
        assertEquals(OptionalInt.of(10), passwordCheckResult.getTotalPasswordsCount());
        assertEquals(null, passwordCheckResult.getError());
    }

    /** The flow: passwords loading has finished -> checkPasswords returns an error state. */
    @Test
    public void passwordCheckReturnsOfflineError() throws ExecutionException, InterruptedException {
        CompletableFuture<PasswordCheckResult> passwordCheckResultFuture =
                mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE);
        verify(mPasswordCheck).startCheck();

        mController.onPasswordCheckStatusChanged(PasswordCheckUIStatus.ERROR_OFFLINE);

        PasswordCheckResult passwordCheckResult = passwordCheckResultFuture.get();
        assertEquals(OptionalInt.empty(), passwordCheckResult.getBreachedCount());
        assertEquals(OptionalInt.empty(), passwordCheckResult.getTotalPasswordsCount());
        assertEquals(
                PasswordCheckUIStatus.ERROR_OFFLINE,
                ((PasswordCheckNativeException) passwordCheckResult.getError()).errorCode);
    }

    /**
     * The flow: getBreachedCredentialsCount is called -> breached credentials fetch completed
     * giving 1 breached credential -> then all passwords fetch is completed.
     */
    @Test
    public void getBreachedCredentialsCountTest() throws ExecutionException, InterruptedException {
        // Set fake to return 1 breached credential.
        final int breachedCount = 1;
        when(mPasswordCheck.hasAccountForRequest()).thenReturn(true);
        when(mPasswordCheck.getCompromisedCredentialsCount()).thenReturn(breachedCount);
        when(mPasswordCheck.getSavedPasswordsCount()).thenReturn(10);

        CompletableFuture<PasswordCheckResult> passwordCheckResultFuture =
                mController.getBreachedCredentialsCount(PasswordStorageType.LOCAL_STORAGE);
        verify(mPasswordCheck).addObserver(mController, true);

        mController.onCompromisedCredentialsFetchCompleted();
        mController.onSavedPasswordsFetchCompleted();

        PasswordCheckResult passwordCheckResult = passwordCheckResultFuture.get();
        assertEquals(OptionalInt.of(breachedCount), passwordCheckResult.getBreachedCount());
        assertEquals(OptionalInt.of(10), passwordCheckResult.getTotalPasswordsCount());
        assertEquals(null, passwordCheckResult.getError());
    }

    @Test
    public void passwordCheckControllerIsDestroyedProperly() {
        mController.checkPasswords(PasswordStorageType.LOCAL_STORAGE);

        mController.destroy();
        verify(mPasswordCheck).stopCheck();
        verify(mPasswordCheck).removeObserver(mController);
    }

    @Test
    public void getBreachedCredentialsCountReturnsSignedOutError()
            throws ExecutionException, InterruptedException {
        when(mPasswordCheck.hasAccountForRequest()).thenReturn(false);

        PasswordCheckResult passwordCheckResult =
                mController.getBreachedCredentialsCount(PasswordStorageType.LOCAL_STORAGE).get();

        assertEquals(OptionalInt.empty(), passwordCheckResult.getBreachedCount());
        assertEquals(OptionalInt.empty(), passwordCheckResult.getTotalPasswordsCount());
        assertEquals(
                PasswordCheckUIStatus.ERROR_SIGNED_OUT,
                ((PasswordCheckNativeException) passwordCheckResult.getError()).errorCode);
    }
}
