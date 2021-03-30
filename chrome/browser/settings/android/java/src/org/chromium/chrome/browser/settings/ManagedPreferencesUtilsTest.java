// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.settings.R;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests of {@link ManagedPreferencesUtils}.
 *
 * TODO(crbug.com/1166810): Move these tests to //components/browser_ui/settings/.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ManagedPreferencesUtilsTest {
    @Rule
    public BaseActivityTestRule<SettingsActivity> mRule =
            new BaseActivityTestRule<>(SettingsActivity.class);

    private Context mContext;

    public static final ManagedPreferenceDelegate UNMANAGED_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return false;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }
            };

    public static final ManagedPreferenceDelegate POLICY_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return true;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return false;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }
            };

    public static final ManagedPreferenceDelegate SINGLE_CUSTODIAN_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return true;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return false;
                }
            };

    public static final ManagedPreferenceDelegate MULTI_CUSTODIAN_DELEGATE =
            new ManagedPreferenceDelegate() {
                @Override
                public boolean isPreferenceControlledByPolicy(Preference preference) {
                    return false;
                }

                @Override
                public boolean isPreferenceControlledByCustodian(Preference preference) {
                    return true;
                }

                @Override
                public boolean doesProfileHaveMultipleCustodians() {
                    return true;
                }
            };

    @Before
    public void setUp() {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        Intent intent = settingsLauncher.createSettingsActivityIntent(
                InstrumentationRegistry.getInstrumentation().getContext(),
                DummySettingsForTest.class.getName());
        mRule.launchActivity(intent);

        PreferenceFragmentCompat fragment =
                (PreferenceFragmentCompat) mRule.getActivity().getMainFragment();
        mContext = fragment.getPreferenceScreen().getContext();
    }

    @Test
    @SmallTest
    public void testShowManagedByAdministratorToast() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedByAdministratorToast(mRule.getActivity());
        });

        onView(withText(R.string.managed_by_your_organization))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastNullDelegate() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedByParentToast(mRule.getActivity(), null);
        });

        onView(withText(R.string.managed_by_your_parent))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastSingleCustodian() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedByParentToast(
                    mRule.getActivity(), SINGLE_CUSTODIAN_DELEGATE);
        });

        onView(withText(R.string.managed_by_your_parent))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowManagedByParentToastMultipleCustodians() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedByParentToast(
                    mRule.getActivity(), MULTI_CUSTODIAN_DELEGATE);
        });

        onView(withText(R.string.managed_by_your_parents))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowManagedSettingsCannotBeResetToast() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ManagedPreferencesUtils.showManagedSettingsCannotBeResetToast(mRule.getActivity());
        });

        onView(withText(R.string.managed_settings_cannot_be_reset))
                .inRoot(withDecorView(not(mRule.getActivity().getWindow().getDecorView())))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdNull() {
        Preference pref = new Preference(mContext);
        int actual = ManagedPreferencesUtils.getManagedIconResId(null, pref);
        Assert.assertEquals(0, actual);
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdPolicy() {
        Preference pref = new Preference(mContext);
        int expected = ManagedPreferencesUtils.getManagedByEnterpriseIconId();
        int actual = ManagedPreferencesUtils.getManagedIconResId(POLICY_DELEGATE, pref);
        Assert.assertEquals(expected, actual);
    }

    @Test
    @SmallTest
    public void testGetManagedIconIdCustodian() {
        Preference pref = new Preference(mContext);
        int expected = ManagedPreferencesUtils.getManagedByCustodianIconId();
        int actual = ManagedPreferencesUtils.getManagedIconResId(SINGLE_CUSTODIAN_DELEGATE, pref);
        Assert.assertEquals(expected, actual);
    }
}
