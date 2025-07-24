// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.components.browser_ui.settings.SettingsNavigation;

/** Tests for SettingsNavigationImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsNavigationImplTest {
    private final Context mContext;
    private final SettingsNavigationImpl mSettingsNavigationImpl;

    public SettingsNavigationImplTest() {
        mContext = Robolectric.buildActivity(Activity.class).get();
        mSettingsNavigationImpl = new SettingsNavigationImpl();
    }

    @Test
    public void testCreateSettingsIntent_financialAccounts() {
        Intent intent =
                mSettingsNavigationImpl.createSettingsIntent(
                        mContext,
                        SettingsNavigation.SettingsFragment.FINANCIAL_ACCOUNTS,
                        /* fragmentArgs */ null);
        assertEquals(
                intent.getStringExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT),
                FinancialAccountsManagementFragment.class.getName());
    }

    @Test
    public void testCreateSettingsIntent_nonCardPaymentMethods() {
        Intent intent =
                mSettingsNavigationImpl.createSettingsIntent(
                        mContext,
                        SettingsNavigation.SettingsFragment.NON_CARD_PAYMENT_METHODS,
                        /* fragmentArgs */ null);
        assertEquals(
                intent.getStringExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT),
                NonCardPaymentMethodsManagementFragment.class.getName());
    }
}
