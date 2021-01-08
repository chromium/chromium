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
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountTrackerService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.DisableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
public class SigninManagerTest {
    @Rule
    public final JniMocker mocker = new JniMocker();
    @Rule
    public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    private static final AccountInfo ACCOUNT_INFO =
            new AccountInfo(new CoreAccountId("gaia-id-user"), "user@domain.com", "gaia-id-user",
                    "full name", "given name", null);

    private final SigninManagerImpl.Natives mNativeMock = mock(SigninManagerImpl.Natives.class);
    private final AccountTrackerService mAccountTrackerService = mock(AccountTrackerService.class);
    private final IdentityMutator mIdentityMutator = mock(IdentityMutator.class);
    private final AndroidSyncSettings mAndroidSyncSettings = mock(AndroidSyncSettings.class);
    private final ExternalAuthUtils mExternalAuthUtils = mock(ExternalAuthUtils.class);
    private final ProfileSyncService mProfileSyncService = mock(ProfileSyncService.class);
    private IdentityManager mIdentityManager;

    private SigninManagerImpl mSigninManager;

    @Before
    public void setUp() {
        mocker.mock(SigninManagerImplJni.TEST_HOOKS, mNativeMock);
        ProfileSyncService.overrideForTests(mProfileSyncService);
        doReturn(true).when(mNativeMock).isSigninAllowedByPolicy(anyLong());
        // Pretend Google Play services are available as it is required for the sign-in
        doReturn(false).when(mExternalAuthUtils).isGooglePlayServicesMissing(any());

        mIdentityManager = spy(
                new IdentityManager(0 /* nativeIdentityManager */, null /* OAuth2TokenService */));
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
    }

    @After
    public void tearDown() {
        if (mSigninManager != null) {
            mSigninManager.destroy();
            mSigninManager = null;
        }
    }

    private void createSigninManager() {
        mSigninManager = new SigninManagerImpl(0 /* nativeSigninManagerAndroid */,
                mAccountTrackerService, mIdentityManager, mIdentityMutator, mAndroidSyncSettings,
                mExternalAuthUtils);
    }

    @Test
    public void signinAndTurnSyncOn() {
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();
        doReturn(ACCOUNT_INFO)
                .when(mIdentityManager)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        eq(ACCOUNT_INFO.getEmail()));
        doReturn(true).when(mIdentityMutator).setPrimaryAccount(any(), anyInt());

        createSigninManager();
        mSigninManager.onFirstRunCheckDone();

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signinAndEnableSync(SigninAccessPoint.START_PAGE, ACCOUNT_INFO, callback);

        verify(mNativeMock).fetchAndApplyCloudPolicy(anyLong(), eq(ACCOUNT_INFO), any());

        mSigninManager.finishSignInAfterPolicyEnforced();
        verify(mIdentityMutator).setPrimaryAccount(ACCOUNT_INFO.getId(), ConsentLevel.SYNC);
        verify(mAndroidSyncSettings)
                .updateAccount(CoreAccountInfo.getAndroidAccountFrom(ACCOUNT_INFO));
        verify(mAndroidSyncSettings).enableChromeSync();
        // Signin should be complete and callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();
    }

    @Test
    public void signinNoTurnSyncOn() {
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();
        doReturn(ACCOUNT_INFO)
                .when(mIdentityManager)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                        eq(ACCOUNT_INFO.getEmail()));
        doReturn(true).when(mIdentityMutator).setPrimaryAccount(any(), anyInt());

        createSigninManager();
        mSigninManager.onFirstRunCheckDone();

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signin(ACCOUNT_INFO, callback);

        // Signin without turning on sync shouldn't apply policies.
        verify(mNativeMock, never()).fetchAndApplyCloudPolicy(anyLong(), any(), any());

        verify(mIdentityMutator).setPrimaryAccount(ACCOUNT_INFO.getId(), ConsentLevel.NOT_REQUIRED);

        verify(mAndroidSyncSettings, never()).updateAccount(any());
        verify(mAndroidSyncSettings, never()).enableChromeSync();
        // Signin should be complete and callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();
    }

    @Test
    public void signOutFromJavaWithManagedDomain() {
        // See verification of nativeWipeProfileData below.
        doReturn("TestDomain").when(mNativeMock).getManagementDomain(anyLong());

        createSigninManager();
        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountCleared should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));

        // Sign-out should only clear the profile when the user is managed.
        verify(mNativeMock, times(1)).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutFromJavaWithNullDomain() {
        // Simulate sign-out with non-managed account.
        doReturn(null).when(mNativeMock).getManagementDomain(anyLong());

        createSigninManager();
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountCleared should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));

        // Sign-out should only clear the service worker cache when the user is not managed.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, times(1)).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutFromJavaWithNullDomainAndForceWipe() {
        // See verification of nativeWipeGoogleServiceWorkerCaches below.
        doReturn(null).when(mNativeMock).getManagementDomain(anyLong());

        createSigninManager();
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST, null, true);

        // PrimaryAccountCleared should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));

        // Sign-out should only clear the service worker cache when the user is not managed.
        verify(mNativeMock, times(1)).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutFromNative() {
        createSigninManager();
        // Simulate native initiating the sign-out.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));

        // Sign-out should only clear the profile when the user is managed.
        verify(mNativeMock, times(1)).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void clearingAccountCookiesTriggersSignout() {
        // Create SigninManager so it adds an observer for onAccountsCookieDeletedByUserAction.
        createSigninManager();

        // Clearing cookies shouldn't do anything when there's no primary account.
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        mIdentityManager.onAccountsCookieDeletedByUserAction();
        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());

        // Clearing cookies shouldn't do anything when there's sync account.
        doReturn(ACCOUNT_INFO).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        mIdentityManager.onAccountsCookieDeletedByUserAction();
        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());

        // Clearing cookies when there's only an unconsented account should trigger sign-out.
        doReturn(ACCOUNT_INFO)
                .when(mIdentityManager)
                .getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED);
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(ConsentLevel.SYNC);
        mIdentityManager.onAccountsCookieDeletedByUserAction();
        verify(mIdentityMutator)
                .clearPrimaryAccount(
                        SignoutReason.USER_DELETED_ACCOUNT_COOKIES, SignoutDelete.IGNORE_METRIC);

        // Sign-out triggered by wiping account cookies shouldn't wipe data.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void testRollbackForMobileIdentityConsitency() {
        doReturn(ACCOUNT_INFO)
                .when(mIdentityManager)
                .getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED);
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(ConsentLevel.SYNC);
        mIdentityManager.onAccountsCookieDeletedByUserAction();

        // SignedIn state (without sync consent) doesn't exist pre-MobileIdentityConsistency. If the
        // feature is disabled while in this state, SigninManager ctor should trigger sign-out.
        createSigninManager();

        verify(mIdentityMutator)
                .clearPrimaryAccount(SignoutReason.MOBILE_IDENTITY_CONSISTENCY_ROLLBACK,
                        SignoutDelete.IGNORE_METRIC);
        verify(mNativeMock).logOutAllAccountsForMobileIdentityConsistencyRollback(anyLong());

        // This sign-out shouldn't wipe data.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testNoRollbackIfMobileIdentityConsitencyIsEnabled() {
        doReturn(ACCOUNT_INFO)
                .when(mIdentityManager)
                .getPrimaryAccountInfo(ConsentLevel.NOT_REQUIRED);
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(ConsentLevel.SYNC);
        mIdentityManager.onAccountsCookieDeletedByUserAction();

        createSigninManager();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());
        verify(mNativeMock, never())
                .logOutAllAccountsForMobileIdentityConsistencyRollback(anyLong());
    }

    @Test
    public void callbackNotifiedWhenNoOperationIsInProgress() {
        createSigninManager();
        assertFalse(mSigninManager.isOperationInProgress());

        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignout() {
        doAnswer(invocation -> {
            mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                    PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));
            return null;
        })
                .when(mIdentityMutator)
                .clearPrimaryAccount(anyInt(), anyInt());

        createSigninManager();
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
        AccountInfo account = new AccountInfo(new CoreAccountId("test_at_gmail.com"),
                "test@gmail.com", "test_at_gmail.com", "full name", "given name", null);

        // No need to seed accounts to the native code.
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();

        doReturn(account)
                .when(mIdentityManager)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(any());
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        Answer<Boolean> setPrimaryAccountAnswer = invocation -> {
            // From now on getPrimaryAccountInfo should return account.
            doReturn(account).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
            return true;
        };
        doAnswer(setPrimaryAccountAnswer)
                .when(mIdentityMutator)
                .setPrimaryAccount(account.getId(), ConsentLevel.SYNC);

        createSigninManager();
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
        AccountInfo account = new AccountInfo(new CoreAccountId("test_at_gmail.com"),
                "test@gmail.com", "test_at_gmail.com", "full name", "given name", null);

        // No need to seed accounts to the native code.
        doReturn(true).when(mAccountTrackerService).checkAndSeedSystemAccounts();

        doReturn(account)
                .when(mIdentityManager)
                .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(any());
        doReturn(true).when(mIdentityManager).hasPrimaryAccount();

        createSigninManager();
        mSigninManager.onFirstRunCheckDone(); // Allow sign-in.

        mSigninManager.signinAndEnableSync(SigninAccessPoint.UNKNOWN, account, null);
        assertTrue(mSigninManager.isOperationInProgress());

        // The following should throw an assertion error
        mSigninManager.finishSignInAfterPolicyEnforced();
    }
}
