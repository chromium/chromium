// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.INVALID_FRESHNESS_SCORE;
import static org.chromium.chrome.browser.magic_stack.HomeModulesUtils.INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION;

import android.os.SystemClock;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
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
    public void testIncreaseFreshnessCount() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;
        verifyFreshnessKeysDoNotExistInSharedPreference(moduleType);

        int count = 5;
        HomeModulesUtils.increaseFreshnessCount(moduleType, count);
        verifyCountAndTimestamp(moduleType, count);

        count = 1;
        HomeModulesUtils.increaseFreshnessCount(moduleType, count);
        verifyCountAndTimestamp(moduleType, /* expectedCount= */ 6);
    }

    @Test
    @SmallTest
    public void testResetFreshnessCountAsFresh() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;
        verifyFreshnessKeysDoNotExistInSharedPreference(moduleType);

        HomeModulesUtils.resetFreshnessCountAsFresh(moduleType);
        verifyCountAndTimestamp(moduleType, /* expectedCount= */ 0);

        int count = 1;
        HomeModulesUtils.increaseFreshnessCount(moduleType, count);
        verifyCountAndTimestamp(moduleType, count);

        HomeModulesUtils.resetFreshnessCountAsFresh(moduleType);
        verifyCountAndTimestamp(moduleType, /* expectedCount= */ 0);
    }

    @Test
    @SmallTest
    public void testFreshnessScoreTimeStamp() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        verifyFreshnessKeysDoNotExistInSharedPreference(moduleType);

        long timeStamp = SystemClock.elapsedRealtime() - 10;
        HomeModulesUtils.setFreshnessScoreTimeStamp(moduleType, timeStamp);
        assertEquals(timeStamp, HomeModulesUtils.getFreshnessScoreTimeStamp(moduleType));
    }

    @Test
    @SmallTest
    public void testGetFreshnessScore() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        verifyFreshnessKeysDoNotExistInSharedPreference(moduleType);

        // Verifies INVALID_FRESHNESS_SCORE is returned when the timestamp isn't saved.
        assertEquals(
                INVALID_FRESHNESS_SCORE,
                HomeModulesUtils.getFreshnessScore(/* useFreshnessScore= */ true, moduleType));

        // Verifies that if the logged time is longer than the threshold, the freshness score is
        // invalid.
        int expectedScore = 100;
        long scoreLoggedTime =
                SystemClock.elapsedRealtime() - HomeModulesMediator.FRESHNESS_THRESHOLD_MS - 10;
        HomeModulesUtils.setFreshnessCountForTesting(moduleType, expectedScore, scoreLoggedTime);
        assertEquals(
                INVALID_FRESHNESS_SCORE,
                HomeModulesUtils.getFreshnessScore(/* useFreshnessScore= */ true, moduleType));

        // Verifies that the freshness score will be used if the logging time is less than the
        // threshold.
        expectedScore = 100;
        scoreLoggedTime = SystemClock.elapsedRealtime() - 10;
        HomeModulesUtils.setFreshnessCountForTesting(moduleType, expectedScore, scoreLoggedTime);
        assertEquals(
                expectedScore,
                HomeModulesUtils.getFreshnessScore(/* useFreshnessScore= */ true, moduleType));

        // Verifies INVALID_FRESHNESS_SCORE is returned when useFreshnessScore is false.
        assertEquals(
                INVALID_FRESHNESS_SCORE,
                HomeModulesUtils.getFreshnessScore(/* useFreshnessScore= */ false, moduleType));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER,
        ChromeFeatureList.SEGMENTATION_PLATFORM_ANDROID_HOME_MODULE_RANKER_V2
    })
    public void testCreateInputContext() {
        @ModuleType int moduleType = ModuleType.PRICE_CHANGE;

        int expectedScore = 100;
        long scoreLoggedTime = SystemClock.elapsedRealtime() - 10;
        HomeModulesUtils.setFreshnessCountForTesting(moduleType, expectedScore, scoreLoggedTime);

        InputContext inputContext = HomeModulesUtils.createInputContext(moduleType);
        assertEquals(1, inputContext.getSizeForTesting());
        // Verifies the freshness score matches.
        assertEquals(
                expectedScore,
                inputContext.getEntryValue(
                                HomeModulesUtils.getFreshnessInputContextString(moduleType))
                        .floatValue,
                0.01);
    }

    @Test
    @SmallTest
    public void testIncreaseAndGetImpressionCountBeforeInteraction() {
        @ModuleType int moduleType = ModuleType.TAB_GROUP_PROMO;
        verifyImpressionCountBeforeInteractionKeysDoNotExistInSharedPreference(moduleType);

        HomeModulesUtils.increaseImpressionCountBeforeInteraction(moduleType);
        assertEquals(1, HomeModulesUtils.getImpressionCountBeforeInteraction(moduleType));

        HomeModulesUtils.increaseImpressionCountBeforeInteraction(moduleType);
        assertEquals(2, HomeModulesUtils.getImpressionCountBeforeInteraction(moduleType));
    }

    private void verifyImpressionCountBeforeInteractionKeysDoNotExistInSharedPreference(
            @ModuleType int moduleType) {
        String moduleImpressionCountBeforeInteractionPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_IMPRESSION_COUNT_BEFORE_INTERACTION.createKey(
                        String.valueOf(moduleType));
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();

        // Verifies that the impression count before interaction key doesn't exist.
        assertFalse(
                sharedPreferencesManager.contains(
                        moduleImpressionCountBeforeInteractionPreferenceKey));
        assertEquals(
                INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION,
                sharedPreferencesManager.readInt(
                        moduleImpressionCountBeforeInteractionPreferenceKey,
                        INVALID_IMPRESSION_COUNT_BEFORE_INTERACTION));
    }

    private void verifyFreshnessKeysDoNotExistInSharedPreference(@ModuleType int moduleType) {
        String moduleFreshnessCountPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_FRESHNESS_COUNT.createKey(
                        String.valueOf(moduleType));
        String moduleFreshnessTimestampPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_FRESHNESS_TIMESTAMP_MS.createKey(
                        String.valueOf(moduleType));
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();

        // Verifies that both freshness count key and the timestamp key don't exist.
        assertFalse(sharedPreferencesManager.contains(moduleFreshnessCountPreferenceKey));
        assertEquals(
                INVALID_FRESHNESS_SCORE,
                sharedPreferencesManager.readInt(
                        moduleFreshnessCountPreferenceKey, INVALID_FRESHNESS_SCORE));
        assertFalse(sharedPreferencesManager.contains(moduleFreshnessTimestampPreferenceKey));
    }

    private void verifyCountAndTimestamp(@ModuleType int moduleType, int expectedCount) {
        String moduleFreshnessCountPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_FRESHNESS_COUNT.createKey(
                        String.valueOf(moduleType));
        String moduleFreshnessTimestampPreferenceKey =
                ChromePreferenceKeys.HOME_MODULES_FRESHNESS_TIMESTAMP_MS.createKey(
                        String.valueOf(moduleType));
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();

        // Verifies the freshness count matches.
        assertTrue(sharedPreferencesManager.contains(moduleFreshnessCountPreferenceKey));
        assertEquals(expectedCount, HomeModulesUtils.getFreshnessCount(moduleType));

        // Verifies that the timestamp is set as the current time.
        assertTrue(sharedPreferencesManager.contains(moduleFreshnessTimestampPreferenceKey));
        assertEquals(
                TimeUtils.uptimeMillis(), HomeModulesUtils.getFreshnessScoreTimeStamp(moduleType));
    }
}
