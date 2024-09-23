// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.mockito.Mockito.when;

import androidx.fragment.app.FragmentManager;
import androidx.lifecycle.Lifecycle.State;
import androidx.test.core.app.ActivityScenario;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link HomeModulesConfigSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeModulesConfigSettingsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ActivityScenario<TestActivity> mActivityScenario;
    private TestActivity mActivity;
    @Mock private Profile mProfile;
    private HomeModulesConfigManager mHomeModulesConfigManager;

    @Before
    public void setUp() {
        mActivityScenario = ActivityScenario.launch(TestActivity.class);
        mActivityScenario.onActivity(
                activity -> {
                    mActivity = activity;
                });
        mHomeModulesConfigManager = HomeModulesConfigManager.getInstance();
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
    }

    @Test
    @SmallTest
    public void testLaunchHomeModulesConfigSettings() {
        registerModuleConfigChecker(3);

        String singleTabNotExistedPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.SINGLE_TAB));
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.PRICE_CHANGE));
        String tabResumptionPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.TAB_RESUMPTION));

        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        HomeModulesConfigSettings fragment =
                (HomeModulesConfigSettings)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        HomeModulesConfigSettings.class.getClassLoader(),
                                        HomeModulesConfigSettings.class.getName());
        fragment.setProfile(mProfile);
        fragmentManager.beginTransaction().replace(android.R.id.content, fragment).commit();
        mActivityScenario.moveToState(State.STARTED);

        ChromeSwitchPreference switchNotExisted =
                fragment.findPreference(singleTabNotExistedPreferenceKey);
        Assert.assertNull(switchNotExisted);

        ChromeSwitchPreference switchExisted = fragment.findPreference(priceChangePreferenceKey);
        Assert.assertEquals(
                mActivity.getString(R.string.price_change_module_name), switchExisted.getTitle());
        Assert.assertTrue(switchExisted.isChecked());

        switchExisted = fragment.findPreference(tabResumptionPreferenceKey);
        Assert.assertEquals(
                mActivity
                        .getResources()
                        .getQuantityString(R.plurals.home_modules_tab_resumption_title, 1),
                switchExisted.getTitle());
        Assert.assertTrue(switchExisted.isChecked());
    }

    @Test
    @SmallTest
    public void testLaunchHomeModulesConfigSettingsWithBlankPage() {
        ModuleConfigChecker moduleConfigChecker = Mockito.mock(ModuleConfigChecker.class);
        when(moduleConfigChecker.isEligible()).thenReturn(false);
        mHomeModulesConfigManager.registerModuleEligibilityChecker(0, moduleConfigChecker);

        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        HomeModulesConfigSettings fragment =
                (HomeModulesConfigSettings)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        HomeModulesConfigSettings.class.getClassLoader(),
                                        HomeModulesConfigSettings.class.getName());
        fragment.setProfile(mProfile);
        fragmentManager.beginTransaction().replace(android.R.id.content, fragment).commit();
        mActivityScenario.moveToState(State.STARTED);

        Assert.assertTrue(fragment.isHomeModulesConfigSettingsEmptyForTesting());
    }

    private void registerModuleConfigChecker(int size) {
        size = Math.min(size, ModuleType.NUM_ENTRIES);
        for (int i = 0; i < size; i++) {
            ModuleConfigChecker moduleConfigChecker = Mockito.mock(ModuleConfigChecker.class);
            when(moduleConfigChecker.isEligible()).thenReturn(true);
            mHomeModulesConfigManager.registerModuleEligibilityChecker(i, moduleConfigChecker);
        }
    }
}
