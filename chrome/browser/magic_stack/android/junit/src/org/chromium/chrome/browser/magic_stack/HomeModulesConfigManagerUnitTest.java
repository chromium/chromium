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

import static org.chromium.chrome.browser.flags.ChromeFeatureList.HOME_MODULE_PREF_REFACTOR;
import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.getSettingsPreferenceKey;

import com.google.android.apps.common.testing.accessibility.framework.replacements.TextUtils;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager.HomeModulesStateListener;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link HomeModulesConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HomeModulesConfigManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private HomeModulesStateListener mListener;

    private HomeModulesConfigManager mHomeModulesConfigManager;
    private ModuleRegistry mModuleRegistry;

    @Before
    public void setUp() {
        mHomeModulesConfigManager = new HomeModulesConfigManager();
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        mModuleRegistry =
                new ModuleRegistry(
                        mHomeModulesConfigManager, mock(ActivityLifecycleDispatcher.class));
        mHomeModulesConfigManager.addListener(mListener);
    }

    @After
    public void tearDown() {
        mModuleRegistry.destroy();
    }

    @Test
    public void testSetAndGetPrefModuleTypeEnabled() {
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(
                        String.valueOf(ModuleType.PRICE_CHANGE));

        ChromeSharedPreferences.getInstance().writeBoolean(priceChangePreferenceKey, true);
        assertTrue(mHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE));

        ChromeSharedPreferences.getInstance().writeBoolean(priceChangePreferenceKey, false);
        Assert.assertFalse(
                mHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE));

        mHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, true);
        assertTrue(
                ChromeSharedPreferences.getInstance().readBoolean(priceChangePreferenceKey, true));
        verify(mListener).onModuleConfigChanged(eq(ModuleType.PRICE_CHANGE), eq(true));

        mHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, false);
        Assert.assertFalse(
                ChromeSharedPreferences.getInstance().readBoolean(priceChangePreferenceKey, true));
        verify(mListener).onModuleConfigChanged(eq(ModuleType.PRICE_CHANGE), eq(false));
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
}
