// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Test relating to {@link SetupListModuleUtils} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class SetupListModuleUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private SharedPreferencesManager mSharedPreferencesManager;
    private static final long ONE_MINUTE_IN_MILLIS = TimeUnit.MINUTES.toMillis(1);

    @Before
    public void setUp() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsFalseDuringFirstRun() {
        FirstRunStatus.setFirstRunTriggeredForTesting(true);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, TimeUtils.currentTimeMillis());
        assertFalse(SetupListModuleUtils.isSetupListActive());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsFalseWhenNoTimestamp() {
        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP);
        assertFalse(SetupListModuleUtils.isSetupListActive());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsTrueWithinActiveWindow() {
        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListModuleUtils.SETUP_LIST_ACTIVE_WINDOW_MILLIS - ONE_MINUTE_IN_MILLIS);
        assertTrue(SetupListModuleUtils.isSetupListActive());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsFalseOutsideActiveWindow() {
        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListModuleUtils.SETUP_LIST_ACTIVE_WINDOW_MILLIS + ONE_MINUTE_IN_MILLIS);
        assertFalse(SetupListModuleUtils.isSetupListActive());
    }

    @Test
    @SmallTest
    public void testGetRankedModuleTypes_ReturnsCorrectOrder() {
        List<Integer> rankedModules = SetupListModuleUtils.getRankedModuleTypes();
        assertEquals(ModuleType.ENHANCED_SAFE_BROWSING_PROMO, (int) rankedModules.get(0));
        assertEquals(ModuleType.ADDRESS_BAR_PLACEMENT_PROMO, (int) rankedModules.get(1));
    }
}
