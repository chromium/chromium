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

import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link SettingsHostFragment}. */
@RunWith(BaseRobolectricTestRunner.class)
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
            return new FakeSettingsFragment();
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
                .add(android.R.id.content, mSettingsHostFragment)
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
                "Initial fragment should be FakeSettingsFragment",
                current instanceof FakeSettingsFragment);
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

    /** Fake settings fragment for testing. */
    public static class FakeSettingsFragment extends Fragment {
        public FakeSettingsFragment() {}
    }

    /** Another fake settings fragment for testing transitions. */
    public static class SecondFakeSettingsFragment extends Fragment {
        public SecondFakeSettingsFragment() {}
    }
}
