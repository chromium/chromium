// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.fragment.app.Fragment;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.settings.FinancialAccountsManagementFragment;
import org.chromium.chrome.browser.autofill.settings.NonCardPaymentMethodsManagementFragment;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
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

    /** Stands in for {@link SettingsActivity}: an activity that hosts settings itself. */
    public static class FakeSettingsHostActivity extends Activity implements SettingsHost {
        @Override
        public boolean isShownInTab() {
            return false;
        }
    }

    public SettingsNavigationImplTest() {
        mContext = Robolectric.buildActivity(Activity.class).get();
        mSettingsNavigationImpl = new SettingsNavigationImpl();
    }

    @Before
    public void setUp() {
        // Several tests call createIntent(), which mutates the static last-intent in
        // SettingsIntentUtil. This registers a resetter that clears it after each test.
        SettingsIntentUtil.setLastIntentForTesting(null);
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

    /**
     * {@link SettingsInTab#shouldOpenSettingsInTab()} depends on the current screen width, so it
     * can become false while settings is already open in a tab, for example when the user folds a
     * foldable device. {@code finishCurrentSettings()} must keep delegating to the host fragment
     * instead of casting the activity to {@link SettingsActivity}. Regression test for
     * crbug.com/562619494.
     */
    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testFinishCurrentSettings_SettingsInTabDisabled_DelegatesToHostFragment() {
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
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());

        // Show a fragment in the host directly. startSettings() cannot be used here because it
        // only routes to the host fragment when settings would be opened in a tab.
        hostFragment.showFragment(
                new SecondFakeSettingsFragment(), /* addToBackStack= */ false, /* tag= */ null);
        hostFragment.getChildFragmentManager().executePendingTransactions();
        Fragment active = hostFragment.getActiveFragment();
        assertTrue(active instanceof SecondFakeSettingsFragment);

        // Now call finishCurrentSettings on active fragment.
        mSettingsNavigationImpl.finishCurrentSettings(active);
        hostFragment.getChildFragmentManager().executePendingTransactions();

        // Should return to initial fragment without casting activity to SettingsActivity.
        assertTrue(hostFragment.getActiveFragment() instanceof FirstFakeSettingsFragment);
    }

    /** Regression test for https://crbug.com/559534170. */
    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
    @Config(qualifiers = "sw600dp")
    public void testStartSettings_SettingsInTab_downloads_savesLastIntent() {
        var scenario = Robolectric.buildActivity(TestActivity.class).setup();
        TestActivity activity = scenario.get();

        mSettingsNavigationImpl.startSettings(activity, DownloadSettings.class);

        Intent lastIntent = SettingsIntentUtil.takeLastIntent();
        assertNotNull(lastIntent);
        assertEquals(
                DownloadSettings.class.getName(),
                lastIntent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
    @Config(qualifiers = "sw600dp")
    public void testStartSettings_SettingsInTabUrlNav_downloads_setsIntentUrl() {
        var scenario = Robolectric.buildActivity(TestActivity.class).setup();
        TestActivity activity = scenario.get();

        mSettingsNavigationImpl.startSettings(activity, DownloadSettings.class);

        Intent started = shadowOf(activity).getNextStartedActivity();
        assertNotNull(started);
        assertEquals("chrome://settings/downloads", started.getDataString());
        assertEquals(
                DownloadSettings.class.getName(),
                started.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
        assertNull(SettingsIntentUtil.takeLastIntent());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV})
    @Config(qualifiers = "sw600dp")
    public void testSettingsInTabUrlNav_DelegatesToHostFragmentNavigation() {
        var scenario = Robolectric.buildActivity(TestActivity.class).setup();
        TestActivity activity = scenario.get();
        TestSettingsHostFragment hostFragment = new TestSettingsHostFragment();
        SettingsNavigation mockHostNav = mock(SettingsNavigation.class);
        hostFragment.setSettingsNavigation(mockHostNav);
        activity.getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        hostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        mSettingsNavigationImpl.startSettings(activity, DownloadSettings.class);
        verify(mockHostNav)
                .startSettings(
                        activity,
                        DownloadSettings.class,
                        /* fragmentArgs= */ null,
                        /* addToBackStack= */ false,
                        /* tag= */ null);

        Fragment active = hostFragment.getActiveFragment();
        mSettingsNavigationImpl.finishCurrentSettings(active);
        verify(mockHostNav)
                .finishCurrentSettings(active, /* parentFragment= */ null, /* parentArgs= */ null);
    }

    /**
     * When settings is already open in a tab, {@code startSettings()} must route to the existing
     * {@link SettingsHostFragment} even if {@link SettingsInTab#shouldOpenSettingsInTab()} has
     * become false (e.g. screen width changed on a foldable device).
     */
    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void testStartSettings_SettingsInTabDisabled_ShowsInHostFragment() {
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
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());

        mSettingsNavigationImpl.startSettings(activity, SecondFakeSettingsFragment.class, null);
        hostFragment.getChildFragmentManager().executePendingTransactions();

        assertTrue(hostFragment.getActiveFragment() instanceof SecondFakeSettingsFragment);
    }

    /**
     * When settings is already open in a tab, {@code createSettingsIntent()} must create an intent
     * to open in a tab via {@link SettingsHostFragment} even if {@link
     * SettingsInTab#shouldOpenSettingsInTab()} is false.
     */
    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    public void
            testCreateSettingsIntent_SettingsInTabDisabled_WithHostFragment_LaunchesChromeLauncherActivity() {
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
        assertFalse(SettingsInTab.shouldOpenSettingsInTab());

        Intent intent =
                mSettingsNavigationImpl.createSettingsIntent(
                        activity, FakeEmbeddableSettingsFragment.class);

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(UrlConstants.SETTINGS_URL, intent.getDataString());
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
    }

    /**
     * When settings is hosted by an activity, navigation must stay in that activity even if {@link
     * SettingsInTab#shouldOpenSettingsInTab()} is true (e.g. settings was opened while the window
     * was narrow and the window was widened afterwards).
     */
    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @Config(qualifiers = "sw600dp")
    public void testCreateSettingsIntent_ActivityHostedSettings_LaunchesSettingsActivity() {
        Activity host = Robolectric.buildActivity(FakeSettingsHostActivity.class).setup().get();
        assertTrue(SettingsInTab.shouldOpenSettingsInTab());

        Intent intent =
                mSettingsNavigationImpl.createSettingsIntent(
                        host, FakeEmbeddableSettingsFragment.class);

        assertEquals(SettingsActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(
                FakeEmbeddableSettingsFragment.class.getName(),
                intent.getStringExtra(SettingsIntentUtil.EXTRA_SHOW_FRAGMENT));
    }

    /** As above, for {@code startSettings()}, which is what settings pages call. */
    @Test
    @EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB})
    @Config(qualifiers = "sw600dp")
    public void testStartSettings_ActivityHostedSettings_StartsSettingsActivity() {
        Activity host = Robolectric.buildActivity(FakeSettingsHostActivity.class).setup().get();
        assertTrue(SettingsInTab.shouldOpenSettingsInTab());

        mSettingsNavigationImpl.startSettings(host, FakeEmbeddableSettingsFragment.class);

        Intent started = shadowOf(host).getNextStartedActivity();
        assertNotNull(started);
        assertEquals(SettingsActivity.class.getName(), started.getComponent().getClassName());
    }
}
