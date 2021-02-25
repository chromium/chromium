// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

import java.util.Set;

/** Tests for {@link SigninPromoUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninPromoUtilLaunchSigninPromoTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade(null));

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Mock
    private SigninActivityLauncher mLauncherMock;

    @Mock
    private Activity mActivityMock;

    private final SigninPreferencesManager mPrefManager = SigninPreferencesManager.getInstance();

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mock(Profile.class));
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(Profile.getLastUsedRegularProfile()))
                .thenReturn(mIdentityManagerMock);

        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
    }

    @After
    public void tearDown() {
        mPrefManager.clearSigninPromoLastShownPrefsForTesting();
    }

    @Test
    public void whenAccountCacheNotPopulated() {
        when(mFakeAccountManagerFacade.isCachePopulated()).thenReturn(false);
        Assert.assertFalse(SigninPromoUtil.launchSigninPromoIfNeeded(mActivityMock, mLauncherMock));
        Assert.assertEquals(0, mPrefManager.getSigninPromoLastShownVersion());
        verify(mFakeAccountManagerFacade, never()).tryGetGoogleAccounts();
        verify(mLauncherMock, never()).launchActivityIfAllowed(any(), anyInt());
    }

    @Test
    public void whenNoLastShownVersionShouldReturnFalseAndSaveVersion() {
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, false));
        Assert.assertEquals(42, mPrefManager.getSigninPromoLastShownVersion());
        verify(mFakeAccountManagerFacade, never()).tryGetGoogleAccounts();
    }

    @Test
    public void whenSignedInShouldReturnFalse() {
        final CoreAccountInfo coreAccountInfo = mAccountManagerTestRule.toCoreAccountInfo(
                AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        when(mIdentityManagerMock.getPrimaryAccountInfo(ConsentLevel.SYNC))
                .thenReturn(coreAccountInfo);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, false));
        verify(mFakeAccountManagerFacade, never()).tryGetGoogleAccounts();
    }

    @Test
    public void whenWasSignedInShouldReturnFalse() {
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, true));
        verify(mFakeAccountManagerFacade, never()).tryGetGoogleAccounts();
    }

    @Test
    public void whenVersionDifferenceTooSmallShouldReturnFalse() {
        mPrefManager.setSigninPromoLastShownVersion(41);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, false));
        verify(mFakeAccountManagerFacade, never()).tryGetGoogleAccounts();
    }

    @Test
    public void whenNoAccountsShouldReturnFalse() {
        mAccountManagerTestRule.removeAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mPrefManager.setSigninPromoLastShownVersion(38);
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, false));
        verify(mFakeAccountManagerFacade).tryGetGoogleAccounts();
    }

    @Test
    public void whenNoAccountListStoredShouldReturnTrue() {
        mPrefManager.setSigninPromoLastShownVersion(40);
        // Old implementation hasn't been storing account list
        Assert.assertTrue(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, false));
        verify(mFakeAccountManagerFacade).tryGetGoogleAccounts();
    }

    @Test
    public void whenHasNewAccountShouldReturnTrue() {
        mAccountManagerTestRule.addAccount("test2@gmail.com");
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountNames(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_EMAIL));
        Assert.assertTrue(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, false));
        verify(mFakeAccountManagerFacade).tryGetGoogleAccounts();
    }

    @Test
    public void whenAccountListUnchangedShouldReturnFalse() {
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountNames(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_EMAIL));
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, false));
        verify(mFakeAccountManagerFacade).tryGetGoogleAccounts();
    }

    @Test
    public void whenNoNewAccountsShouldReturnFalse() {
        mPrefManager.setSigninPromoLastShownVersion(40);
        mPrefManager.setSigninPromoLastAccountNames(
                Set.of(AccountManagerTestRule.TEST_ACCOUNT_EMAIL, "test2@gmail.com"));
        Assert.assertFalse(SigninPromoUtil.shouldLaunchSigninPromo(mPrefManager, 42, false));
        verify(mFakeAccountManagerFacade).tryGetGoogleAccounts();
    }
}
