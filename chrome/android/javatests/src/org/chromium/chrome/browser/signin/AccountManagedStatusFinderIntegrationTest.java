// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.SigninConstants;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.AccountManagedStatusFinder;
import org.chromium.components.signin.identitymanager.AccountManagedStatusFinderOutcome;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

/** Integration tests for {@link AccountManagedStatusFinder}. */
@RunWith(BaseJUnit4ClassRunner.class)
@DoNotBatch(reason = "Changes the global state")
public class AccountManagedStatusFinderIntegrationTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Mock private Callback<Integer> mMockCallback;

    private IdentityManager mIdentityManager;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
                });
    }

    @Test
    @MediumTest
    public void testImmediateConsumer() throws Exception {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccountManagedStatusFinder finder =
                            new AccountManagedStatusFinder(
                                    mIdentityManager, TestAccounts.ACCOUNT1, mMockCallback);

                    assertEquals(
                            AccountManagedStatusFinderOutcome.CONSUMER_GMAIL, finder.getOutcome());
                    finder.destroy();
                });
        verify(mMockCallback, never()).onResult(any());
    }

    @Test
    @MediumTest
    public void testImmediateEnterprise() throws Exception {
        mSigninTestRule.addAccount(TestAccounts.MANAGED_ACCOUNT);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccountManagedStatusFinder finder =
                            new AccountManagedStatusFinder(
                                    mIdentityManager, TestAccounts.MANAGED_ACCOUNT, mMockCallback);

                    assertEquals(AccountManagedStatusFinderOutcome.ENTERPRISE, finder.getOutcome());
                    finder.destroy();
                });
        verify(mMockCallback, never()).onResult(any());
    }

    @Test
    @MediumTest
    public void testDelayedConsumer() throws Exception {
        AccountInfo accountHostedDomainUnknown =
                new AccountInfo.Builder(
                                "test@example.com",
                                FakeAccountManagerFacade.toGaiaId("test@example.com"))
                        .build();

        mSigninTestRule.addAccount(accountHostedDomainUnknown);

        AccountManagedStatusFinder finder =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var result =
                                    new AccountManagedStatusFinder(
                                            mIdentityManager,
                                            accountHostedDomainUnknown,
                                            mMockCallback);
                            assertEquals(
                                    AccountManagedStatusFinderOutcome.PENDING, result.getOutcome());
                            return result;
                        });
        verify(mMockCallback, never()).onResult(any());

        AccountInfo accountWithNoHostedDomainFound =
                new AccountInfo.Builder(accountHostedDomainUnknown)
                        .hostedDomain(SigninConstants.NO_HOSTED_DOMAIN_FOUND)
                        .build();
        mSigninTestRule.updateAccount(accountWithNoHostedDomainFound);

        verify(mMockCallback).onResult(AccountManagedStatusFinderOutcome.CONSUMER_NOT_WELL_KNOWN);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            AccountManagedStatusFinderOutcome.CONSUMER_NOT_WELL_KNOWN,
                            finder.getOutcome());
                    finder.destroy();
                });
    }

    @Test
    @MediumTest
    public void testDelayedEnterprise() throws Exception {
        AccountInfo accountHostedDomainUnknown =
                new AccountInfo.Builder(
                                "test@example.com",
                                FakeAccountManagerFacade.toGaiaId("test@example.com"))
                        .build();

        mSigninTestRule.addAccount(accountHostedDomainUnknown);

        AccountManagedStatusFinder finder =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            var result =
                                    new AccountManagedStatusFinder(
                                            mIdentityManager,
                                            accountHostedDomainUnknown,
                                            mMockCallback);
                            assertEquals(
                                    AccountManagedStatusFinderOutcome.PENDING, result.getOutcome());
                            return result;
                        });
        verify(mMockCallback, never()).onResult(any());

        AccountInfo accountWithHostedDomain =
                new AccountInfo.Builder(accountHostedDomainUnknown)
                        .hostedDomain("example.com")
                        .build();
        mSigninTestRule.updateAccount(accountWithHostedDomain);

        verify(mMockCallback).onResult(AccountManagedStatusFinderOutcome.ENTERPRISE);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(AccountManagedStatusFinderOutcome.ENTERPRISE, finder.getOutcome());
                    finder.destroy();
                });
    }
}
