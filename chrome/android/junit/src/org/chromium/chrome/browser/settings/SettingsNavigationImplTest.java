// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.fragment.app.Fragment;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.base.TestActivity;

/** Tests for SettingsNavigationImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsNavigationImplTest {
    private final Context mContext;
    private final SettingsNavigationImpl mSettingsNavigationImpl;

    /** Fake settings fragment for testing. */
    public static class FakeSettingsFragment extends Fragment {
        public FakeSettingsFragment() {}
    }

    /** Another fake settings fragment for testing transitions. */
    public static class SecondFakeSettingsFragment extends Fragment {
        public SecondFakeSettingsFragment() {}
    }

    /** Subclass SettingsHostFragment to mock initial fragment instantiation. */
    public static class TestSettingsHostFragment extends SettingsHostFragment {
        @Override
        protected Fragment createInitialFragment() {
            return new FakeSettingsFragment();
        }
    }

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
                        /* fragmentArgs= */ null);
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
                        /* fragmentArgs= */ null);
        assertEquals(
                intent.getStringExtra(SettingsActivity.EXTRA_SHOW_FRAGMENT),
                NonCardPaymentMethodsManagementFragment.class.getName());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testStartSettings_SettingsInTab_ShowsInHostFragment() {
        var scenario = Robolectric.buildActivity(TestActivity.class).setup();
        TestActivity activity = scenario.get();
        TestSettingsHostFragment hostFragment = new TestSettingsHostFragment();
        activity.getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        hostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        mSettingsNavigationImpl.startSettings(activity, SecondFakeSettingsFragment.class, null);
        hostFragment.getChildFragmentManager().executePendingTransactions();

        assertTrue(hostFragment.getActiveFragment() instanceof SecondFakeSettingsFragment);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testStartSettings_SettingsInTab_NullFragment_ShowsInitialFragment() {
        var scenario = Robolectric.buildActivity(TestActivity.class).setup();
        TestActivity activity = scenario.get();
        TestSettingsHostFragment hostFragment = new TestSettingsHostFragment();
        activity.getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        hostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        // First show some other fragment.
        mSettingsNavigationImpl.startSettings(activity, SecondFakeSettingsFragment.class, null);
        hostFragment.getChildFragmentManager().executePendingTransactions();
        assertTrue(hostFragment.getActiveFragment() instanceof SecondFakeSettingsFragment);

        // Now startSettings with null fragment class.
        mSettingsNavigationImpl.startSettings(activity, null, null);
        hostFragment.getChildFragmentManager().executePendingTransactions();

        // The initial fragment shown when Settings is started for the first time is shown.
        assertTrue(hostFragment.getActiveFragment() instanceof FakeSettingsFragment);
    }
}
