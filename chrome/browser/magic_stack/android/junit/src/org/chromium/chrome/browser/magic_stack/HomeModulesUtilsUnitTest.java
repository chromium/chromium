// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link HomeModulesUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeModulesUtilsUnitTest {
    @Test
    @SmallTest
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
        HomeModulesUtils.increaseFreshnessCount(moduleType, count);
        assertEquals(
                count,
                sharedPreferencesManager.readInt(
                        moduleFreshnessCountPreferenceKey,
                        HomeModulesMediator.INVALID_FRESHNESS_SCORE));

        HomeModulesUtils.resetFreshnessCount(moduleType);
        assertTrue(sharedPreferencesManager.contains(moduleFreshnessCountPreferenceKey));
        assertEquals(
                HomeModulesMediator.INVALID_FRESHNESS_SCORE,
                sharedPreferencesManager.readInt(
                        moduleFreshnessCountPreferenceKey,
                        HomeModulesMediator.INVALID_FRESHNESS_SCORE));
    }
}
