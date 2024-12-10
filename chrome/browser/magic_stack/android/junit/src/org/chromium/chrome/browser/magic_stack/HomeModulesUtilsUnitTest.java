// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.magic_stack.HomeModulesMediator.INVALID_FRESHNESS_SCORE;

import android.os.SystemClock;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.segmentation_platform.InputContext;

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

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2
    })
    public void testCreateInputContext_InvalidScore() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        HomeModulesUtils.setFreshnessCountForTesting(
                moduleType, HomeModulesUtils.INVALID_FRESHNESS_SCORE);

        InputContext inputContext = HomeModulesUtils.createInputContextForTesting(moduleType);

        assertEquals(1, inputContext.getSizeForTesting());
        assertEquals(
                INVALID_FRESHNESS_SCORE,
                inputContext.getEntryForTesting(
                                HomeModulesUtils.getFreshnessInputContextString(moduleType))
                        .floatValue,
                0.01);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2
    })
    public void testCreateInputContext_InvalidTimestamp() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        // Verifies that if the logged time is longer than the threshold, the freshness score is
        // invalid.
        int expectedScore = 100;
        long scoreLoggedTime =
                SystemClock.elapsedRealtime() - HomeModulesMediator.FRESHNESS_THRESHOLD_MS - 10;
        HomeModulesUtils.setFreshnessScoreTimeStamp(moduleType, scoreLoggedTime);
        HomeModulesUtils.setFreshnessCountForTesting(moduleType, expectedScore);

        InputContext inputContext = HomeModulesUtils.createInputContextForTesting(moduleType);

        assertEquals(1, inputContext.getSizeForTesting());
        assertEquals(
                INVALID_FRESHNESS_SCORE,
                inputContext.getEntryForTesting(
                                HomeModulesUtils.getFreshnessInputContextString(moduleType))
                        .floatValue,
                0.01);
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2
    })
    public void testCreateInputContext() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        // Verifies that the freshness score will be used if the logging time is less than the
        // threshold.
        int expectedScore = 100;
        long scoreLoggedTime = SystemClock.elapsedRealtime() - 10;
        HomeModulesUtils.setFreshnessScoreTimeStamp(moduleType, scoreLoggedTime);
        HomeModulesUtils.setFreshnessCountForTesting(moduleType, expectedScore);

        InputContext inputContext = HomeModulesUtils.createInputContextForTesting(moduleType);

        assertEquals(1, inputContext.getSizeForTesting());
        assertEquals(
                expectedScore,
                inputContext.getEntryForTesting(
                                HomeModulesUtils.getFreshnessInputContextString(moduleType))
                        .floatValue,
                0.01);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER})
    @DisableFeatures({ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2})
    public void testCreateInputContext_Disabled() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        // Verifies that the freshness score won't be used if the flag
        // ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2 is disabled.
        int expectedScore = 100;
        long scoreLoggedTime = SystemClock.elapsedRealtime() - 10;
        HomeModulesUtils.setFreshnessScoreTimeStamp(moduleType, scoreLoggedTime);
        HomeModulesUtils.setFreshnessCountForTesting(moduleType, expectedScore);

        InputContext inputContext = HomeModulesUtils.createInputContextForTesting(moduleType);

        assertEquals(1, inputContext.getSizeForTesting());
        assertEquals(
                INVALID_FRESHNESS_SCORE,
                inputContext.getEntryForTesting(
                                HomeModulesUtils.getFreshnessInputContextString(moduleType))
                        .floatValue,
                0.01);
    }
}
