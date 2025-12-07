// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridgeJni;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.prefs.PrefChangeRegistrar;
import org.chromium.components.prefs.PrefChangeRegistrarJni;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountError;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeIdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;

import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
@EnableFeatures({
    SigninFeatures.SKIP_CHECK_FOR_ACCOUNT_MANAGEMENT_ON_SIGNIN,
    SigninFeatures.MAKE_ACCOUNTS_AVAILABLE_IN_IDENTITY_MANAGER,
})
public class SigninManagerImplTest {
    private static final long NATIVE_SIGNIN_MANAGER = 10001L;
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;

    @Rule public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private SigninManagerImpl.Natives mNativeMock;
    @Mock private BrowsingDataBridge.Natives mBrowsingDataBridgeNativeMock;
    @Mock private PrefChangeRegistrar.Natives mPrefChangeRegistrarNativeMock;
    @Mock private PrefService mPrefService;
    @Mock private IdentityMutator mIdentityMutator;
    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private Profile mProfile;
    @Mock private SigninManager.SignInStateObserver mSignInStateObserver;

    private final FakeIdentityManager mIdentityManager = new FakeIdentityManager();
    private SigninManagerImpl mSigninManager;

    @Before
    public void setUp() {
        SigninManagerImplJni.setInstanceForTesting(mNativeMock);
        BrowsingDataBridgeJni.setInstanceForTesting(mBrowsingDataBridgeNativeMock);
        PrefChangeRegistrarJni.setInstanceForTesting(mPrefChangeRegistrarNativeMock);

        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        BookmarkModel.setInstanceForTesting(FakeBookmarkModel.createModel());

        when(mPrefService.getBoolean(Pref.SIGNIN_ALLOWED)).thenReturn(true);
        // Pretend Google Play services are available as it is required for the sign-in
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(false);
        // Suppose that the accounts are already seeded
        mIdentityManager.addOrUpdateExtendedAccountInfo(TestAccounts.ACCOUNT1);
        mIdentityManager.setIsClearPrimaryAccountAllowed(true);
    }

    @After
    public void tearDown() {
        mSigninManager.removeSignInStateObserver(mSignInStateObserver);
        mSigninManager.destroy();
        AccountInfoServiceProvider.resetForTests();
    }

    @Test
    public void testSeedAccountsCalled_accountFetchSucceeded() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        createSigninManager();

        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                        List.of(TestAccounts.ACCOUNT1), null);
    }

    @Test
    public void testSeedAccountsCalled_accountFetchFailed_accountListPopulated() {
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        mAccountManagerTestRule.setAccountFetchFailed();

        createSigninManager();

        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                        List.of(TestAccounts.ACCOUNT1), null);
    }

    @Test
    public void testSeedAccountsCalled_accountFetchFailed_accountListEmpty() {
        mAccountManagerTestRule.setAccountFetchFailed();

        createSigninManager();

        verify(mIdentityMutator, never())
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(any(), any());
    }

    @Test
    public void testOnAccountsChanged() {
        createSigninManager();
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);

        List<AccountInfo> accounts =
                AccountManagerFacadeProvider.getInstance().getAccounts().getResult();
        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(accounts, null);
    }

    @Test
    public void testSignin() {
        createSigninManager();
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt(), anyInt(), any()))
                .thenReturn(PrimaryAccountError.NO_ERROR);

        when(mNativeMock.getUserAcceptedAccountManagement(anyLong())).thenReturn(true);

        doAnswer(
                        (args) -> {
                            ((Runnable) args.getArgument(2)).run();
                            return null;
                        })
                .when(mNativeMock)
                .fetchAndApplyCloudPolicy(anyLong(), any(), any());

        assertTrue(mSigninManager.isSigninAllowed());

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signin(TestAccounts.ACCOUNT1, SigninAccessPoint.START_PAGE, callback);

        // Signin without turning on sync should still apply policies.
        verify(mNativeMock).fetchAndApplyCloudPolicy(anyLong(), any(), any());
        verify(mIdentityMutator)
                .setPrimaryAccount(
                        eq(TestAccounts.ACCOUNT1.getId()),
                        eq(ConsentLevel.SIGNIN),
                        eq(SigninAccessPoint.START_PAGE),
                        any());

        // Signin should be complete and callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();

        // The primary account is now present and consented to sign in.
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
        assertFalse(mSigninManager.isSigninAllowed());
    }

    @Test
    public void signOutSignedInAccountFromJavaWithManagedDomain() {
        createSigninManager();
        when(mNativeMock.getManagementDomain(NATIVE_SIGNIN_MANAGER)).thenReturn("TestDomain");

        // Trigger the sign out flow!
        mSigninManager.signOut(SignoutReason.TEST);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST));
        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(List.of(), null);

        // Sign-out should only clear the profile when the user is managed.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        inOrder.verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    public void signOutNonSyncingAccountFromJavaWithNullDomain() {
        createSigninManager();
        mSigninManager.signOut(SignoutReason.TEST);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST));
        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(List.of(), null);

        // Sign-out should only clear the service worker cache when the user is neither managed or
        // syncing.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        inOrder.verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    public void signOutSyncingAccountFromJavaWithNullDomain() {
        createSigninManager();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);

        // Simulate sign-out with non-managed account.
        mSigninManager.signOut(SignoutReason.TEST);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST));
        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(List.of(), null);

        // Sign-out should only clear the service worker cache when the user has decided not to
        // wipe data.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        inOrder.verify(mNativeMock).wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), any());
    }

    @Test
    public void syncPromoShowCountResetWhenSignOutSyncingAccount() {
        createSigninManager();
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId.NTP),
                        1);
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);

        // Simulate sign-out with non-managed account.
        mSigninManager.signOut(SignoutReason.TEST);

        ArgumentCaptor<Runnable> callback = ArgumentCaptor.forClass(Runnable.class);
        verify(mNativeMock)
                .wipeGoogleServiceWorkerCaches(eq(NATIVE_SIGNIN_MANAGER), callback.capture());
        assertNotNull(callback.getValue());

        callback.getValue().run();
        assertEquals(
                0,
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                        SigninPreferencesManager.SigninPromoAccessPointId.NTP)));
    }

    @Test
    public void signOutSyncingAccountFromJavaWithNullDomainAndForceWipe() {
        createSigninManager();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);

        mSigninManager.signOut(SignoutReason.TEST, null, true);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).clearPrimaryAccount(eq(SignoutReason.TEST));

        // Sign-out should only clear the profile when the user is syncing and has decided to
        // wipe data.
        inOrder.verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    // TODO(crbug.com/40820738): add test for revokeSyncConsentFromJavaWithManagedDomain() and
    // revokeSyncConsentFromJavaWipeData() - this requires making the BookmarkModel mockable in
    // SigninManagerImpl.

    @Test
    public void revokeSyncConsentFromJavaWithNullDomain() {
        createSigninManager();
        SigninManager.SignOutCallback callback = mock(SigninManager.SignOutCallback.class);
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);

        mSigninManager.revokeSyncConsent(SignoutReason.TEST, callback, false);

        // The primary account should be cleared *before* clearing any account data.
        // For more information see crbug.com/589028.
        InOrder inOrder = inOrder(mNativeMock, mIdentityMutator);
        inOrder.verify(mIdentityMutator).revokeSyncConsent(eq(SignoutReason.TEST));

        // Disabling sync should only clear the service worker cache when the user is neither
        // managed or syncing.
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        inOrder.verify(mNativeMock).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void revokeSyncConsentFromJavaWithNullDomainAndWipeData() {
        createSigninManager();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);

        mSigninManager.revokeSyncConsent(
                SignoutReason.TEST,
                mock(SigninManager.SignOutCallback.class),
                /* forceWipeUserData= */ true);

        // Passwords should not be among the cleared types.
        int[] expectedClearedTypes =
                new int[] {
                    BrowsingDataType.HISTORY,
                    BrowsingDataType.CACHE,
                    BrowsingDataType.SITE_DATA,
                    BrowsingDataType.FORM_DATA,
                };
        verify(mBrowsingDataBridgeNativeMock)
                .clearBrowsingData(
                        any(),
                        any(),
                        eq(expectedClearedTypes),
                        eq(TimePeriod.ALL_TIME),
                        any(),
                        any(),
                        any(),
                        any());
    }

    @Test
    public void wipeSyncDataOnly() {
        createSigninManager();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);

        mSigninManager.wipeSyncUserData(
                CallbackUtils.emptyRunnable(), SigninManager.DataWipeOption.WIPE_SYNC_DATA);

        // Passwords should not be among the cleared types.
        int[] expectedClearedTypes =
                new int[] {
                    BrowsingDataType.HISTORY,
                    BrowsingDataType.CACHE,
                    BrowsingDataType.SITE_DATA,
                    BrowsingDataType.FORM_DATA,
                };
        verify(mBrowsingDataBridgeNativeMock)
                .clearBrowsingData(
                        any(),
                        any(),
                        eq(expectedClearedTypes),
                        eq(TimePeriod.ALL_TIME),
                        any(),
                        any(),
                        any(),
                        any());
    }

    @Test
    public void callbackNotifiedWhenNoOperationIsInProgress() {
        createSigninManager();
        AtomicInteger callCount = new AtomicInteger(0);

        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignout() {
        createSigninManager();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
        doAnswer(
                        invocation -> {
                            // From now on getPrimaryAccountInfo should return null.
                            mIdentityManager.setPrimaryAccount(null);
                            return null;
                        })
                .when(mIdentityMutator)
                .clearPrimaryAccount(anyInt());

        mSigninManager.signOut(SignoutReason.TEST);
        AtomicInteger callCount = new AtomicInteger(0);
        mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
        assertEquals(0, callCount.get());

        mSigninManager.finishSignOut();
        assertEquals(1, callCount.get());
    }

    @Test
    public void callbackNotifiedOnSignin() {
        createSigninManager();
        when(mNativeMock.getUserAcceptedAccountManagement(anyLong())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        final Answer<Integer> setPrimaryAccountAnswer =
                invocation -> {
                    // From now on getPrimaryAccountInfo should return account.
                    mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
                    return PrimaryAccountError.NO_ERROR;
                };
        doAnswer(setPrimaryAccountAnswer)
                .when(mIdentityMutator)
                .setPrimaryAccount(
                        eq(TestAccounts.ACCOUNT1.getId()),
                        eq(ConsentLevel.SYNC),
                        eq(SigninAccessPoint.UNKNOWN),
                        any());

        AtomicInteger callCount = new AtomicInteger(0);
        doAnswer(
                        (args) -> {
                            mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
                            assertEquals(0, callCount.get());

                            ((Runnable) args.getArgument(2)).run();
                            return null;
                        })
                .when(mNativeMock)
                .fetchAndApplyCloudPolicy(anyLong(), any(), any());

        mSigninManager.signin(TestAccounts.ACCOUNT1, SigninAccessPoint.UNKNOWN, null);
        assertEquals(1, callCount.get());
    }

    @Test
    public void signInStateObserverCallOnSignIn() {
        createSigninManager();
        when(mNativeMock.getUserAcceptedAccountManagement(anyLong())).thenReturn(true);
        mAccountManagerTestRule.addAccount(TestAccounts.ACCOUNT1);
        final Answer<Integer> setPrimaryAccountAnswer =
                invocation -> {
                    // From now on getPrimaryAccountInfo should return account.
                    mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
                    return PrimaryAccountError.NO_ERROR;
                };
        doAnswer(setPrimaryAccountAnswer)
                .when(mIdentityMutator)
                .setPrimaryAccount(
                        eq(TestAccounts.ACCOUNT1.getId()),
                        eq(ConsentLevel.SIGNIN),
                        eq(SigninAccessPoint.START_PAGE),
                        any());

        doAnswer(
                        (args) -> {
                            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
                            verify(mSignInStateObserver).onSignInAllowedChanged();

                            ((Runnable) args.getArgument(2)).run();
                            return null;
                        })
                .when(mNativeMock)
                .fetchAndApplyCloudPolicy(anyLong(), any(), any());

        mSigninManager.signin(TestAccounts.ACCOUNT1, SigninAccessPoint.START_PAGE, null);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSignInStateObserver).onSignOutAllowedChanged();
        assertFalse(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signInStateObserverCallOnSignOut() {
        createSigninManager();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
        assertTrue(mSigninManager.isSignOutAllowed());

        mSigninManager.signOut(SignoutReason.TEST);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSignInStateObserver).onSignOutAllowedChanged();
        assertFalse(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signOutNotAllowedForChildAccounts() {
        createSigninManager();
        mIdentityManager.setPrimaryAccount(TestAccounts.ACCOUNT1);
        mIdentityManager.setIsClearPrimaryAccountAllowed(false);

        assertFalse(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signInShouldBeSupportedForNonDemoUsers() {
        createSigninManager();
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(true);

        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true));
        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ false));
    }

    @Test
    public void signInShouldNotBeSupportedWhenGooglePlayServicesIsRequiredAndNotAvailable() {
        createSigninManager();
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(false);

        assertFalse(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true));
    }

    private void createSigninManager() {
        mSigninManager =
                (SigninManagerImpl)
                        SigninManagerImpl.create(
                                NATIVE_SIGNIN_MANAGER,
                                mProfile,
                                mPrefService,
                                mIdentityManager,
                                mIdentityMutator);
        mSigninManager.addSignInStateObserver(mSignInStateObserver);
    }
}
