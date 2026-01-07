// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.setup_list;

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
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_SETUP_LIST)
    public void testIsSetupListActive_ReturnsFalseWhenFeatureDisabled() {
        // Re-create instance after feature flag is disabled.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsFalseDuringFirstRun() {
        FirstRunStatus.setFirstRunTriggeredForTesting(true);
        // Re-create instance after FirstRunStatus is set.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, TimeUtils.currentTimeMillis());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsFalseWhenNoTimestamp() {
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP);
        // Re-create instance after shared pref is removed.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsTrueWithinActiveWindow() {
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.SETUP_LIST_ACTIVE_WINDOW_MILLIS - ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertTrue(SetupListManager.getInstance().isSetupListActive());
    }

    @Test
    @SmallTest
    public void testIsSetupListActive_ReturnsFalseOutsideActiveWindow() {
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(
                SetupListManager.SETUP_LIST_ACTIVE_WINDOW_MILLIS + ONE_MINUTE_IN_MILLIS);
        // Re-create instance after time is advanced.
        SetupListManager.setInstanceForTesting(new SetupListManager());
        assertFalse(SetupListManager.getInstance().isSetupListActive());
    }
}
