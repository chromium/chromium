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

import android.text.TextUtils;

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
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.PRICE_CHANGE));

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
        registerModuleConfigChecker(1);

        // Verifies that a module is enabled if it is eligible to build and is enabled in settings.
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(false);
        mHomeModulesConfigManager.setPrefModuleTypeEnabled(0, true);
        assertTrue(mHomeModulesConfigManager.getPrefModuleTypeEnabled(0));
        assertTrue(mHomeModulesConfigManager.getEnabledModuleSet().isEmpty());

        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(true);
        Set<Integer> expectedSet = Set.of(0);
        assertEquals(expectedSet, mHomeModulesConfigManager.getEnabledModuleSet());

        mHomeModulesConfigManager.setPrefModuleTypeEnabled(0, false);
        assertFalse(mHomeModulesConfigManager.getPrefModuleTypeEnabled(0));
        assertTrue(mHomeModulesConfigManager.getEnabledModuleSet().isEmpty());
    }

    @Test
    public void testGetModuleListShownInSettings() {
        registerModuleConfigChecker(1);

        // Verifies that there isn't any module shown in the settings.
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(false);
        assertTrue(mHomeModulesConfigManager.getModuleListShownInSettings().isEmpty());

        // Verifies the list contains the module which eligible to build.
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(true);
        List<Integer> expectedList = List.of(0);
        assertEquals(expectedList, mHomeModulesConfigManager.getModuleListShownInSettings());
    }

    @Test
    public void testHasModuleShownInSettings() {
        registerModuleConfigChecker(1);

        // Verifies that there isn't any module shown in the settings.
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(false);
        assertFalse(mHomeModulesConfigManager.hasModuleShownInSettings());

        // Verifies the list contains the module which is eligible to build.
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(true);
        assertTrue(mHomeModulesConfigManager.hasModuleShownInSettings());
    }

    @Test
    public void testGetSettingsPreferenceKey() {
        String tabResumptionPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.TAB_RESUMPTION));
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.PRICE_CHANGE));

        assertFalse(TextUtils.equals(tabResumptionPreferenceKey, priceChangePreferenceKey));

        // Verifies that the SINGLE_TAB and TAB_RESUMPTION modules are shared with the same
        // preference key.
        assertEquals(
                tabResumptionPreferenceKey,
                mHomeModulesConfigManager.getSettingsPreferenceKey(ModuleType.SINGLE_TAB));
        assertEquals(
                tabResumptionPreferenceKey,
                mHomeModulesConfigManager.getSettingsPreferenceKey(ModuleType.TAB_RESUMPTION));

        // Verifies that the PRICE_CHANGE has its own preference key.
        assertEquals(
                priceChangePreferenceKey,
                mHomeModulesConfigManager.getSettingsPreferenceKey(ModuleType.PRICE_CHANGE));
    }

    @Test
    public void testFreshnessCount() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;
        String moduleFreshnessCountPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_FRESHNESS_COUNT.createKey(
                        String.valueOf(moduleType));
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();

        assertFalse(sharedPreferencesManager.contains(moduleFreshnessCountPreferenceKey));
        assertEquals(
                HomeModulesMediator.INVALID_FRESHNESS_SCORE,
                sharedPreferencesManager.readInt(
                        moduleFreshnessCountPreferenceKey,
                        HomeModulesMediator.INVALID_FRESHNESS_SCORE));

        int count = 5;
        mHomeModulesConfigManager.increaseFreshnessCount(moduleType, count);
        assertEquals(
                count,
                sharedPreferencesManager.readInt(
                        moduleFreshnessCountPreferenceKey,
                        HomeModulesMediator.INVALID_FRESHNESS_SCORE));

        mHomeModulesConfigManager.resetFreshnessCount(moduleType);
        assertTrue(sharedPreferencesManager.contains(moduleFreshnessCountPreferenceKey));
        assertEquals(
                HomeModulesMediator.INVALID_FRESHNESS_SCORE,
                sharedPreferencesManager.readInt(
                        moduleFreshnessCountPreferenceKey,
                        HomeModulesMediator.INVALID_FRESHNESS_SCORE));
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
