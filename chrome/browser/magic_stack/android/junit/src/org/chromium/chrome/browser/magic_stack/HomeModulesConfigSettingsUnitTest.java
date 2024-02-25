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
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link HomeModulesConfigSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeModulesConfigSettingsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ActivityScenario<TestActivity> mActivityScenario;
    private TestActivity mActivity;
    @Mock private Profile mProfile;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;

    @Before
    public void setUp() {
        mActivityScenario = ActivityScenario.launch(TestActivity.class);
        mActivityScenario.onActivity(
                activity -> {
                    mActivity = activity;
                });
    }

    @After
    public void tearDown() {
        mActivityScenario.close();
    }

    @Test
    @SmallTest
    public void testLaunchHomeModulesConfigSettings() {
        List<Integer> moduleTypeShownInSettingsForTest = List.of(1);
        when(mHomeModulesConfigManager.getModuleListShownInSettings())
                .thenReturn(moduleTypeShownInSettingsForTest);
        when(mHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE))
                .thenReturn(true);

        String singleTabNotExistedPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(0));
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(1));
        when(mHomeModulesConfigManager.getSettingsPreferenceKey(ModuleType.PRICE_CHANGE))
                .thenReturn(priceChangePreferenceKey);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);

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
    }

    @Test
    @SmallTest
    public void testLaunchHomeModulesConfigSettingsWithBlankPage() {
        List<Integer> moduleTypeShownInSettingsForTest = new ArrayList<>();
        when(mHomeModulesConfigManager.getModuleListShownInSettings())
                .thenReturn(moduleTypeShownInSettingsForTest);
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(1));
        when(mHomeModulesConfigManager.getSettingsPreferenceKey(ModuleType.PRICE_CHANGE))
                .thenReturn(priceChangePreferenceKey);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);

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
}
