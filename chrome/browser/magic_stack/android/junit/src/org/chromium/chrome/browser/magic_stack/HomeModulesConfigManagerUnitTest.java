// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.HOME_MODULE_PREF_REFACTOR;
import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getEducationalTipModuleList;
import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getSettingsPreferenceKey;

import android.text.TextUtils;
import android.view.ViewGroup;

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

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager.HomeModulesStateListener;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Set;

/** Unit tests for {@link HomeModulesConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeModulesConfigManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private HomeModulesStateListener mListener;

    private final List<ModuleConfigChecker> mModuleConfigCheckerList = new ArrayList<>();
    private HomeModulesConfigManager mHomeModulesConfigManager;
    private ModuleRegistry mModuleRegistry;

    static class TestModuleProviderBuilder implements ModuleProviderBuilder, ModuleConfigChecker {
        public TestModuleProviderBuilder() {}

        @Override
        public boolean isEligible() {
            return false;
        }

        @Override
        public boolean build(
                ModuleDelegate moduleDelegate, Callback<ModuleProvider> onModuleBuiltCallback) {
            return false;
        }

        @Override
        public ViewGroup createView(ViewGroup parentView) {
            return null;
        }

        @Override
        public void bind(PropertyModel model, ViewGroup view, PropertyKey propertyKey) {}
    }

    @Before
    public void setUp() {
        mHomeModulesConfigManager = HomeModulesConfigManager.getInstance();
        mModuleRegistry =
                new ModuleRegistry(
                        mHomeModulesConfigManager, mock(ActivityLifecycleDispatcher.class));
        mHomeModulesConfigManager.addListener(mListener);
    }

    @After
    public void tearDown() {
        mHomeModulesConfigManager.cleanupForTesting();
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
    @DisableFeatures(HOME_MODULE_PREF_REFACTOR)
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
    @EnableFeatures(HOME_MODULE_PREF_REFACTOR)
    public void testGetEnabledModuleSet_allCardsOff_restoresOnAndOffTypes() {
        registerModuleConfigCheckerWithEligibility(ModuleType.SINGLE_TAB, true);
        registerModuleConfigCheckerWithEligibility(ModuleType.PRICE_CHANGE, false);

        mHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.SINGLE_TAB, true);
        mHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, false);

        Set<Integer> enabledModulesBeforeToggleOff = Set.of(ModuleType.SINGLE_TAB);
        Assert.assertEquals(
                enabledModulesBeforeToggleOff, mHomeModulesConfigManager.getEnabledModuleSet());

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, false);

        Set<Integer> enabledModulesAfterToggleOff = mHomeModulesConfigManager.getEnabledModuleSet();
        Assert.assertTrue(enabledModulesAfterToggleOff.isEmpty());

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, true);
        Set<Integer> enabledModulesAfterToggleOn = mHomeModulesConfigManager.getEnabledModuleSet();

        Assert.assertEquals(enabledModulesBeforeToggleOff, enabledModulesAfterToggleOn);
    }

    @Test
    @EnableFeatures(HOME_MODULE_PREF_REFACTOR)
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
    public void testGetModuleListShownInSettings_featureDisabled() {
        registerModuleConfigChecker(1);

        // Verifies that there isn't any module shown in the settings.
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(false);
        assertTrue(mHomeModulesConfigManager.getModuleListShownInSettings().isEmpty());

        // Verifies the list contains the module which eligible to build.
        when(mModuleConfigCheckerList.get(0).isEligible()).thenReturn(true);
        assertEquals(List.of(0), mHomeModulesConfigManager.getModuleListShownInSettings());
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
    public void testGetEducationalTipListItemShown() {
        for (@ModuleType int tipModule : getEducationalTipModuleList()) {
            registerModuleConfigCheckerWithEligibility(tipModule, true);
        }

        // Verifies that the return list of getModuleListShownInSettings() contains only one of
        // educational tip modules.
        List<Integer> moduleList = mHomeModulesConfigManager.getModuleListShownInSettings();
        assertEquals(1, moduleList.size());
        assertTrue(HomeModulesUtils.belongsToEducationalTipModule(moduleList.get(0)));
    }

    @Test
    public void testGetMultipleListItemShown() {
        List<Integer> moduleTypeList =
                Arrays.asList(
                        ModuleType.SINGLE_TAB,
                        ModuleType.SAFETY_HUB,
                        ModuleType.QUICK_DELETE_PROMO,
                        ModuleType.PRICE_CHANGE);

        for (Integer moduleType : moduleTypeList) {
            registerModuleConfigCheckerWithEligibility(moduleType, true);
        }

        List<Integer> moduleList = mHomeModulesConfigManager.getModuleListShownInSettings();

        for (Integer moduleType : moduleTypeList) {
            assertTrue(moduleList.contains(moduleType));
        }
    }

    @Test
    public void testGetMultipleListItemShownSomeComplex() {
        List<Integer> eligibleTypeList =
                Arrays.asList(ModuleType.SAFETY_HUB, ModuleType.AUXILIARY_SEARCH);
        List<Integer> notEligibleTypeList =
                Arrays.asList(ModuleType.SINGLE_TAB, ModuleType.PRICE_CHANGE);

        for (Integer moduleType : eligibleTypeList) {
            registerModuleConfigCheckerWithEligibility(moduleType, true);
        }

        for (Integer moduleType : notEligibleTypeList) {
            registerModuleConfigCheckerWithEligibility(moduleType, false);
        }

        List<Integer> moduleList = mHomeModulesConfigManager.getModuleListShownInSettings();

        // Verifies the return list of getModuleListShownInSettings() only contains eligible module
        // types.
        for (Integer moduleType : eligibleTypeList) {
            assertTrue(moduleList.contains(moduleType));
        }
    }

    @Test
    public void testGetSettingsPreferenceKey() {
        String singleTabPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.SINGLE_TAB));
        String defaultBrowserPromoPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.DEFAULT_BROWSER_PROMO));
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.PRICE_CHANGE));

        assertFalse(TextUtils.equals(singleTabPreferenceKey, priceChangePreferenceKey));
        assertFalse(TextUtils.equals(defaultBrowserPromoPreferenceKey, priceChangePreferenceKey));

        assertEquals(singleTabPreferenceKey, getSettingsPreferenceKey(ModuleType.SINGLE_TAB));

        // Verifies that all the educational tip modules are shared with the same preference key.
        assertEquals(
                defaultBrowserPromoPreferenceKey,
                getSettingsPreferenceKey(ModuleType.DEFAULT_BROWSER_PROMO));
        assertEquals(
                defaultBrowserPromoPreferenceKey,
                getSettingsPreferenceKey(ModuleType.TAB_GROUP_PROMO));
        assertEquals(
                defaultBrowserPromoPreferenceKey,
                getSettingsPreferenceKey(ModuleType.TAB_GROUP_SYNC_PROMO));
        assertEquals(
                defaultBrowserPromoPreferenceKey,
                getSettingsPreferenceKey(ModuleType.QUICK_DELETE_PROMO));

        // Verifies that the PRICE_CHANGE has its own preference key.
        assertEquals(priceChangePreferenceKey, getSettingsPreferenceKey(ModuleType.PRICE_CHANGE));
    }

    @Test
    @EnableFeatures(HOME_MODULE_PREF_REFACTOR)
    public void testSetPrefAllCardsEnabled() {
        mHomeModulesConfigManager.setPrefAllCardsEnabled(false);
        assertFalse(
                "Expected HOME_MODULE_CARDS_ENABLED preference to be false",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, true));
        verify(mListener).allCardsConfigChanged(eq(false));

        mHomeModulesConfigManager.setPrefAllCardsEnabled(true);
        assertTrue(
                "Expected HOME_MODULE_CARDS_ENABLED preference to be true",
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.HOME_MODULE_CARDS_ENABLED, false));
        verify(mListener).allCardsConfigChanged(eq(true));
    }

    private void registerModuleConfigChecker(int size) {
        size = Math.min(size, ModuleType.NUM_ENTRIES);
        for (int i = 0; i < size; i++) {
            ModuleConfigChecker moduleConfigChecker = Mockito.mock(ModuleConfigChecker.class);
            mModuleConfigCheckerList.add(moduleConfigChecker);
            mHomeModulesConfigManager.registerModuleEligibilityChecker(i, moduleConfigChecker);
        }
    }

    private void registerModuleConfigCheckerWithEligibility(
            @ModuleType int moduleType, boolean eligibility) {
        TestModuleProviderBuilder builder = mock(TestModuleProviderBuilder.class);
        when(builder.isEligible()).thenReturn(eligibility);
        mModuleRegistry.registerModule(moduleType, builder);
    }
}
