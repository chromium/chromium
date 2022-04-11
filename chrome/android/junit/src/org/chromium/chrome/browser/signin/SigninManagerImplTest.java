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
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.AccountTrackerService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutDelete;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.sync.ModelType;

import java.util.HashMap;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
public class SigninManagerImplTest {
    private static final long NATIVE_SIGNIN_MANAGER = 10001L;
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;
    private static final AccountInfo ACCOUNT_INFO =
            new AccountInfo(new CoreAccountId("gaia-id-user"), "user@domain.com", "gaia-id-user",
                    "full name", "given name", null, new AccountCapabilities(new HashMap<>()));

    @Rule
    public final JniMocker mocker = new JniMocker();

    private final SigninManagerImpl.Natives mNativeMock = mock(SigninManagerImpl.Natives.class);
    private final IdentityManager.Natives mIdentityManagerNativeMock =
            mock(IdentityManager.Natives.class);
    private final AccountTrackerService mAccountTrackerService = mock(AccountTrackerService.class);
    private final IdentityMutator mIdentityMutator = mock(IdentityMutator.class);
    private final ExternalAuthUtils mExternalAuthUtils = mock(ExternalAuthUtils.class);
    private final SyncService mSyncService = mock(SyncService.class);

    private final IdentityManager mIdentityManager =
            IdentityManager.create(NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);
    private SigninManagerImpl mSigninManager;

    @Before
    public void setUp() {
        mocker.mock(SigninManagerImplJni.TEST_HOOKS, mNativeMock);
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        SyncService.overrideForTests(mSyncService);
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
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                     NATIVE_IDENTITY_MANAGER, ACCOUNT_INFO.getEmail()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager = (SigninManagerImpl) SigninManagerImpl.create(
                NATIVE_SIGNIN_MANAGER, mAccountTrackerService, mIdentityManager, mIdentityMutator);
    }

    @After
    public void tearDown() {
        mSigninManager.destroy();
        AccountInfoServiceProvider.resetForTests();
    }

    @Test
    public void signinAndTurnSyncOn() {
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt())).thenReturn(true);
        when(mSyncService.getChosenDataTypes()).thenReturn(Set.of(ModelType.BOOKMARKS));

        // Until the first run check is done, we do not allow sign in.
        assertFalse(mSigninManager.isSigninAllowed());
        assertFalse(mSigninManager.isSyncOptInAllowed());

        // First run check is complete and there is no signed in account.  Sign in is allowed.
        mSigninManager.onFirstRunCheckDone();
        assertTrue(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSyncOptInAllowed());

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signinAndEnableSync(SigninAccessPoint.START_PAGE,
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()), callback);

        verify(mNativeMock)
                .fetchAndApplyCloudPolicy(eq(NATIVE_SIGNIN_MANAGER), eq(ACCOUNT_INFO), any());
        // A sign in operation is in progress, so we do not allow a new one to be started.
        assertFalse(mSigninManager.isSigninAllowed());
        assertFalse(mSigninManager.isSyncOptInAllowed());

        mSigninManager.finishSignInAfterPolicyEnforced();
        verify(mIdentityMutator).setPrimaryAccount(ACCOUNT_INFO.getId(), ConsentLevel.SYNC);
        verify(mSyncService).setSyncRequested(true);
        // Signin should be complete and callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();

        // The primary account is now present and consented to sign in and sync.  We do not allow
        // another account to be signed in.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        assertFalse(mSigninManager.isSigninAllowed());
        assertFalse(mSigninManager.isSyncOptInAllowed());
    }

    @Test
    public void signinNoTurnSyncOn() {
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt())).thenReturn(true);

        assertFalse(mSigninManager.isSigninAllowed());
        assertFalse(mSigninManager.isSyncOptInAllowed());

        mSigninManager.onFirstRunCheckDone();
        assertTrue(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSyncOptInAllowed());

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signin(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()), callback);

        // Signin without turning on sync shouldn't apply policies.
        verify(mNativeMock, never()).fetchAndApplyCloudPolicy(anyLong(), any(), any());

        verify(mIdentityMutator).setPrimaryAccount(ACCOUNT_INFO.getId(), ConsentLevel.SIGNIN);

        verify(mSyncService, never()).setSyncRequested(true);
        // Signin should be complete and callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();

        // The primary account is now present and consented to sign in.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     eq(NATIVE_IDENTITY_MANAGER), eq(ConsentLevel.SIGNIN)))
                .thenReturn(ACCOUNT_INFO);
        assertFalse(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSyncOptInAllowed());
    }

    @Test
    public void signOutNonSyncingAccountFromJavaWithManagedDomain() {
        when(mNativeMock.getManagementDomain(NATIVE_SIGNIN_MANAGER)).thenReturn("TestDomain");

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.SIGNOUT_TEST);

        // PrimaryAccountChanged should be called *before* clearing any account data.
        // For more information see crbug.com/589028.
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
        // For more information see crbug.com/589028.
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
        // For more information see crbug.com/589028.
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
        // For more information see crbug.com/589028.
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
        // For more information see crbug.com/589028.
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

    // TODO(crbug.com/1294761): add test for revokeSyncConsentFromJavaWithManagedDomain() and
    // revokeSyncConsentFromJavaWipeData() - this requires making the BookmarkModel mockable in
    // SigninManagerImpl.

    @Test
    public void revokeSyncConsentFromJavaWithNullDomain() {
        SigninManager.SignOutCallback callback = mock(SigninManager.SignOutCallback.class);
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                     eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.revokeSyncConsent(SignoutReason.SIGNOUT_TEST, callback, false);

        // PrimaryAccountChanged should be called *before* clearing any account data.
        // For more information see crbug.com/589028.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());

        // Simulate native callback to trigger clearing of account data.
        mIdentityManager.onPrimaryAccountChanged(new PrimaryAccountChangeEvent(
                PrimaryAccountChangeEvent.Type.CLEARED, PrimaryAccountChangeEvent.Type.NONE));

        // Disabling sync should only clear the service worker cache when the user is neither
        // managed or syncing.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock).wipeGoogleServiceWorkerCaches(anyLong(), any());
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

        mSigninManager.signinAndEnableSync(SigninAccessPoint.UNKNOWN,
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()), null);

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
        mSigninManager.signinAndEnableSync(SigninAccessPoint.UNKNOWN,
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()), null);
    }
}
