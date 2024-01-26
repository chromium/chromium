// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.magic_stack;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link ChromeHomeModulesConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeHomeModulesConfigManagerUnitTest {
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testSetAndGetPrefModuleTypeDisabled() {
        ChromeHomeModulesConfigManager chromeHomeModulesConfigManager =
                ChromeHomeModulesConfigManager.getInstance();
        String priceChangePreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_MODULE_TYPE.createKey(String.valueOf(1));

        ChromeSharedPreferences.getInstance().writeBoolean(priceChangePreferenceKey, true);
        Assert.assertTrue(
                chromeHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE));

        ChromeSharedPreferences.getInstance().writeBoolean(priceChangePreferenceKey, false);
        Assert.assertFalse(
                chromeHomeModulesConfigManager.getPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE));

        chromeHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, true);
        Assert.assertTrue(
                ChromeSharedPreferences.getInstance().readBoolean(priceChangePreferenceKey, true));

        chromeHomeModulesConfigManager.setPrefModuleTypeEnabled(ModuleType.PRICE_CHANGE, false);
        Assert.assertFalse(
                ChromeSharedPreferences.getInstance().readBoolean(priceChangePreferenceKey, true));
    }
}
