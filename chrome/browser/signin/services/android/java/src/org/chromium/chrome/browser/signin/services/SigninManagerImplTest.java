// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.IdentityManagerImpl;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SignoutReason;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.url.GURL;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * Integration test for {@link SigninManagerImpl}.
 *
 * <p>These tests initialize the native part of the service.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Integration test suite that changes the list of accounts")
@EnableFeatures(SigninFeatures.SKIP_CHECK_FOR_ACCOUNT_MANAGEMENT_ON_SIGNIN)
public class SigninManagerImplTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private SigninManager.SignInStateObserver mSignInStateObserver;
    @Mock private SigninManagerImpl.Natives mNativeMock;

    private SigninManager mSigninManager;
    private IdentityManagerImpl mIdentityManager;
    private BookmarkModel mBookmarkModel;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    mIdentityManager =
                            (IdentityManagerImpl)
                                    IdentityServicesProvider.get().getIdentityManager(profile);
                    mSigninManager = IdentityServicesProvider.get().getSigninManager(profile);
                    mSigninManager.addSignInStateObserver(mSignInStateObserver);

                    PrefService prefService = UserPrefs.get(profile);
                    prefService.setBoolean(Pref.SIGNIN_ALLOWED, true);

                    mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();
                });

        // Pretend Google Play services are available as it is required for the sign-in
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(false);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSigninManager.removeSignInStateObserver(mSignInStateObserver));
    }

    @Test
    @MediumTest
    public void testSignin() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSigninManager.signin(
                                TestAccounts.ACCOUNT1, SigninAccessPoint.WEB_SIGNIN, callback));

        // Signin should be complete and callback should be invoked.
        verify(callback).onSignInComplete();
        verify(callback, never()).onSignInAborted();

        // The primary account is now present and consented to sign in.
        assertNotNull(mSigninTestRule.getPrimaryAccount());
    }

    @Test
    @MediumTest
    public void testSetUninstallExtensionsOnAccountRemoved() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        SigninManagerImplJni.setInstanceForTesting(mNativeMock);

        verify(mNativeMock, never())
                .setUninstallAccountExtensionsOnSignout(anyLong(), anyBoolean());

        // Removing the account from the device should trigger onAccountsChanged() which sets the
        // configuration for extensions to be uninstalled.
        mSigninTestRule.removeAccount(TestAccounts.ACCOUNT1.getId());

        verify(mNativeMock).setUninstallAccountExtensionsOnSignout(anyLong(), eq(true));
    }

    @Test
    @MediumTest
    public void testDataNotWipedOnSignOut() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        verifyDataNotWipedOnSignout();
    }

    @Test
    @MediumTest
    public void testDataNotWipedOnSignOutWithManagedAccount() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);
        verifyDataNotWipedOnSignout();
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.USER_POLICY_FETCH_REQUIRES_ACCEPTANCE)
    public void testPolicyFetchRequiresAcceptance_NotAccepted() {
        mSigninTestRule.addAccount(TestAccounts.MANAGED_ACCOUNT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSigninManager.setUserAcceptedAccountManagement(false));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.Android.AccountManagementAcceptedBeforeUserPolicyFetch", false);

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSigninManager.signin(
                                TestAccounts.MANAGED_ACCOUNT,
                                SigninAccessPoint.WEB_SIGNIN,
                                callback));

        verify(callback, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)).onSignInComplete();
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.USER_POLICY_FETCH_REQUIRES_ACCEPTANCE)
    public void testPolicyFetchRequiresAcceptance_FeatureDisabled() {
        mSigninTestRule.addAccount(TestAccounts.MANAGED_ACCOUNT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSigninManager.setUserAcceptedAccountManagement(false));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.Android.AccountManagementAcceptedBeforeUserPolicyFetch", false);

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSigninManager.signin(
                                TestAccounts.MANAGED_ACCOUNT,
                                SigninAccessPoint.WEB_SIGNIN,
                                callback));

        verify(callback, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)).onSignInComplete();
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.USER_POLICY_FETCH_REQUIRES_ACCEPTANCE)
    public void testPolicyFetchRequiresAcceptance_Accepted() {
        mSigninTestRule.addAccount(TestAccounts.MANAGED_ACCOUNT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSigninManager.setUserAcceptedAccountManagement(true));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.Android.AccountManagementAcceptedBeforeUserPolicyFetch", true);

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSigninManager.signin(
                                TestAccounts.MANAGED_ACCOUNT,
                                SigninAccessPoint.WEB_SIGNIN,
                                callback));

        verify(callback, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)).onSignInComplete();
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.SKIP_CHECK_FOR_ACCOUNT_MANAGEMENT_ON_SIGNIN)
    @EnableFeatures(SigninFeatures.USER_POLICY_FETCH_REQUIRES_ACCEPTANCE)
    public void testPolicyFetchRequiresAcceptance_ThrowException() {
        mSigninTestRule.addAccount(TestAccounts.MANAGED_ACCOUNT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSigninManager.setUserAcceptedAccountManagement(false));

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Signin.Android.AccountManagementAcceptedBeforeUserPolicyFetch")
                        .build();

        SigninManager.SignInCallback callback = mock(SigninManager.SignInCallback.class);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertThrows(
                            IllegalStateException.class,
                            () ->
                                    mSigninManager.signin(
                                            TestAccounts.MANAGED_ACCOUNT,
                                            SigninAccessPoint.WEB_SIGNIN,
                                            callback));
                });

        verify(callback, never()).onSignInComplete();
        histogramWatcher.assertExpected();
    }

    @Test
    @MediumTest
    public void testSyncPromoShowCountResetsWhenSignOutSyncingAccount() {
        var keyPrefix =
                ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                        SigninPreferencesManager.SigninPromoAccessPointId.NTP);
        ChromeSharedPreferences.getInstance().writeInt(keyPrefix, 1);
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninManager.signOut(SignoutReason.TEST);
                    mSigninManager.runAfterOperationInProgress(
                            () -> {
                                assertEquals(
                                        0,
                                        ChromeSharedPreferences.getInstance().readInt(keyPrefix));
                            });
                });
    }

    @Test
    @MediumTest
    public void testCallbackNotifiedWhenNoOperationIsInProgress() {
        AtomicInteger callCount = new AtomicInteger(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet));
        CriteriaHelper.pollUiThread(() -> callCount.get() == 1);
    }

    @Test
    @MediumTest
    public void testCallbackNotifiedOnSignout() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        AtomicInteger callCount = new AtomicInteger(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninManager.signOut(SignoutReason.TEST);
                    mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
                    assertEquals(0, callCount.get());
                });

        CriteriaHelper.pollUiThread(() -> callCount.get() == 1);
    }

    @Test
    @MediumTest
    public void testCallbackNotifiedOnSignin() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        AtomicInteger callCount = new AtomicInteger(0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninManager.signin(
                            TestAccounts.ACCOUNT1,
                            SigninAccessPoint.WEB_SIGNIN,
                            new SigninManager.SignInCallback() {});
                    mSigninManager.runAfterOperationInProgress(callCount::incrementAndGet);
                    assertEquals(0, callCount.get());
                });

        CriteriaHelper.pollUiThread(() -> callCount.get() == 1);
    }

    @Test
    @MediumTest
    public void testSignInStateObserverCalledOnSignIn() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mSigninManager.isSigninAllowed());
                    mSigninManager.signin(
                            TestAccounts.ACCOUNT1,
                            SigninAccessPoint.START_PAGE,
                            new SigninManager.SignInCallback() {});
                });

        verify(mSignInStateObserver, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(3))
                .onSignInAllowedChanged();
        verify(mSignInStateObserver, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .onSignOutAllowedChanged();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mSigninManager.isSigninAllowed());
                    assertTrue(mSigninManager.isSignOutAllowed());
                });
    }

    @Test
    @MediumTest
    public void testSignInStateObserverCalledOnSignOut() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mSigninManager.isSignOutAllowed());
                    mSigninManager.signOut(SignoutReason.TEST);
                });

        verify(mSignInStateObserver, times(3)).onSignInAllowedChanged();
        verify(mSignInStateObserver, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(2))
                .onSignOutAllowedChanged();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(mSigninManager.isSigninAllowed());
                    assertFalse(mSigninManager.isSignOutAllowed());
                });
    }

    @Test
    @MediumTest
    public void testSignOutNotAllowedForChildAccounts() {
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        assertNotNull(mSigninTestRule.getPrimaryAccount());

        ThreadUtils.runOnUiThreadBlocking(() -> assertFalse(mSigninManager.isSignOutAllowed()));
    }

    @Test
    @MediumTest
    public void testSignInIsSupportedWithGooglePlayServicesAvailable() {
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(true);

        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true));
        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ false));
    }

    @Test
    @MediumTest
    public void testSignInIsSupportedWithGooglePlayServicesNotAvailable() {
        when(mExternalAuthUtils.canUseGooglePlayServices()).thenReturn(false);

        assertFalse(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ true));
        assertTrue(mSigninManager.isSigninSupported(/* requireUpdatedPlayServices= */ false));
    }

    @Test
    @MediumTest
    public void testIsSwitchAccountAllowed_signinNotSupported() {
        when(mExternalAuthUtils.isGooglePlayServicesMissing(any())).thenReturn(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertFalse(mSigninManager.isSwitchAccountAllowed());
                });
    }

    @Test
    @MediumTest
    public void testDidAccountFetchSucceed() {
        assertTrue(mSigninManager.didAccountsFetchSucceed());

        mSigninTestRule.setAccountFetchFailed();
        assertFalse(mSigninManager.didAccountsFetchSucceed());
    }

    private void verifyDataNotWipedOnSignout() {
        GURL url = new GURL(UrlConstants.ABOUT_URL);
        BookmarkId bookmarkId = addBookmark("test", url);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Trigger the sign out flow without wiping user data.
                    mSigninManager.signOut(SignoutReason.TEST);
                    mSigninManager.runAfterOperationInProgress(
                            () -> {
                                assertNotNull(
                                        "Bookmark should not have been wiped",
                                        mBookmarkModel.getBookmarkById(bookmarkId));
                            });
                });
    }

    private BookmarkId addBookmark(String title, GURL url) {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addBookmark(
                                mBookmarkModel.getMobileFolderId(), 0, title, url));
    }
}
