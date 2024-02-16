// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager.HomeModulesStateListener;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link HomeModulesConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeModulesConfigManagerUnitTest {
    private List<ModuleConfigChecker> mModuleConfigCheckerList = new ArrayList<>();
    private HomeModulesConfigManager mHomeModulesConfigManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mHomeModulesConfigManager = HomeModulesConfigManager.getInstance();
    }

    @Test
    public void testSetAndGetPrefModuleTypeEnabled() {
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(1));

        HomeModulesStateListener listener = Mockito.mock(HomeModulesStateListener.class);
        mHomeModulesConfigManager.addListener(listener);

        ChromeSharedPreferences.getInstance().writeBoolean(priceChangePreferenceKey, true);
        assertTrue(mHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE));

        ChromeSharedPreferences.getInstance().writeBoolean(priceChangePreferenceKey, false);
        Assert.assertFalse(
                mHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE));

        mHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, true);
        assertTrue(
                ChromeSharedPreferences.getInstance().readBoolean(priceChangePreferenceKey, true));
        verify(listener).onModuleConfigChanged(eq(ModuleType.PRICE_CHANGE), eq(true));

        mHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, false);
        Assert.assertFalse(
                ChromeSharedPreferences.getInstance().readBoolean(priceChangePreferenceKey, true));
        verify(listener).onModuleConfigChanged(eq(ModuleType.PRICE_CHANGE), eq(false));
    }

    @Test
    public void testGetEnabledModuleList() {
        registerModuleConfigChecker(2);

        // Verifies that:
        // 1) a not configurable module is always enabled.
        // 2) a configurable module needs to a) eligible to build and b) enabled in
        // settings to be treated as enabled.
        when(mModuleConfigCheckerList.get(0).isConfigurable()).thenReturn(false);
        when(mModuleConfigCheckerList.get(1).isConfigurable()).thenReturn(true);
        when(mModuleConfigCheckerList.get(1).isEligible()).thenReturn(true);
        mHomeModulesConfigManager.setPrefModuleTypeEnabled(1, true);
        assertTrue(mHomeModulesConfigManager.getPrefModuleTypeEnabled(1));
        Set<Integer> expectedSet = Set.of(0, 1);
        assertEquals(expectedSet, mHomeModulesConfigManager.getEnabledModuleSet());

        when(mModuleConfigCheckerList.get(1).isEligible()).thenReturn(false);
        expectedSet = Set.of(0);
        assertEquals(expectedSet, mHomeModulesConfigManager.getEnabledModuleSet());

        when(mModuleConfigCheckerList.get(1).isEligible()).thenReturn(true);
        mHomeModulesConfigManager.setPrefModuleTypeEnabled(1, false);
        assertFalse(mHomeModulesConfigManager.getPrefModuleTypeEnabled(1));
        expectedSet = Set.of(0);
        assertEquals(expectedSet, mHomeModulesConfigManager.getEnabledModuleSet());
    }

    @Test
    public void testGetModuleListShownInSettings() {
        registerModuleConfigChecker(2);

        // Verifies that there isn't any module shown in the settings.
        when(mModuleConfigCheckerList.get(0).isConfigurable()).thenReturn(false);
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(true);
        when(mModuleConfigCheckerList.get(1).isConfigurable()).thenReturn(true);
        when(mModuleConfigCheckerList.get(1).isEligible()).thenReturn(false);
        assertTrue(mHomeModulesConfigManager.getModuleListShownInSettings().isEmpty());

        // Verifies the list contains the module which is configurable and eligible to build.
        when(mModuleConfigCheckerList.get(1).isEligible()).thenReturn(true);
        List<Integer> expectedList = List.of(1);
        assertEquals(expectedList, mHomeModulesConfigManager.getModuleListShownInSettings());
    }

    @Test
    public void testHasModuleShownInSettings() {
        registerModuleConfigChecker(2);

        // Verifies that there isn't any module shown in the settings.
        when(mModuleConfigCheckerList.get(0).isConfigurable()).thenReturn(false);
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(true);
        when(mModuleConfigCheckerList.get(1).isConfigurable()).thenReturn(true);
        when(mModuleConfigCheckerList.get(1).isEligible()).thenReturn(false);
        assertFalse(mHomeModulesConfigManager.hasModuleShownInSettings());

        // Verifies the list contains the module which is configurable and eligible to build.
        when(mModuleConfigCheckerList.get(1).isEligible()).thenReturn(true);
        assertTrue(mHomeModulesConfigManager.hasModuleShownInSettings());
    }

    @Test
    public void testFreshnessCount() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;
        String moduleFreshnessCountPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_FRESHNESS_COUNT.createKey(
                        String.valueOf(moduleType));
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();

        assertFalse(sharedPreferencesManager.contains(moduleFreshnessCountPreferenceKey));
        assertEquals(0, sharedPreferencesManager.readInt(moduleFreshnessCountPreferenceKey, 0));

        int count = 5;
        mHomeModulesConfigManager.increaseFreshnessCount(moduleType, count);
        assertEquals(count, sharedPreferencesManager.readInt(moduleFreshnessCountPreferenceKey, 0));

        mHomeModulesConfigManager.resetFreshnessCount(moduleType);
        assertTrue(sharedPreferencesManager.contains(moduleFreshnessCountPreferenceKey));
        assertEquals(0, sharedPreferencesManager.readInt(moduleFreshnessCountPreferenceKey, 0));
    }

    private void registerModuleConfigChecker(int size) {
        size = Math.min(size, ModuleType.NUM_ENTRIES);
        for (int i = 0; i < size; i++) {
            ModuleConfigChecker moduleConfigChecker = Mockito.mock(ModuleConfigChecker.class);
            mModuleConfigCheckerList.add(moduleConfigChecker);
            mHomeModulesConfigManager.registerModuleEligibilityChecker(i, moduleConfigChecker);
        }
    }
}
