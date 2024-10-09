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

import android.content.Context;
import android.os.UserManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.FakeBookmarkModel;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridgeJni;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
import org.chromium.components.signin.identitymanager.IdentityMutator;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.identitymanager.PrimaryAccountError;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.List;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/** Tests for {@link SigninManagerImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
@EnableFeatures({
    SigninFeatures.USE_CONSENT_LEVEL_SIGNIN_FOR_LEGACY_ACCOUNT_EMAIL_PREF,
    SigninFeatures.SKIP_CHECK_FOR_ACCOUNT_MANAGEMENT_ON_SIGNIN
})
public class SigninManagerImplTest {
    private static final long NATIVE_SIGNIN_MANAGER = 10001L;
    private static final long NATIVE_IDENTITY_MANAGER = 10002L;
    private static final AccountInfo ACCOUNT_INFO =
            new AccountInfo.Builder(
                            "user@domain.com", FakeAccountManagerFacade.toGaiaId("user@domain.com"))
                    .fullName("full name")
                    .givenName("given name")
                    .build();

    @Rule public final JniMocker mocker = new JniMocker();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private SigninManagerImpl.Natives mNativeMock;
    @Mock private IdentityManager.Natives mIdentityManagerNativeMock;
    @Mock private BrowsingDataBridge.Natives mBrowsingDataBridgeNativeMock;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeNativeMock;
    @Mock private UserPrefs.Natives mUserPrefsNativeMock;
    @Mock private PrefService mPrefService;
    @Mock private IdentityMutator mIdentityMutator;
    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private SyncService mSyncService;
    @Mock private Profile mProfile;
    @Mock private SigninManager.SignInStateObserver mSignInStateObserver;

    private final IdentityManager mIdentityManager =
            IdentityManager.create(NATIVE_IDENTITY_MANAGER, null /* OAuth2TokenService */);
    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            new FakeAccountManagerFacade();
    private SigninManagerImpl mSigninManager;

    @Before
    public void setUp() {
        mocker.mock(SigninManagerImplJni.TEST_HOOKS, mNativeMock);
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        mocker.mock(BrowsingDataBridgeJni.TEST_HOOKS, mBrowsingDataBridgeNativeMock);
        mocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeNativeMock);
        mocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsNativeMock);
        when(mUserPrefsNativeMock.get(mProfile)).thenReturn(mPrefService);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        BookmarkModel.setInstanceForTesting(FakeBookmarkModel.createModel());

        when(mNativeMock.isSigninAllowedByPolicy(NATIVE_SIGNIN_MANAGER)).thenReturn(true);
        // Pretend Google Play services are available as it is required for the sign-in
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(false);
        // Suppose that the accounts are already seeded
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        NATIVE_IDENTITY_MANAGER, ACCOUNT_INFO.getEmail()))
                .thenReturn(ACCOUNT_INFO);
        when(mIdentityManagerNativeMock.isClearPrimaryAccountAllowed(NATIVE_IDENTITY_MANAGER))
                .thenReturn(true);

        AccountManagerFacadeProvider.setInstanceForTests(mFakeAccountManagerFacade);
    }

    @After
    public void tearDown() {
        mSigninManager.removeSignInStateObserver(mSignInStateObserver);
        mSigninManager.destroy();
        AccountInfoServiceProvider.resetForTests();
    }

    @Test
    public void testAccountManagerFacadeObserverAddedOnCreate_accountFetchSucceeded() {
        CoreAccountInfo coreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId("email@domain.com", "gaia-id");
        AccountManagerFacade accountManagerFacadeMock = Mockito.mock(AccountManagerFacade.class);
        when(accountManagerFacadeMock.getCoreAccountInfos())
                .thenReturn(Promise.fulfilled(List.of(coreAccountInfo)));
        when(accountManagerFacadeMock.didAccountFetchSucceed()).thenReturn(true);
        AccountManagerFacadeProvider.setInstanceForTests(accountManagerFacadeMock);

        createSigninManager();

        verify(accountManagerFacadeMock).addObserver(mSigninManager);
        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                        List.of(coreAccountInfo), null);
    }

    @Test
    public void
            testAccountManagerFacadeObserverAddedOnCreate_accountFetchFailed_accountListPopulated() {
        CoreAccountInfo coreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId("email@domain.com", "gaia-id");
        AccountManagerFacade accountManagerFacadeMock = Mockito.mock(AccountManagerFacade.class);
        when(accountManagerFacadeMock.getCoreAccountInfos())
                .thenReturn(Promise.fulfilled(List.of(coreAccountInfo)));
        when(accountManagerFacadeMock.didAccountFetchSucceed()).thenReturn(false);
        AccountManagerFacadeProvider.setInstanceForTests(accountManagerFacadeMock);

        createSigninManager();

        verify(accountManagerFacadeMock).addObserver(mSigninManager);
        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                        List.of(coreAccountInfo), null);
    }

    @Test
    public void
            testAccountManagerFacadeObserverAddedOnCreate_accountFetchFailed_accountListEmpty() {
        AccountManagerFacade accountManagerFacadeMock = Mockito.mock(AccountManagerFacade.class);
        when(accountManagerFacadeMock.getCoreAccountInfos())
                .thenReturn(Promise.fulfilled(List.of()));
        when(accountManagerFacadeMock.didAccountFetchSucceed()).thenReturn(false);
        AccountManagerFacadeProvider.setInstanceForTests(accountManagerFacadeMock);

        createSigninManager();

        verify(accountManagerFacadeMock).addObserver(mSigninManager);
        verify(mIdentityMutator, never())
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(any(), any());
    }

    @Test
    public void testOnCoreAccountInfosChanged() {
        createSigninManager();
        mFakeAccountManagerFacade.addAccount(ACCOUNT_INFO);

        List<CoreAccountInfo> coreAccountInfos =
                mFakeAccountManagerFacade.getCoreAccountInfos().getResult();
        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(coreAccountInfos, null);
    }

    @Test
    public void signinAndTurnSyncOn() {
        createSigninManager();
        when(mNativeMock.getUserAcceptedAccountManagement(anyLong())).thenReturn(true);
        mFakeAccountManagerFacade.addAccount(ACCOUNT_INFO);
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt(), anyInt(), any()))
                .thenReturn(PrimaryAccountError.NO_ERROR);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.BOOKMARKS));

        // There is no signed in account. Sign in is allowed.
        assertTrue(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSyncOptInAllowed());
        // Sign out is not allowed.
        assertFalse(mSigninManager.isSignOutAllowed());

        List<CoreAccountInfo> coreAccountInfos =
                mFakeAccountManagerFacade.getCoreAccountInfos().getResult();
        CoreAccountInfo primaryAccountInfo =
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, ACCOUNT_INFO.getEmail());

        doAnswer(
                        (args) -> {
                            // A sign in operation is in progress, so we do not allow a new sign
                            // in/out operation.
                            assertFalse(mSigninManager.isSigninAllowed());
                            assertFalse(mSigninManager.isSyncOptInAllowed());
                            assertFalse(mSigninManager.isSignOutAllowed());

                            ((Runnable) args.getArgument(2)).run();
                            return null;
                        })
                .when(mNativeMock)
                .fetchAndApplyCloudPolicy(eq(NATIVE_SIGNIN_MANAGER), eq(primaryAccountInfo), any());

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signinAndEnableSync(ACCOUNT_INFO, SigninAccessPoint.START_PAGE, callback);

        verify(mIdentityMutator)
                .seedAccountsThenReloadAllAccountsWithPrimaryAccount(
                        coreAccountInfos, primaryAccountInfo.getId());
        verify(mIdentityMutator)
                .setPrimaryAccount(
                        eq(primaryAccountInfo.getId()),
                        eq(ConsentLevel.SYNC),
                        eq(SigninAccessPoint.START_PAGE),
                        any());
        verify(mSyncService).setSyncRequested();
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
        // Signing out is allowed.
        assertTrue(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signinNoTurnSyncOn() {
        createSigninManager();
        when(mIdentityMutator.setPrimaryAccount(any(), anyInt(), anyInt(), any()))
                .thenReturn(PrimaryAccountError.NO_ERROR);

        when(mNativeMock.getUserAcceptedAccountManagement(anyLong())).thenReturn(true);

        doAnswer(
                        (args) -> {
                            ((Callback<Boolean>) args.getArgument(2)).onResult(true);
                            return null;
                        })
                .when(mNativeMock)
                .isAccountManaged(anyLong(), any(), any());

        doAnswer(
                        (args) -> {
                            ((Runnable) args.getArgument(2)).run();
                            return null;
                        })
                .when(mNativeMock)
                .fetchAndApplyCloudPolicy(anyLong(), any(), any());

        assertTrue(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSyncOptInAllowed());

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        mSigninManager.signin(ACCOUNT_INFO, SigninAccessPoint.START_PAGE, callback);

        // Signin without turning on sync should still apply policies.
        verify(mNativeMock).fetchAndApplyCloudPolicy(anyLong(), any(), any());
        verify(mIdentityMutator)
                .setPrimaryAccount(
                        eq(ACCOUNT_INFO.getId()),
                        eq(ConsentLevel.SIGNIN),
                        eq(SigninAccessPoint.START_PAGE),
                        any());

        verify(mSyncService, never()).setSyncRequested();
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
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void signOutNonSyncingAccountFromJavaWithManagedDomain() {
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
        inOrder.verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void signOutSyncingAccountFromJavaWithManagedDomain() {
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
        inOrder.verify(mNativeMock).wipeProfileData(eq(NATIVE_SIGNIN_MANAGER), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
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
        // Simulate sign-out with non-managed account.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

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
                                SigninPreferencesManager.SyncPromoAccessPointId.NTP),
                        1);

        // Simulate sign-out with non-managed account.
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

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
                                        SigninPreferencesManager.SyncPromoAccessPointId.NTP)));
    }

    @Test
    public void signOutSyncingAccountFromJavaWithNullDomainAndForceWipe() {
        createSigninManager();
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

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
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

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
    public void revokeSyncConsentFromJavaWithNullDomainAndWipeData_noLocalUpm() {
        createSigninManager();
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                .thenReturn(false);
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.revokeSyncConsent(
                SignoutReason.TEST,
                mock(SigninManager.SignOutCallback.class),
                /* forceWipeUserData= */ true);

        // Passwords should be among the cleared types.
        int[] expectedClearedTypes =
                new int[] {
                    BrowsingDataType.HISTORY,
                    BrowsingDataType.CACHE,
                    BrowsingDataType.SITE_DATA,
                    BrowsingDataType.FORM_DATA,
                    BrowsingDataType.PASSWORDS
                };
        verify(mBrowsingDataBridgeNativeMock)
                .clearBrowsingData(
                        any(),
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
    public void revokeSyncConsentFromJavaWithNullDomainAndWipeData_withLocalUpm() {
        createSigninManager();
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                .thenReturn(true);
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

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
                        any(),
                        eq(expectedClearedTypes),
                        eq(TimePeriod.ALL_TIME),
                        any(),
                        any(),
                        any(),
                        any());
    }

    @Test
    public void wipeSyncDataOnly_noLocalUpm() {
        createSigninManager();
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                .thenReturn(false);
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

        mSigninManager.wipeSyncUserData(
                CallbackUtils.emptyRunnable(), SigninManager.DataWipeOption.WIPE_SYNC_DATA);

        // Passwords should be among the cleared types.
        int[] expectedClearedTypes =
                new int[] {
                    BrowsingDataType.HISTORY,
                    BrowsingDataType.CACHE,
                    BrowsingDataType.SITE_DATA,
                    BrowsingDataType.FORM_DATA,
                    BrowsingDataType.PASSWORDS
                };
        verify(mBrowsingDataBridgeNativeMock)
                .clearBrowsingData(
                        any(),
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
    public void wipeSyncDataOnly_withLocalUpm() {
        createSigninManager();
        when(mPasswordManagerUtilBridgeNativeMock.usesSplitStoresAndUPMForLocal(mPrefService))
                .thenReturn(true);
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);

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
                        any(),
                        eq(expectedClearedTypes),
                        eq(TimePeriod.ALL_TIME),
                        any(),
                        any(),
                        any(),
                        any());
    }

    @Test
    public void clearingAccountCookieDoesNotTriggerSignoutWhenUserIsSignedOut() {
        createSigninManager();
        mFakeAccountManagerFacade.addAccount(ACCOUNT_INFO);

        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt());
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void clearingAccountCookieDoesNotTriggerSignoutWhenUserIsSignedInAndSync() {
        createSigninManager();
        mFakeAccountManagerFacade.addAccount(
                AccountUtils.createAccountFromName(ACCOUNT_INFO.getEmail()));

        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt());
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
    }

    @Test
    public void clearingAccountCookieDoesNotTriggerSignoutWhenUserIsSignedInWithoutSync() {
        createSigninManager();
        mFakeAccountManagerFacade.addAccount(ACCOUNT_INFO);
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        NATIVE_IDENTITY_MANAGER, ConsentLevel.SIGNIN))
                .thenReturn(
                        AccountUtils.findCoreAccountInfoByEmail(
                                mFakeAccountManagerFacade.getCoreAccountInfos().getResult(),
                                ACCOUNT_INFO.getEmail()));

        mIdentityManager.onAccountsCookieDeletedByUserAction();

        verify(mIdentityMutator, never()).clearPrimaryAccount(anyInt());
        verify(mNativeMock, never()).wipeProfileData(anyLong(), any());
        verify(mNativeMock, never()).wipeGoogleServiceWorkerCaches(anyLong(), any());
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
        doAnswer(
                        invocation -> {
                            mIdentityManager.onPrimaryAccountChanged(
                                    new PrimaryAccountChangeEvent(
                                            PrimaryAccountChangeEvent.Type.CLEARED,
                                            PrimaryAccountChangeEvent.Type.NONE));
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
        mFakeAccountManagerFacade.addAccount(ACCOUNT_INFO);
        final Answer<Integer> setPrimaryAccountAnswer =
                invocation -> {
                    // From now on getPrimaryAccountInfo should return account.
                    when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                                    eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                            .thenReturn(ACCOUNT_INFO);
                    return PrimaryAccountError.NO_ERROR;
                };
        doAnswer(setPrimaryAccountAnswer)
                .when(mIdentityMutator)
                .setPrimaryAccount(
                        eq(ACCOUNT_INFO.getId()),
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

        mSigninManager.signinAndEnableSync(ACCOUNT_INFO, SigninAccessPoint.UNKNOWN, null);
        assertEquals(1, callCount.get());
    }

    @Test
    public void signInStateObserverCallOnSignIn() {
        createSigninManager();
        when(mNativeMock.getUserAcceptedAccountManagement(anyLong())).thenReturn(true);
        mFakeAccountManagerFacade.addAccount(ACCOUNT_INFO);
        List<CoreAccountInfo> coreAccountInfos =
                mFakeAccountManagerFacade.getCoreAccountInfos().getResult();
        CoreAccountInfo primaryAccountInfo =
                AccountUtils.findCoreAccountInfoByEmail(coreAccountInfos, ACCOUNT_INFO.getEmail());
        final Answer<Integer> setPrimaryAccountAnswer =
                invocation -> {
                    // From now on getPrimaryAccountInfo should return account.
                    when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                                    eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                            .thenReturn(primaryAccountInfo);
                    return PrimaryAccountError.NO_ERROR;
                };
        doAnswer(setPrimaryAccountAnswer)
                .when(mIdentityMutator)
                .setPrimaryAccount(
                        eq(primaryAccountInfo.getId()),
                        eq(ConsentLevel.SYNC),
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

        mSigninManager.signinAndEnableSync(ACCOUNT_INFO, SigninAccessPoint.START_PAGE, null);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSignInStateObserver).onSignOutAllowedChanged();
        assertFalse(mSigninManager.isSigninAllowed());
        assertTrue(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signInStateObserverCallOnSignOut() {
        createSigninManager();
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        assertTrue(mSigninManager.isSignOutAllowed());

        mSigninManager.signOut(SignoutReason.TEST);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(mSignInStateObserver).onSignOutAllowedChanged();
        assertFalse(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signOutNotAllowedForChildAccounts() {
        createSigninManager();
        when(mIdentityManagerNativeMock.getPrimaryAccountInfo(
                        eq(NATIVE_IDENTITY_MANAGER), anyInt()))
                .thenReturn(ACCOUNT_INFO);
        when(mIdentityManagerNativeMock.isClearPrimaryAccountAllowed(NATIVE_IDENTITY_MANAGER))
                .thenReturn(false);

        assertFalse(mSigninManager.isSignOutAllowed());
    }

    @Test
    public void signInShouldBeSupportedForNonDemoUsers() {
        createSigninManager();
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(true);

        // Make sure that the user is not a demo user.
        ShadowApplication shadowApplication = ShadowApplication.getInstance();
        UserManager userManager = Mockito.mock(UserManager.class);
        Mockito.when(userManager.isDemoUser()).thenReturn(false);
        shadowApplication.setSystemService(Context.USER_SERVICE, userManager);

        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true));
        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ false));
    }

    @Test
    public void signInShouldNotBeSupportedWhenGooglePlayServicesIsRequiredAndNotAvailable() {
        createSigninManager();
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(false);

        // Make sure that the user is not a demo user.
        ShadowApplication shadowApplication = ShadowApplication.getInstance();
        UserManager userManager = Mockito.mock(UserManager.class);
        Mockito.when(userManager.isDemoUser()).thenReturn(false);
        shadowApplication.setSystemService(Context.USER_SERVICE, userManager);

        assertFalse(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true));
    }

    private void createSigninManager() {
        mSigninManager =
                (SigninManagerImpl)
                        SigninManagerImpl.create(
                                NATIVE_SIGNIN_MANAGER,
                                mProfile,
                                mIdentityManager,
                                mIdentityMutator,
                                mSyncService);
        mSigninManager.addSignInStateObserver(mSignInStateObserver);
    }
}
