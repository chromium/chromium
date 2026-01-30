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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Test relating to {@link SetupListManager} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
@Features.EnableFeatures(ChromeFeatureList.ANDROID_SETUP_LIST)
public class SetupListManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private SharedPreferencesManager mSharedPreferencesManager;
    private static final long ONE_MINUTE_IN_MILLIS = TimeUnit.MINUTES.toMillis(1);

    @Before
    public void setUp() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP);
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_SETUP_LIST)
    public void testSetupList_ReturnFalseWhenFeatureDisabled() {
        // Re-create instance after feature flag is disabled.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
        assertFalse(
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP));
    }

    @Test
    @SmallTest
    public void testSetupList_ReturnFalseDuringFirstRun() {
        FirstRunStatus.setFirstRunTriggeredForTesting(true);
        // Re-create instance after FirstRunStatus is set.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
        assertFalse(
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP));
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_TrueAndSetsTimestampWhenNotSet() {
        // Ensure the timestamp is not set initially.
        assertFalse(
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP));

        // Re-create instance.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
        // Check that the timestamp is now set.
        assertTrue(
                mSharedPreferencesManager.contains(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP));
        assertEquals(
                TimeUtils.currentTimeMillis(),
                mSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP, -1L));
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsTrueWithinActiveWindow() {
        // Set the timestamp to be within the active window.
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.SETUP_LIST_ACTIVE_WINDOW_MILLIS - ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
    }

    @Test
    @SmallTest
    public void testTwoCellLayout_InActiveWithinThreeDays() {
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS - ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
    }

    @Test
    @SmallTest
    public void testTwoCellLayout_ActiveAfterThreeDays() {
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.TWO_CELL_LAYOUT_ACTIVE_WINDOW_MILLIS + ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
        assertTrue(SetupListManager.getInstance().shouldShowTwoCellLayout());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsFalseOutsideActiveWindow() {
        // Set the timestamp to be outside the active window.
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.SETUP_LIST_FIRST_SHOWN_TIMESTAMP,
                TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.SETUP_LIST_ACTIVE_WINDOW_MILLIS + ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
        assertFalse(SetupListManager.getInstance().shouldShowTwoCellLayout());
    }

    @Test
    @SmallTest
    public void testGetRankedModuleTypes_ReordersOnCompletion() {
        SetupListManager.setInstanceForTesting(new SetupListManager());
        List<Integer> rankedModules = SetupListManager.getInstance().getRankedModuleTypes();

        // Initially, pick the first item.
        int firstModuleType = rankedModules.get(0);
        String prefKey = SetupListModuleUtils.getCompletionKeyForModule(firstModuleType);

        // Mark the first item as completed.
        mSharedPreferencesManager.writeBoolean(prefKey, true);

        // Notify manager of the change.
        SetupListManager.getInstance().onSharedPreferenceChanged(null, prefKey);

        rankedModules = SetupListManager.getInstance().getRankedModuleTypes();

        // The first item should now be at the end of the list.
        assertEquals(firstModuleType, (int) rankedModules.get(rankedModules.size() - 1));
    }

    @Test
    @SmallTest
    public void testGetManualRank_UpdatesDynamically() {
        SetupListManager.setInstanceForTesting(new SetupListManager());
        List<Integer> rankedModules = SetupListManager.getInstance().getRankedModuleTypes();

        // Initially, pick the first item.
        int firstModuleType = rankedModules.get(0);
        String prefKey = SetupListModuleUtils.getCompletionKeyForModule(firstModuleType);

        // Its rank should be 0.
        assertEquals(0, (int) SetupListManager.getInstance().getManualRank(firstModuleType));

        // Complete the first item.
        mSharedPreferencesManager.writeBoolean(prefKey, true);
        SetupListManager.getInstance().onSharedPreferenceChanged(null, prefKey);

        // Now its rank should be at the end.
        int expectedRank = SetupListManager.getInstance().getRankedModuleTypes().size() - 1;
        assertEquals(
                expectedRank, (int) SetupListManager.getInstance().getManualRank(firstModuleType));
    }
}
