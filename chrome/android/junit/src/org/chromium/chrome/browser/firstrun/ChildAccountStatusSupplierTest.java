// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.robolectric.Shadows.shadowOf;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.os.Looper;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.test.util.FakeAccountManagerFacade;

/** Tests for {@link ChildAccountStatusSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChildAccountStatusSupplierTest {
    private static final String ADULT_ACCOUNT_EMAIL = "adult.account@gmail.com";

    FakeAccountManagerFacade mAccountManagerFacade = new FakeAccountManagerFacade();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mAccountManagerFacade);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Captor public ArgumentCaptor<Callback<Boolean>> mCallbackCaptor;
    @Mock private FirstRunAppRestrictionInfo mFirstRunAppRestrictionInfoMock;

    @Test
    public void testNoAccounts() {
        ChildAccountStatusSupplier supplier;
        try (var ignored =
                mAccountManagerFacade.blockGetCoreAccountInfos(/* populateCache= */ false)) {
            supplier =
                    new ChildAccountStatusSupplier(
                            mAccountManagerFacade, mFirstRunAppRestrictionInfoMock);
            shadowOf(Looper.getMainLooper()).idle();
            // Supplier shouldn't be set and should not record any histograms until it can obtain
            // the list of accounts from AccountManagerFacade.
            assertNull(supplier.get());
            assertEquals(
                    0,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "MobileFre.ChildAccountStatusDuration"));
        }
        shadowOf(Looper.getMainLooper()).idle();

        assertFalse(supplier.get());
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }

    @Test
    public void testOneChildAccount() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        ChildAccountStatusSupplier supplier =
                new ChildAccountStatusSupplier(
                        mAccountManagerFacade, mFirstRunAppRestrictionInfoMock);
        shadowOf(Looper.getMainLooper()).idle();

        assertTrue(supplier.get());
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }

    @Test
    public void testNonChildAccount() {
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_EMAIL);

        ChildAccountStatusSupplier supplier =
                new ChildAccountStatusSupplier(
                        mAccountManagerFacade, mFirstRunAppRestrictionInfoMock);
        shadowOf(Looper.getMainLooper()).idle();

        assertFalse(supplier.get());
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }

    @Test
    public void testOneChildAccountWithNonChildAccounts() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_EMAIL);

        ChildAccountStatusSupplier supplier =
                new ChildAccountStatusSupplier(
                        mAccountManagerFacade, mFirstRunAppRestrictionInfoMock);
        shadowOf(Looper.getMainLooper()).idle();

        assertTrue(supplier.get());
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }

    @Test
    public void testNonChildWhenNoAppRestrictions() {
        mAccountManagerTestRule.addAccount(ADULT_ACCOUNT_EMAIL);
        // Block getAccounts call to make sure ChildAccountStatusSupplier checks app restrictions.
        try (var ignored =
                mAccountManagerFacade.blockGetCoreAccountInfos(/* populateCache= */ false)) {
            doNothing()
                    .when(mFirstRunAppRestrictionInfoMock)
                    .getHasAppRestriction(mCallbackCaptor.capture());
            ChildAccountStatusSupplier supplier =
                    new ChildAccountStatusSupplier(
                            mAccountManagerFacade, mFirstRunAppRestrictionInfoMock);
            shadowOf(Looper.getMainLooper()).idle();
            assertNull(supplier.get());

            Callback<Boolean> getHasAppRestrictionsCallback = mCallbackCaptor.getValue();
            getHasAppRestrictionsCallback.onResult(false);

            // No app restrictions should mean that the child account status is false.
            assertFalse(supplier.get());
            assertEquals(
                    1,
                    RecordHistogram.getHistogramTotalCountForTesting(
                            "MobileFre.ChildAccountStatusDuration"));
        }
    }

    @Test
    public void testWaitsForAccountManagerFacadeWhenAppRestrictionsFound() {
        mAccountManagerTestRule.addAccount(AccountManagerTestRule.TEST_CHILD_ACCOUNT);

        ChildAccountStatusSupplier supplier;
        // Block getAccounts call to make sure ChildAccountStatusSupplier checks app restrictions.
        try (var ignored =
                mAccountManagerFacade.blockGetCoreAccountInfos(/* populateCache= */ false)) {
            doCallback((Callback<Boolean> callback) -> callback.onResult(true))
                    .when(mFirstRunAppRestrictionInfoMock)
                    .getHasAppRestriction(any());
            supplier =
                    new ChildAccountStatusSupplier(
                            mAccountManagerFacade, mFirstRunAppRestrictionInfoMock);
            shadowOf(Looper.getMainLooper()).idle();
            // Since app restrictions were found - ChildAccountSupplier should wait for status from
            // AccountManagerFacade, so the status shouldn't be available yet.
            assertNull(supplier.get());
        }
        shadowOf(Looper.getMainLooper()).idle();

        assertTrue(supplier.get());
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "MobileFre.ChildAccountStatusDuration"));
    }
}
