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
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.TestActivity;

/** Tests for SettingsNavigationImpl. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsNavigationImplTest {
    private final Context mContext;
    private final SettingsNavigationImpl mSettingsNavigationImpl;

    /** Fake settings fragment for testing. */
    public static class FirstFakeSettingsFragment extends Fragment {
        public FirstFakeSettingsFragment() {}
    }

    /** Another fake settings fragment for testing transitions. */
    public static class SecondFakeSettingsFragment extends Fragment {
        public SecondFakeSettingsFragment() {}
    }

    /** Fake embeddable settings fragment for testing SettingsInTab intent creation. */
    public static class FakeEmbeddableSettingsFragment extends Fragment
            implements EmbeddableSettingsPage {
        public FakeEmbeddableSettingsFragment() {}

        @Override
        public org.chromium.base.supplier.MonotonicObservableSupplier<String> getPageTitle() {
            return null;
        }

        @Override
        public int getAnimationType() {
            return AnimationType.PROPERTY;
        }
    }

    /** Subclass SettingsHostFragment to mock initial fragment instantiation. */
    public static class TestSettingsHostFragment extends SettingsHostFragment {
        @Override
        protected Fragment createInitialFragment(@Nullable Intent intent) {
            return new FirstFakeSettingsFragment();
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
                intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT),
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
                intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT),
                NonCardPaymentMethodsManagementFragment.class.getName());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @Config(qualifiers = "sw600dp")
    public void testCreateSettingsIntent_SettingsInTab_LaunchesChromeLauncherActivity() {
        Intent intent =
                mSettingsNavigationImpl.createSettingsIntent(
                        mContext, FakeEmbeddableSettingsFragment.class);

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(UrlConstants.SETTINGS_URL, intent.getDataString());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(
                FakeEmbeddableSettingsFragment.class.getName(),
                intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @Config(qualifiers = "sw600dp")
    public void
            testCreateSettingsIntent_SettingsInTab_StandaloneFragment_LaunchesSettingsActivity() {
        Intent intent =
                mSettingsNavigationImpl.createSettingsIntent(
                        mContext, FirstFakeSettingsFragment.class);

        assertEquals(SettingsActivity.class.getName(), intent.getComponent().getClassName());
        assertTrue(
                intent.getBooleanExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT_STANDALONE, false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @Config(qualifiers = "sw600dp")
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
    @Config(qualifiers = "sw600dp")
    public void testStartSettings_SettingsInTab_NonActivityContext_ShowsInHostFragment() {
        var scenario = Robolectric.buildActivity(TestActivity.class).setup();
        TestActivity activity = scenario.get();
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.RESUMED);
        TestSettingsHostFragment hostFragment = new TestSettingsHostFragment();

        // The SettingsHostFragment will default to showing FirstFakeSettingsFragment.
        activity.getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        hostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        // Start settings with a non-activity context, requesting the second fragment.
        mSettingsNavigationImpl.startSettings(
                ContextUtils.getApplicationContext(), SecondFakeSettingsFragment.class, null);
        hostFragment.getChildFragmentManager().executePendingTransactions();

        // The second fragment is shown even though the context is not an activity context.
        assertTrue(hostFragment.getActiveFragment() instanceof SecondFakeSettingsFragment);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @Config(qualifiers = "sw600dp")
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
        assertTrue(hostFragment.getActiveFragment() instanceof FirstFakeSettingsFragment);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @Config(qualifiers = "sw600dp")
    public void testFinishCurrentSettings_SettingsInTab_DelegatesToHostFragment() {
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
        Fragment active = hostFragment.getActiveFragment();
        assertTrue(active instanceof SecondFakeSettingsFragment);

        // Now call finishCurrentSettings on active fragment.
        mSettingsNavigationImpl.finishCurrentSettings(active);
        hostFragment.getChildFragmentManager().executePendingTransactions();

        // Should return to initial fragment without casting activity to SettingsActivity.
        assertTrue(hostFragment.getActiveFragment() instanceof FirstFakeSettingsFragment);
    }
}
