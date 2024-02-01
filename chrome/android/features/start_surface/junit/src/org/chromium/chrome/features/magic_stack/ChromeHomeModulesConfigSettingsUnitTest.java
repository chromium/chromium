// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.magic_stack;

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
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.features.magic_stack.ChromeHomeModulesConfigSettingsUnitTest.ShadowModuleRegistry;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.ui.base.TestActivity;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/** Unit tests for {@link ChromeHomeModulesConfigSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowModuleRegistry.class})
public class ChromeHomeModulesConfigSettingsUnitTest {
    /** Shadow class for {@link ModuleRegistry} */
    @Implements(ModuleRegistry.class)
    public static class ShadowModuleRegistry {
        @Mock private static ModuleRegistry sModuleRegistry;

        @Implementation
        public static ModuleRegistry getInstance() {
            return sModuleRegistry;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private ActivityScenario<TestActivity> mActivityScenario;
    private TestActivity mActivity;
    @Mock private Profile mProfile;
    @Mock private ModuleRegistry mMockModuleRegistry;

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
    @Feature({"ChromeHomepage"})
    public void testLaunchChromeHomepageSettings() {
        ShadowModuleRegistry.sModuleRegistry = mMockModuleRegistry;
        Set<Integer> moduleTypeRegisteredForTest = new HashSet<>(Arrays.asList(0, 1));
        when(mMockModuleRegistry.getRegisteredModuleTypes())
                .thenReturn(moduleTypeRegisteredForTest);
        when(mMockModuleRegistry.isModuleConfigurable(ModuleType.SINGLE_TAB)).thenReturn(false);
        when(mMockModuleRegistry.isModuleEligibleToBuild(ModuleType.SINGLE_TAB)).thenReturn(true);
        when(mMockModuleRegistry.isModuleEligibleToBuild(ModuleType.PRICE_CHANGE)).thenReturn(true);
        when(mMockModuleRegistry.isModuleConfigurable(ModuleType.PRICE_CHANGE)).thenReturn(true);

        String singleTabNotExistedPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(0));
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(1));
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        sharedPreferencesManager.writeBoolean(priceChangePreferenceKey, true);

        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        ChromeHomeModulesConfigSettings fragment =
                (ChromeHomeModulesConfigSettings)
                        fragmentManager
                                .getFragmentFactory()
                                .instantiate(
                                        ChromeHomeModulesConfigSettings.class.getClassLoader(),
                                        ChromeHomeModulesConfigSettings.class.getName());
        fragment.setProfile(mProfile);
        fragmentManager.beginTransaction().replace(android.R.id.content, fragment).commit();
        mActivityScenario.moveToState(State.STARTED);

        ChromeSwitchPreference switchNotExisted =
                fragment.findPreference(singleTabNotExistedPreferenceKey);
        Assert.assertNull(switchNotExisted);

        ChromeSwitchPreference switchExisted = fragment.findPreference(priceChangePreferenceKey);
        Assert.assertEquals(
                mActivity.getString(R.string.price_change_module_context_menu_item),
                switchExisted.getTitle());
        Assert.assertTrue(switchExisted.isChecked());
    }
}
