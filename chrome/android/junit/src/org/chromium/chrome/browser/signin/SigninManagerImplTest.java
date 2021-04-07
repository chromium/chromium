// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.stubbing.Answer;

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
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;

import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.DisableFeatures(
        {ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY, ChromeFeatureList.DEPRECATE_MENAGERIE_API})
public class SigninManagerImplTest {
    private static final long NATIVE_SIGNIN_MANAGER = 10001L;
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;
    private static final AccountInfo ACCOUNT_INFO =
            new AccountInfo(new CoreAccountId("gaia-id-user"), "user@domain.com", "gaia-id-user",
                    "full name", "given name", null);

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Rule
    public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    private final SigninManagerImpl.Natives mNativeMock = mock(SigninManagerImpl.Natives.class);
    private final IdentityManager.Natives mIdentityManagerNativeMock =
            mock(IdentityManager.Natives.class);
    private final AccountTrackerService mAccountTrackerService = mock(AccountTrackerService.class);
    private final IdentityMutator mIdentityMutator = mock(IdentityMutator.class);
    private final AndroidSyncSettings mAndroidSyncSettings = mock(AndroidSyncSettings.class);
    private final ExternalAuthUtils mExternalAuthUtils = mock(ExternalAuthUtils.class);
    private final ProfileSyncService mProfileSyncService = mock(ProfileSyncService.class);

    private final IdentityManager mIdentityManager =
            IdentityManager.create(NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);
    private SigninManagerImpl mSigninManager;

    @Before
    public void setUp() {
        mocker.mock(SigninManagerImplJni.TEST_HOOKS, mNativeMock);
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        ProfileSyncService.overrideForTests(mProfileSyncService);
        AndroidSyncSettings.overrideForTests(mAndroidSyncSettings);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        when(mNativeMock.isSigninAllowedByPolicy(NATIVE_SIGNIN_MANAGER)).thenReturn(true);
        // Pretend Google Play services are available as it is required for the sign-in
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(false);
        // Suppose that the accounts are already seeded
        doAnswer(invocation -> {
            Runnable runnable = invocation.getArgument(0);
            runnable.run();
            return null;
        })
                .when(mAccountTrackerService)
                .seedAccountsIfNeeded(any(Runnable.class));
        when(mIdentityManagerNativeMock
                        .findExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
                                NATIVE_IDENTITY_MANAGER, ACCOUNT_INFO.getEmail()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager = (SigninManagerImpl) SigninManagerImpl.create(
                NATIVE_SIGNIN_MANAGER, mAccountTrackerService, mIdentityManager, mIdentityMutator);
    }

    @After
    public void tearDown() {
        mSigninManager.destroy();
    }

    @Test
    public void signinAndTurnSyncOn() {
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt())).thenReturn(true);

        mSigninManager.onFirstRunCheckDone();

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signinAndEnableSync(SigninAccessPoint.START_PAGE, ACCOUNT_INFO, callback);

        verify(mNativeMock)
                .fetchAndApplyCloudPolicy(eq(NATIVE_SIGNIN_MANAGER), eq(ACCOUNT_INFO), any());

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
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt())).thenReturn(true);

        mSigninManager.onFirstRunCheckDone();

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signin(ACCOUNT_INFO, callback);

        // Signin without turning on sync shouldn't apply policies.
        verify(mNativeMock, never()).fetchAndApplyCloudPolicy(anyLong(), any(), any());

        verify(mIdentityMutator).setPrimaryAccount(ACCOUNT_INFO.getId(), ConsentLevel.SIGNIN);

        verify(mAndroidSyncSettings, never()).updateAccount(any());
        verify(mAndroidSyncSettings, never()).enableChromeSync();
        // Signin should be complete and callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();
    }

    @Test
    public void signOutNonSyncingAccountFromJavaWithManagedDomain() {
        when(mNativeMock.getManagementDomain(NATIVE_SIGNIN_MANAGER)).thenReturn("TestDomain");

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountChanged should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));

        // Sign-out should only clear the profile when the user is managed.
        verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutSyncingAccountFromJavaWithManagedDomain() {
        when(mNativeMock.getManagementDomain(NATIVE_SIGNIN_MANAGER)).thenReturn("TestDomain");

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountChanged should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.CLEARED));

        // Sign-out should only clear the profile when the user is managed.
        verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutNonSyncingAccountFromJavaWithNullDomain() {
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountChanged should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));

        // Sign-out should only clear the service worker cache when the user is neither managed or
        // syncing.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    public void signOutSyncingAccountFromJavaWithNullDomain() {
        // Simulate sign-out with non-managed account.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountCleared should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.CLEARED));

        // Sign-out should only clear the service worker cache when the user has decided not to
        // wipe data.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    public void signOutSyncingAccountFromJavaWithNullDomainAndForceWipe() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST, null, true);

        // PrimaryAccountCleared should be called *before* clearing any account data.
        // http://crbug.com/589028
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        // Not possible to wipe data if sync account is not cleared.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.CLEARED));

        // Sign-out should only clear the profile when the user is syncing and has decided to
        // wipe data.
        verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutNonSyncingAccountFromNative() {
        // Simulate native initiating the sign-out.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));

        // Sign-out should only clear the service worker cache when the user is not syncing.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    public void signOutSyncingAccountFromNativeWithManagedDomain() {
        when(mNativeMock.getManagementDomain(NATIVE_SIGNIN_MANAGER)).thenReturn("TestDomain");

        // Simulate native initiating the sign-out.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.CLEARED));

        // Turning off sync should only clear the profile data when the account is managed.
        verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void signOutSyncingAccountFromNativeWithNullDomain() {
        // Simulate native initiating the sign-out.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.CLEARED));

        // Turning off sync should only clear service worker caches when the account is not managed.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    public void clearingAccountCookieDoesNotTriggerSignoutWhenUserIsSignedOut() {
        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void clearingAccountCookieDoesNotTriggerSignoutWhenUserIsSignedInAndSync() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void clearingAccountCookieTriggersSignoutWhenUserIsSignedInWithoutSync() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     NATIVE_IDENTITY_MANAGER, ConsentLevel.SIGNIN))
                .thenReturn(ACCOUNT_INFO);

        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator)
                .clearPrimaryAccount(
                        SignoutReason.USER_DELETED_ACCOUNT_COOKIES, SignoutDelete.IGNORE_METRIC);
        // Sign-out triggered by wiping account cookies shouldn't wipe data.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void rollbackWhenMobileIdentityConsistencyIsDisabled() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     NATIVE_IDENTITY_MANAGER, ConsentLevel.SIGNIN))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager = (SigninManagerImpl) SigninManagerImpl.create(
                NATIVE_SIGNIN_MANAGER, mAccountTrackerService, mIdentityManager, mIdentityMutator);

        // SignedIn state (without sync consent) doesn't exist pre-MobileIdentityConsistency. If the
        // feature is disabled while in this state, SigninManager ctor should trigger sign-out.
        verify(mIdentityMutator)
                .clearPrimaryAccount(SignoutReason.MOBILE_IDENTITY_CONSISTENCY_ROLLBACK,
                        SignoutDelete.IGNORE_METRIC);
        verify(mNativeMock)
                .logOutAllAccountsForMobileIdentityConsistencyRollback(NATIVE_SIGNIN_MANAGER);
        // This sign-out shouldn't wipe data.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void noRollbackWhenMobileIdentityConsistencyIsEnabled() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     NATIVE_IDENTITY_MANAGER, ConsentLevel.SIGNIN))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager = (SigninManagerImpl) SigninManagerImpl.create(
                NATIVE_SIGNIN_MANAGER, mAccountTrackerService, mIdentityManager, mIdentityMutator);
        ;

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt(), anyInt());
        verify(mNativeMock, never())
                .logOutAllAccountsForMobileIdentityConsistencyRollback(anyLong());
    }

    @Test
    public void callbackNotifiedWhenNoOperationIsInProgress() {
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

        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.finishSignOut();
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignin() {
        final Answer<Boolean> setPrimaryAccountAnswer = invocation -> {
            // From now on getPrimaryAccountInfo should return account.
            when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                         eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                    .thenReturn(ACCOUNT_INFO);
            return true;
        };
        doAnswer(setPrimaryAccountAnswer)
                .when(mIdentityMutator)
                .setPrimaryAccount(ACCOUNT_INFO.getId(), ConsentLevel.SYNC);

        mSigninManager.onFirstRunCheckDone(); // Allow sign-in.

        mSigninManager.signinAndEnableSync(SigninAccessPoint.UNKNOWN, ACCOUNT_INFO, null);

        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.finishSignInAfterPolicyEnforced();
        assertEquals(1, callCount.get());
    }

    @Test(expected = AssertionError.class)
    public void signinfailsWhenAlreadySignedIn() {
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        mSigninManager.signinAndEnableSync(SigninAccessPoint.UNKNOWN, ACCOUNT_INFO, null);
    }
}
