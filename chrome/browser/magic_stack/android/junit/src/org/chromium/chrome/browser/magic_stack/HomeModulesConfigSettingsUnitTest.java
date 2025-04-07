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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
        mHomeModulesConfigManager.cleanupForTesting();
        mActivityScenario.close();
    }

    @Test
    @SmallTest
    public void testLaunchHomeModulesConfigSettings() {
        registerModuleConfigChecker(3);

        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.PRICE_CHANGE));

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

        ChromeSwitchPreference switchExisted = fragment.findPreference(priceChangePreferenceKey);
        Assert.assertEquals(
                mActivity.getString(R.string.price_change_module_name), switchExisted.getTitle());
        Assert.assertTrue(switchExisted.isChecked());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.EDUCATIONAL_TIP_MODULE,
        ChromeFeatureList.SEGMENTATION_PLATFORM_EPHEMERAL_CARD_RANKER
    })
    public void testLaunchHomeModulesConfigSettingsForEducationalTipModules() {
        registerModuleConfigChecker(10);

        String tabGroupPromoNotExistedPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.TAB_GROUP_PROMO));
        String defaultBrowserPromoPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.DEFAULT_BROWSER_PROMO));

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
                fragment.findPreference(tabGroupPromoNotExistedPreferenceKey);
        Assert.assertNull(switchNotExisted);

        ChromeSwitchPreference switchExisted =
                fragment.findPreference(defaultBrowserPromoPreferenceKey);
        Assert.assertEquals(
                mActivity.getString(R.string.educational_tip_module_name),
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
            if (i == ModuleType.DEPRECATED_EDUCATIONAL_TIP
                    || i == ModuleType.DEPRECATED_TAB_RESUMPTION) {
                continue;
            }

            ModuleConfigChecker moduleConfigChecker = Mockito.mock(ModuleConfigChecker.class);
            when(moduleConfigChecker.isEligible()).thenReturn(true);
            mHomeModulesConfigManager.registerModuleEligibilityChecker(i, moduleConfigChecker);
        }
    }
}
