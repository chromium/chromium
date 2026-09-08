// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources.Theme;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.sync.SyncService;

/** Unit tests for {@link SettingsHostFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "w720dp-h1024dp")
@EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_MULTI_COLUMN})
public class SettingsHostFragmentTest {
    @Rule
    public ActivityScenarioRule<TestChromeBaseAppCompatActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestChromeBaseAppCompatActivity.class);

    private TestChromeBaseAppCompatActivity mActivity;
    private SettingsHostFragment mSettingsHostFragment;

    /** Subclass SettingsHostFragment to mock initial fragment instantiation. */
    public static class TestSettingsHostFragment extends SettingsHostFragment {
        @Override
        protected Fragment createInitialFragment(@Nullable Intent intent) {
            return new FirstFakeSettingsFragment();
        }
    }

    @Before
    public void setUp() {
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            mActivity = activity;
                            ApplicationStatus.onStateChangeForTesting(
                                    activity, ActivityState.RESUMED);
                        });
        ProfileManager.setLastUsedProfileForTesting(mock(Profile.class));
        IdentityServicesProvider.setSigninManagerForTesting(mock(SigninManager.class));
        TemplateUrlServiceFactory.setInstanceForTesting(mock(TemplateUrlService.class));
        SyncServiceFactory.setInstanceForTesting(mock(SyncService.class));
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
    }

    private void attachHostFragment() {
        mSettingsHostFragment = new TestSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_IN_TAB_DESKTOP})
    public void testConstructor_SettingsInTabDisabled_ThrowsAssertionError() {
        Assume.assumeTrue(BuildConfig.ENABLE_ASSERTS);
        assertThrows(AssertionError.class, SettingsHostFragment::new);
    }

    @Test
    public void testInitialFragmentAttached() {
        attachHostFragment();
        Fragment current =
                mSettingsHostFragment
                        .getChildFragmentManager()
                        .findFragmentById(mSettingsHostFragment.getView().getId());
        assertNotNull("Initial fragment should be attached", current);
        assertTrue(
                "Initial fragment should be FirstFakeSettingsFragment",
                current instanceof FirstFakeSettingsFragment);
    }

    @Test
    public void testContextProvidesTheme() {
        attachHostFragment();
        Context context = mSettingsHostFragment.getContext();
        assertNotNull(context);
        assertEquals(R.style.ThemeOverlay_Chromium_Settings, context.getThemeResId());
    }

    @Test
    public void testContextPreservesThemeAttributes() {
        // Record the default attribute value on the activity theme.
        TypedValue defaultTv = new TypedValue();
        mActivity.getTheme().resolveAttribute(android.R.attr.colorAccent, defaultTv, true);

        // Apply a theme overlay to the activity and verify the attribute changes.
        mActivity.getTheme().applyStyle(R.style.ThemeOverlay_Chromium_Settings_Containment, true);
        TypedValue customTv = new TypedValue();
        mActivity.getTheme().resolveAttribute(android.R.attr.colorAccent, customTv, true);
        assertNotEquals(defaultTv.data, customTv.data);

        // Verify that SettingsHostFragment's themed context preserves the overlaid attribute.
        attachHostFragment();
        Context context = mSettingsHostFragment.getContext();
        assertNotNull(context);

        Theme theme = context.getTheme();
        TypedValue contextTv = new TypedValue();
        assertTrue(theme.resolveAttribute(android.R.attr.colorAccent, contextTv, true));
        assertEquals(customTv.data, contextTv.data);
    }

    @Test
    public void testOnPreferenceStartFragment() {
        attachHostFragment();
        Preference preference = mock(Preference.class);
        when(preference.getFragment()).thenReturn(SecondFakeSettingsFragment.class.getName());
        Bundle extras = new Bundle();
        extras.putString("test_key", "test_value");
        when(preference.getExtras()).thenReturn(extras);

        PreferenceFragmentCompat caller = mock(PreferenceFragmentCompat.class);
        boolean handled = mSettingsHostFragment.onPreferenceStartFragment(caller, preference);

        assertTrue("Preference start fragment should be handled", handled);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();

        Fragment current =
                mSettingsHostFragment
                        .getChildFragmentManager()
                        .findFragmentById(mSettingsHostFragment.getView().getId());
        assertNotNull("New fragment should be attached", current);
        assertTrue(
                "New fragment should be SecondFakeSettingsFragment",
                current instanceof SecondFakeSettingsFragment);
        assertEquals("test_value", current.getArguments().getString("test_key"));
        assertEquals(1, mSettingsHostFragment.getChildFragmentManager().getBackStackEntryCount());
    }

    @Test
    public void testCreateInitialFragment() {
        SettingsHostFragment fragment = new SettingsHostFragment();

        Fragment initial = fragment.createInitialFragment(null);
        assertTrue(
                "Initial fragment should be MultiColumnSettings",
                initial instanceof MultiColumnSettings);
    }

    @Test
    public void testClearInitialUrl() {
        mSettingsHostFragment = new TestMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);

        mSettingsHostFragment.setInitialUrl("chrome://settings/appearance");
        multiColumnSettings.setInitialUrl("chrome://settings/appearance");
        assertEquals("chrome://settings/appearance", mSettingsHostFragment.getInitialUrl());
        assertEquals("chrome://settings/appearance", multiColumnSettings.getInitialUrl());

        mSettingsHostFragment.clearInitialUrl();
        assertNull(
                "clearInitialUrl should clear initial URL on SettingsHostFragment",
                mSettingsHostFragment.getInitialUrl());
        assertNull(
                "clearInitialUrl should clear initial URL on child MultiColumnSettings",
                multiColumnSettings.getInitialUrl());
    }

    @Test
    public void testGetAndShowFragment() {
        attachHostFragment();
        assertEquals(mSettingsHostFragment, SettingsHostFragment.get(mActivity));

        boolean shown =
                mSettingsHostFragment.showFragment(
                        new SecondFakeSettingsFragment(),
                        /* addToBackStack= */ true,
                        /* tag= */ null);
        assertTrue("showFragment should succeed when attached", shown);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();

        Fragment current = mSettingsHostFragment.getActiveFragment();
        assertNotNull("Active fragment should be present", current);
        assertTrue(
                "Active fragment should be SecondFakeSettingsFragment",
                current instanceof SecondFakeSettingsFragment);
    }

    @Test
    public void testShowFragment_NullFragment_ShowsInitialFragment() {
        attachHostFragment();
        // First show some other fragment.
        mSettingsHostFragment.showFragment(
                new SecondFakeSettingsFragment(), /* addToBackStack= */ false, /* tag= */ null);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();
        assertTrue(mSettingsHostFragment.getActiveFragment() instanceof SecondFakeSettingsFragment);

        // Now show null fragment, which should show initial fragment (FakeSettingsFragment).
        boolean shown =
                mSettingsHostFragment.showFragment(
                        null, /* addToBackStack= */ false, /* tag= */ null);
        assertTrue("showFragment should succeed for null fragment", shown);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();

        Fragment current = mSettingsHostFragment.getActiveFragment();
        assertNotNull("Active fragment should be present", current);
        assertTrue(
                "Active fragment should be FirstFakeSettingsFragment after passing null",
                current instanceof FirstFakeSettingsFragment);
    }

    @Test
    public void testShowFragment_NullFragment_ResetsDetailFragment() {
        mSettingsHostFragment = new TestMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);

        // First show a detail fragment with addToBackStack=true.
        multiColumnSettings.showDetailFragment(
                new SecondFakeSettingsFragment(), /* addToBackStack= */ true, /* tag= */ null);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();
        assertEquals(1, multiColumnSettings.getChildFragmentManager().getBackStackEntryCount());

        // Now show null fragment, which should reset the detail fragment and clear back stack.
        boolean shown =
                mSettingsHostFragment.showFragment(
                        null, /* addToBackStack= */ false, /* tag= */ null);
        assertTrue("showFragment should succeed for null fragment", shown);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();

        assertEquals(0, multiColumnSettings.getChildFragmentManager().getBackStackEntryCount());
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void testShowFragment_NullFragment_SingleColumn_RemovesDetailFragment() {
        DeviceInfo.setIsDesktopForTesting(true);
        mSettingsHostFragment = new TestSingleColumnMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);

        // Show a detail fragment.
        multiColumnSettings.showDetailFragment(
                new SecondFakeSettingsFragment(), /* addToBackStack= */ false, /* tag= */ null);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();
        assertNotNull(
                multiColumnSettings
                        .getChildFragmentManager()
                        .findFragmentById(R.id.preferences_detail));

        // Now show null fragment, which in single-column mode should remove the detail fragment.
        boolean shown =
                mSettingsHostFragment.showFragment(
                        null, /* addToBackStack= */ false, /* tag= */ null);
        assertTrue("showFragment should succeed for null fragment", shown);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();

        assertNull(
                "Detail fragment should be removed in single column mode",
                multiColumnSettings
                        .getChildFragmentManager()
                        .findFragmentById(R.id.preferences_detail));
    }

    @Test
    @Config(qualifiers = "w320dp")
    public void testIsTwoColumn_UnlaidOutFallback_NarrowDisplay() {
        DeviceInfo.setIsDesktopForTesting(true);
        mSettingsHostFragment = new TestMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);
        assertFalse(
                "isTwoColumn should return false on narrow display before layout pass",
                multiColumnSettings.isTwoColumn());
    }

    @Test
    @Config(qualifiers = "w1024dp")
    public void testIsTwoColumn_UnlaidOutFallback_WideDisplay() {
        DeviceInfo.setIsDesktopForTesting(true);
        mSettingsHostFragment = new TestMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);
        assertTrue(
                "isTwoColumn should return true on wide display before layout pass",
                multiColumnSettings.isTwoColumn());
    }

    @Test
    public void testShowFragment_MainSettingsFragment_ResetsDetailFragment() {
        mSettingsHostFragment = new TestMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);

        // First show a detail fragment with addToBackStack=true.
        multiColumnSettings.showDetailFragment(
                new SecondFakeSettingsFragment(), /* addToBackStack= */ true, /* tag= */ null);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();
        assertEquals(1, multiColumnSettings.getChildFragmentManager().getBackStackEntryCount());

        // Now show MainSettings fragment, which should reset the detail fragment and clear back
        // stack.
        boolean shown =
                mSettingsHostFragment.showFragment(
                        new TestMainSettings(), /* addToBackStack= */ false, /* tag= */ null);
        assertTrue("showFragment should succeed for MainSettings fragment", shown);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();

        assertEquals(0, multiColumnSettings.getChildFragmentManager().getBackStackEntryCount());
    }

    /** Subclass SettingsHostFragment to mock initial fragment instantiation. */
    public static class TestMultiColumnSettingsHostFragment extends SettingsHostFragment {
        @Override
        protected Fragment createInitialFragment(@Nullable Intent intent) {
            return new TestMultiColumnSettings();
        }
    }

    /** Subclass SettingsHostFragment for single column mode with null initial detail. */
    public static class TestSingleColumnMultiColumnSettingsHostFragment
            extends SettingsHostFragment {
        @Override
        protected Fragment createInitialFragment(@Nullable Intent intent) {
            return new TestSingleColumnMultiColumnSettings();
        }
    }

    @Test
    public void testFinishCurrentSettings() {
        attachHostFragment();
        mSettingsHostFragment.showFragment(
                new SecondFakeSettingsFragment(), /* addToBackStack= */ false, /* tag= */ null);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();
        Fragment active = mSettingsHostFragment.getActiveFragment();
        assertTrue(active instanceof SecondFakeSettingsFragment);

        // Finishing settings returns to the main settings UI.
        mSettingsHostFragment.finishCurrentSettings(active);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();
        assertTrue(mSettingsHostFragment.getActiveFragment() instanceof FirstFakeSettingsFragment);
    }

    @Test
    public void testFinishCurrentSettings_MultiColumnSettings_TwoColumnMode() {
        // Create a MultiColumnSettings with a FirstFakeSettingsFragment as the initial fragment.
        mSettingsHostFragment = new TestMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);

        // Show a detail fragment in two-column mode.
        multiColumnSettings.showDetailFragment(
                new SecondFakeSettingsFragment(), /* addToBackStack= */ false, /* tag= */ null);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();

        Fragment active = mSettingsHostFragment.getMainFragment();
        assertTrue(active instanceof SecondFakeSettingsFragment);

        // Finishing settings should reset the detail fragment to the initial detail fragment.
        mSettingsHostFragment.finishCurrentSettings(active);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();
        assertTrue(mSettingsHostFragment.getMainFragment() instanceof FirstFakeSettingsFragment);
    }

    /**
     * Tests that finishing a detail settings fragment in single-column mode closes the sliding pane
     * and removes the detail fragment, returning to the MainSettings page.
     */
    @Test
    @Config(qualifiers = "w320dp")
    public void testFinishCurrentSettings_MultiColumnSettings_SingleColumnMode() {
        DeviceInfo.setIsDesktopForTesting(true);
        mSettingsHostFragment = new TestSingleColumnMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);

        SecondFakeSettingsFragment detailFragment = new SecondFakeSettingsFragment();
        multiColumnSettings.showDetailFragment(
                detailFragment, /* addToBackStack= */ false, /* tag= */ null);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();
        assertNotNull(
                multiColumnSettings
                        .getChildFragmentManager()
                        .findFragmentById(R.id.preferences_detail));

        mSettingsHostFragment.finishCurrentSettings(detailFragment);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();

        assertNull(
                "Detail fragment should be removed in single column mode",
                multiColumnSettings
                        .getChildFragmentManager()
                        .findFragmentById(R.id.preferences_detail));
    }

    /**
     * Tests that SettingsHostFragment.get(Fragment) correctly resolves the host fragment from any
     * child fragment within the settings hierarchy.
     */
    @Test
    public void testGet_FromFragment() {
        mSettingsHostFragment = new TestMultiColumnSettingsHostFragment();
        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(
                        android.R.id.content,
                        mSettingsHostFragment,
                        SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        MultiColumnSettings multiColumnSettings =
                (MultiColumnSettings) mSettingsHostFragment.getActiveFragment();
        assertNotNull(multiColumnSettings);

        SecondFakeSettingsFragment detailFragment = new SecondFakeSettingsFragment();
        multiColumnSettings.showDetailFragment(
                detailFragment, /* addToBackStack= */ false, /* tag= */ null);
        multiColumnSettings.getChildFragmentManager().executePendingTransactions();

        assertEquals(mSettingsHostFragment, SettingsHostFragment.get(detailFragment));
        assertEquals(mSettingsHostFragment, SettingsHostFragment.get(multiColumnSettings));
        assertNull(SettingsHostFragment.get(new SecondFakeSettingsFragment()));
        assertNull(SettingsHostFragment.get((Fragment) null));
    }

    @Test
    public void testSetDependencyProvider_whenNotAdded_defersRegistrationUntilAttached() {
        SettingsHostFragment fragment = new TestSettingsHostFragment();
        FragmentDependencyProvider mockProvider = mock(FragmentDependencyProvider.class);

        // setDependencyProvider should not throw when fragment is unattached
        fragment.setDependencyProvider(mockProvider);

        mActivity
                .getSupportFragmentManager()
                .beginTransaction()
                .add(android.R.id.content, fragment, SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG)
                .commitNow();

        assertNotNull(fragment.getChildFragmentManager());
    }

    @Test
    public void testOnAttach_registersWideDisplayPaddingApplier() {
        attachHostFragment();
        TestPreferenceFragment fragment = new TestPreferenceFragment();
        mSettingsHostFragment.showFragment(fragment, /* addToBackStack= */ false, /* tag= */ null);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();

        View view = fragment.getView();
        assertNotNull(view);
        RecyclerView recyclerView = view.findViewById(R.id.recycler_view);
        assertNotNull(recyclerView);

        // Trigger global layout to execute WideDisplayPadding.apply()
        view.getViewTreeObserver().dispatchOnGlobalLayout();

        // WideDisplayPadding.apply should have added PaddedItemDecorationWithDivider
        boolean hasPaddedDecoration = false;
        for (int i = 0; i < recyclerView.getItemDecorationCount(); i++) {
            if (recyclerView.getItemDecorationAt(i) instanceof PaddedItemDecorationWithDivider) {
                hasPaddedDecoration = true;
                break;
            }
        }
        assertTrue(
                "WideDisplayPadding should add PaddedItemDecorationWithDivider",
                hasPaddedDecoration);
    }

    @Test
    public void testSetDependencyProvider_appliesDependenciesToExistingChildFragments() {
        attachHostFragment();
        TestMainSettings mainSettings = new TestMainSettings();
        mSettingsHostFragment.showFragment(
                mainSettings, /* addToBackStack= */ false, /* tag= */ null);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();

        FragmentDependencyProvider mockProvider = mock(FragmentDependencyProvider.class);
        mSettingsHostFragment.setDependencyProvider(mockProvider);

        verify(mockProvider)
                .attachDependencies(mSettingsHostFragment.getChildFragmentManager(), mainSettings);
    }

    @Test
    public void testActivityRecreation_themeSwitch_populatesDependenciesOnRestoredFragments() {
        attachHostFragment();
        TestMainSettings mainSettings = new TestMainSettings();
        mSettingsHostFragment.showFragment(
                mainSettings, /* addToBackStack= */ false, /* tag= */ null);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();

        // Simulate activity recreation (e.g. OS theme switch or screen rotation).
        mActivityScenarios.getScenario().recreate();

        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            var manager = activity.getSupportFragmentManager();
                            SettingsHostFragment restoredHost =
                                    (SettingsHostFragment)
                                            manager.findFragmentByTag(
                                                    SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG);
                            assertNotNull("Restored host fragment should exist", restoredHost);

                            Fragment restoredChild = restoredHost.getActiveFragment();
                            assertNotNull("Restored child fragment should exist", restoredChild);
                            assertTrue(
                                    "Restored child fragment should be MainSettings",
                                    restoredChild instanceof MainSettings);

                            MainSettings restoredMain = (MainSettings) restoredChild;
                            assertNotNull("Profile should be set", restoredMain.getProfile());
                            assertNotNull(
                                    "ModalDialogManagerSupplier should be set",
                                    restoredMain.getModalDialogManagerSupplierForTesting());
                            assertNotNull(
                                    "ModalDialogManager should be present",
                                    restoredMain.getModalDialogManagerSupplierForTesting().get());
                        });
    }

    @Test
    public void testActivityRecreation_siteSettings_populatesDependenciesOnRestoredFragments() {
        attachHostFragment();
        TestSiteSettingsFragment siteSettingsFragment = new TestSiteSettingsFragment();
        mSettingsHostFragment.showFragment(
                siteSettingsFragment, /* addToBackStack= */ false, /* tag= */ null);
        mSettingsHostFragment.getChildFragmentManager().executePendingTransactions();

        // Simulate activity recreation (e.g. font size change, OS theme switch).
        mActivityScenarios.getScenario().recreate();

        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            var manager = activity.getSupportFragmentManager();
                            SettingsHostFragment restoredHost =
                                    (SettingsHostFragment)
                                            manager.findFragmentByTag(
                                                    SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG);
                            assertNotNull("Restored host fragment should exist", restoredHost);

                            Fragment restoredChild = restoredHost.getActiveFragment();
                            assertNotNull("Restored child fragment should exist", restoredChild);
                            assertTrue(
                                    "Restored child fragment should be BaseSiteSettingsFragment",
                                    restoredChild instanceof BaseSiteSettingsFragment);

                            BaseSiteSettingsFragment restoredSiteSettings =
                                    (BaseSiteSettingsFragment) restoredChild;
                            assertTrue(
                                    "SiteSettingsDelegate should be set",
                                    restoredSiteSettings.hasSiteSettingsDelegate());
                        });
    }

    @Test
    public void testOnConfigurationChanged_updatesContainment() {
        attachHostFragment();
        SettingsContainmentHelper mockHelper = mock(SettingsContainmentHelper.class);
        mSettingsHostFragment.setContainmentHelperForTesting(mockHelper);

        mSettingsHostFragment.onConfigurationChanged(new Configuration());

        verify(mockHelper).updateContainmentForAttachedFragments(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV)
    public void testSettingsNavigationFactory_createSettingsNavigation() {
        attachHostFragment();
        SettingsNavigation mockNavigation = mock(SettingsNavigation.class);
        mSettingsHostFragment.setSettingsNavigation(mockNavigation);

        SettingsNavigation resolved = SettingsNavigationFactory.createSettingsNavigation(mActivity);
        assertEquals(
                "Should resolve the tab-scoped SettingsNavigation when host is attached and URL nav"
                        + " is enabled",
                mockNavigation,
                resolved);

        SettingsNavigation nullResolved = SettingsNavigationFactory.createSettingsNavigation(null);
        assertNotNull("Should fallback to default instance when context is null", nullResolved);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV)
    public void testSettingsNavigationFactory_createSettingsNavigation_urlNavDisabled() {
        attachHostFragment();
        SettingsNavigation mockNavigation = mock(SettingsNavigation.class);
        mSettingsHostFragment.setSettingsNavigation(mockNavigation);

        SettingsNavigation resolved = SettingsNavigationFactory.createSettingsNavigation(mActivity);
        assertNotNull(resolved);
        assertNotEquals(
                "Should not resolve tab-scoped delegate when URL nav is disabled",
                mockNavigation,
                resolved);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SETTINGS_IN_TAB_URL_NAV)
    public void testOnPreferenceStartFragment_DelegatesToSettingsNavigation() {
        attachHostFragment();
        SettingsNavigation mockNavigation = mock(SettingsNavigation.class);
        mSettingsHostFragment.setSettingsNavigation(mockNavigation);

        Preference preference = mock(Preference.class);
        when(preference.getFragment()).thenReturn(SecondFakeSettingsFragment.class.getName());
        Bundle extras = new Bundle();
        extras.putString("test_key", "test_value");
        when(preference.getExtras()).thenReturn(extras);

        PreferenceFragmentCompat caller = mock(PreferenceFragmentCompat.class);
        boolean handled = mSettingsHostFragment.onPreferenceStartFragment(caller, preference);

        assertTrue("Preference start fragment should be handled", handled);
        verify(mockNavigation)
                .startSettings(
                        mSettingsHostFragment.getContext(),
                        SecondFakeSettingsFragment.class,
                        extras);
    }

    @Test
    public void testSaveAndRestoreInstanceState_invokesCallbackAndStoresBundle() {
        attachHostFragment();
        mSettingsHostFragment.setSaveInstanceStateCallback(
                bundle -> bundle.putString("test_key", "test_value"));

        Bundle savedState = new Bundle();
        mSettingsHostFragment.onSaveInstanceState(savedState);
        assertEquals("test_value", savedState.getString("test_key"));

        // Simulate activity recreation with saved state.
        mActivityScenarios.getScenario().recreate();
        mActivityScenarios
                .getScenario()
                .onActivity(
                        activity -> {
                            var manager = activity.getSupportFragmentManager();
                            SettingsHostFragment restoredHost =
                                    (SettingsHostFragment)
                                            manager.findFragmentByTag(
                                                    SettingsHostFragment.SETTINGS_NATIVE_PAGE_TAG);
                            assertNotNull(restoredHost);
                            Bundle restoredState = restoredHost.getSavedInstanceState();
                            assertNotNull(restoredState);
                            assertEquals("test_value", restoredState.getString("test_key"));
                        });
    }

    /** A test PreferenceFragmentCompat subclass. */
    public static class TestPreferenceFragment extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            Context context = getPreferenceManager().getContext();
            PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(context);
            setPreferenceScreen(screen);
        }
    }

    /** Fake settings fragment for testing. */
    public static class FirstFakeSettingsFragment extends Fragment {
        public FirstFakeSettingsFragment() {}
    }

    /** Another fake settings fragment for testing transitions. */
    public static class SecondFakeSettingsFragment extends Fragment {
        public SecondFakeSettingsFragment() {}
    }

    public static class TestHeaderFragment extends PreferenceFragmentCompat {
        public TestHeaderFragment() {}

        @Override
        public void onCreatePreferences(
                @Nullable Bundle savedInstanceState, @Nullable String rootKey) {
            setPreferenceScreen(getPreferenceManager().createPreferenceScreen(requireContext()));
        }
    }

    /** Subclass of MainSettings to inject dependencies and avoid native calls. */
    public static class TestMainSettings extends MainSettings {
        private final Profile mMockProfile = mock(Profile.class);

        public TestMainSettings() {
            setProfile(mMockProfile);
            setSkipUpdatePreferencesForTesting(true);
        }

        @Override
        public Profile getProfile() {
            return mMockProfile;
        }

        @Override
        public void onCreatePreferences(
                @Nullable Bundle savedInstanceState, @Nullable String rootKey) {
            setPreferenceScreen(getPreferenceManager().createPreferenceScreen(requireContext()));
        }
    }

    /** Subclass of MultiColumnSettings to inject dependencies and avoid native calls. */
    public static class TestMultiColumnSettings extends MultiColumnSettings {
        public TestMultiColumnSettings() {}

        @Override
        public PreferenceFragmentCompat onCreatePreferenceHeader() {
            return new TestHeaderFragment();
        }

        @Override
        public Fragment onCreateInitialDetailFragment() {
            return new FirstFakeSettingsFragment();
        }
    }

    /** Subclass of MultiColumnSettings for single column mode. */
    public static class TestSingleColumnMultiColumnSettings extends MultiColumnSettings {
        public TestSingleColumnMultiColumnSettings() {}

        @Override
        public PreferenceFragmentCompat onCreatePreferenceHeader() {
            return new TestHeaderFragment();
        }
    }

    /** Subclass of BaseSiteSettingsFragment to test dependency injection on restore. */
    public static class TestSiteSettingsFragment extends BaseSiteSettingsFragment {
        public TestSiteSettingsFragment() {}

        @Override
        public void onCreatePreferences(
                @Nullable Bundle savedInstanceState, @Nullable String rootKey) {
            setPreferenceScreen(getPreferenceManager().createPreferenceScreen(requireContext()));
        }
    }
}
