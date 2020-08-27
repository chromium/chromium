// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.externalauth.ExternalAuthUtils;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninManagerTest {
    @Rule
    public final JniMocker mocker = new JniMocker();


    @Mock
    SigninManager.Natives mNativeMock;

    private AccountTrackerService mAccountTrackerService;
    private IdentityManager mIdentityManager;
    private IdentityMutator mIdentityMutator;
    private SigninManager mSigninManager;
    private CoreAccountInfo mAccount;

    @Before
    public void setUp() {
        initMocks(this);

        mocker.mock(SigninManagerJni.TEST_HOOKS, mNativeMock);

        doReturn(true).when(mNativeMock).isSigninAllowedByPolicy(anyLong());

        mAccountTrackerService = mock(AccountTrackerService.class);

        mIdentityMutator = mock(IdentityMutator.class);

        mIdentityManager = spy(
                new IdentityManager(0 /* nativeIdentityManager */, null /* OAuth2TokenService */));

        AndroidSyncSettings androidSyncSettings = mock(AndroidSyncSettings.class);

        ExternalAuthUtils externalAuthUtils = mock(ExternalAuthUtils.class);
        // Pretend Google Play services are available as it is required for the sign-in
        doReturn(false).when(externalAuthUtils).isGooglePlayServicesMissing(any());

        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        mSigninManager =
                new SigninManager(0 /* nativeSigninManagerAndroid */, mAccountTrackerService,
                        mIdentityManager, mIdentityMutator, androidSyncSettings, externalAuthUtils);

        mAccount = new CoreAccountInfo(
                new CoreAccountId("gaia-id-user"), "user@domain.com", "gaia-id-user");
    }

    @Test
    public void signInFromJava() {
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();
        doReturn(mAccount)
                .when(mIdentityManager)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        eq(mAccount.getEmail()));

        mSigninManager.onFirstRunCheckDone();
        mSigninManager.signinAndEnableSync(SigninAccessPoint.START_PAGE, mAccount, null);

        verify(mNativeMock).fetchAndApplyCloudPolicy(anyLong(), eq(mAccount), any());
    }

    @Test
    public void signOutFromJavaWithManagedDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mNativeMock).wipeProfileData(anyLong(), any());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // See verification of nativeWipeProfileData below.
        doReturn("TestDomain").when(mNativeMock).getManagementDomain(anyLong());

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountCleared should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountCleared(mAccount);

        // Sign-out should only clear the profile when the user is managed.
        verify(mNativeMock, times(1)).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutFromJavaWithNullDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mNativeMock).wipeProfileData(anyLong(), any());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // See verification of nativeWipeGoogleServiceWorkerCaches below.
        doReturn(null).when(mNativeMock).getManagementDomain(anyLong());

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountCleared should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountCleared(mAccount);

        // Sign-out should only clear the service worker cache when the user is not managed.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, times(1)).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutFromJavaWithNullDomainAndForceWipe() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mNativeMock).wipeProfileData(anyLong(), any());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // See verification of nativeWipeGoogleServiceWorkerCaches below.
        doReturn(null).when(mNativeMock).getManagementDomain(anyLong());

        // Trigger the sign out flow
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST, null, true);

        // PrimaryAccountCleared should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountCleared(mAccount);

        // Sign-out should only clear the service worker cache when the user is not managed.
        verify(mNativeMock, times(1)).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutFromNative() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mNativeMock).wipeProfileData(anyLong(), any());
        doNothing().when(mNativeMock).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Trigger the sign out flow!
        mIdentityManager.onPrimaryAccountCleared(mAccount);

        // Sign-out should only clear the profile when the user is managed.
        verify(mNativeMock, times(1)).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void callbackNotifiedWhenNoOperationIsInProgress() {
        assertFalse(mSigninManager.isOperationInProgress());

        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignout() {
        doAnswer(invocation -> {
            mIdentityManager.onPrimaryAccountCleared(mAccount);
            return null;
        })
                .when(mIdentityMutator)
                .clearPrimaryAccount(anyInt(), anyInt(), anyInt());

        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);
        assertTrue(mSigninManager.isOperationInProgress());
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.finishSignOut();
        assertFalse(mSigninManager.isOperationInProgress());
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignin() {
        CoreAccountInfo account = new CoreAccountInfo(
                new CoreAccountId("test_at_gmail.com"), "test@gmail.com", "test_at_gmail.com");

        // No need to seed accounts to the native code.
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();
        // Request that policy is loaded. It will pause sign-in until onPolicyCheckedBeforeSignIn is
        // invoked.
        doNothing().when(mNativeMock).fetchAndApplyCloudPolicy(anyLong(), any(), any());

        doReturn(account)
                .when(mIdentityManager)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(any());
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        Answer<Boolean> setPrimaryAccountAnswer = invocation -> {
            // From now on getPrimaryAccountInfo should return account.
            doReturn(account).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
            return true;
        };
        doAnswer(setPrimaryAccountAnswer).when(mIdentityMutator).setPrimaryAccount(account.getId());
        doNothing().when(mIdentityMutator).reloadAllAccountsFromSystemWithPrimaryAccount(any());

        mSigninManager.onFirstRunCheckDone(); // Allow sign-in.

        mSigninManager.signinAndEnableSync(SigninAccessPoint.UNKNOWN, account, null);
        assertTrue(mSigninManager.isOperationInProgress());
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.finishSignInAfterPolicyEnforced();
        assertFalse(mSigninManager.isOperationInProgress());
        assertEquals(1, callCount.get());
    }

    @Test(expected = AssertionError.class)
    public void failIfAlreadySignedin() {
        CoreAccountInfo account = new CoreAccountInfo(
                new CoreAccountId("test_at_gmail.com"), "test@gmail.com", "test_at_gmail.com");

        // No need to seed accounts to the native code.
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();
        // Request that policy is loaded. It will pause sign-in until onPolicyCheckedBeforeSignIn is
        // invoked.
        doNothing().when(mNativeMock).fetchAndApplyCloudPolicy(anyLong(), any(), any());

        doReturn(account)
                .when(mIdentityManager)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(any());
        doReturn(true).when(mIdentityManager).hasPrimaryAccount();

        mSigninManager.onFirstRunCheckDone(); // Allow sign-in.

        mSigninManager.signinAndEnableSync(SigninAccessPoint.UNKNOWN, account, null);
        assertTrue(mSigninManager.isOperationInProgress());

        // The following should throw an assertion error
        mSigninManager.finishSignInAfterPolicyEnforced();
    }
}
