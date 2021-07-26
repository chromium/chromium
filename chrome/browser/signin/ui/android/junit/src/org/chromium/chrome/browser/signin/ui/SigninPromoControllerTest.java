// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.accounts.Account;

import com.google.common.base.Optional;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.AccountUtils;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/**
 * Tests for {@link SigninPromoController}..
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.MINOR_MODE_SUPPORT})
@Features.DisableFeatures({ChromeFeatureList.FORCE_DISABLE_EXTENDED_SYNC_PROMOS})
public class SigninPromoControllerTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final Features.JUnitProcessor processor = new Features.JUnitProcessor();

    private final FakeAccountManagerFacade mFakeAccountManagerFacade =
            spy(new FakeAccountManagerFacade(null));

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeAccountManagerFacade);

    @Mock
    private IdentityManager mIdentityManagerMock;

    @Before
    public void setUp() {
        Profile.setLastUsedProfileForTesting(mock(Profile.class));
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(Profile.getLastUsedRegularProfile()))
                .thenReturn(mIdentityManagerMock);
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenNoAccountOnDevice() {
        Assert.assertFalse(SigninPromoController.shouldHideSyncPromoForNTP(
                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDefaultAccountCannotOfferSyncPromos() {
        final Account account =
                AccountUtils.createAccountFromName("test.account.default@gmail.com");
        mAccountManagerTestRule.addAccount(account);
        mAccountManagerTestRule.addAccount("test.account.secondary@gmail.com");
        doReturn(Optional.of(false))
                .when(mFakeAccountManagerFacade)
                .canOfferExtendedSyncPromos(account);

        Assert.assertTrue(SigninPromoController.shouldHideSyncPromoForNTP(
                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldHideSyncPromoForNTPWhenDefaultAccountCapabilityIsNotFetched() {
        final Account account =
                AccountUtils.createAccountFromName("test.account.default@gmail.com");
        mAccountManagerTestRule.addAccount(account);
        mAccountManagerTestRule.addAccount("test.account.secondary@gmail.com");

        Assert.assertTrue(SigninPromoController.shouldHideSyncPromoForNTP(
                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }

    @Test
    public void shouldShowSyncPromoForNTPWhenSecondaryAccountCannotOfferSyncPromos() {
        final Account secondAccount =
                AccountUtils.createAccountFromName("test.account.secondary@gmail.com");
        doAnswer(invocation -> {
            final Account account0 = invocation.getArgument(0);
            return Optional.of(!account0.equals(secondAccount));
        })
                .when(mFakeAccountManagerFacade)
                .canOfferExtendedSyncPromos(any());
        mAccountManagerTestRule.addAccount("test.account.default@gmail.com");
        mAccountManagerTestRule.addAccount(secondAccount);

        Assert.assertFalse(SigninPromoController.shouldHideSyncPromoForNTP(
                SigninAccessPoint.NTP_CONTENT_SUGGESTIONS));
    }
}
