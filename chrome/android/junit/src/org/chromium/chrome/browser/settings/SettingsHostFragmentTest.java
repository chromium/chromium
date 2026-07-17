// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.os.Bundle;
import android.view.View;

import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SettingsHostFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(qualifiers = "sw600dp")
@EnableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.SETTINGS_MULTI_COLUMN})
public class SettingsHostFragmentTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    private TestActivity mActivity;
    private SettingsHostFragment mSettingsHostFragment;

    /** Subclass SettingsHostFragment to mock initial fragment instantiation. */
    public static class TestSettingsHostFragment extends SettingsHostFragment {
        @Override
        protected Fragment createInitialFragment() {
            return new FirstFakeSettingsFragment();
        }
    }

    @Before
    public void setUp() {
        mActivityScenarios
                .getScenario()
                .onActivity(activity -> mActivity = (TestActivity) activity);
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

    @Test(expected = AssertionError.class)
    @DisableFeatures(ChromeFeatureList.SETTINGS_IN_TAB)
    public void testConstructor_SettingsInTabDisabled_ThrowsAssertionError() {
        // Should throw.
        new SettingsHostFragment();
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
        assertEquals(R.style.Theme_Chromium_Settings, context.getThemeResId());
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

        Fragment initial = fragment.createInitialFragment();
        assertTrue(
                "Initial fragment should be MultiColumnSettings",
                initial instanceof MultiColumnSettings);
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
}
