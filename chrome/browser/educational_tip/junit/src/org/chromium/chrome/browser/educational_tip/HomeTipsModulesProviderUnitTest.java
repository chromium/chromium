// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import static org.junit.Assert.assertArrayEquals;

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
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;

import java.util.Collection;

/** Test relating to {@link HomeTipsModulesProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeTipsModulesProviderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTime = new FakeTimeTestRule();

    private SharedPreferencesManager mSharedPreferencesManager;

    @Before
    public void setUp() {
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.ANDROID_SETUP_LIST)
    public void testGetModulesToRegister_returnsSetupListWhenActive() {
        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        mSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP, TimeUtils.currentTimeMillis());
        mFakeTime.advanceMillis(SetupListModuleUtils.SETUP_LIST_ACTIVE_WINDOW_MILLIS - 1);

        Collection<Integer> expectedModules = SetupListModuleUtils.getRankedModuleTypes();
        Collection<Integer> actualModules = HomeTipsModulesProvider.getModuleTypesToRegister();

        assertArrayEquals(expectedModules.toArray(), actualModules.toArray());
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.ANDROID_SETUP_LIST)
    public void testGetModulesToRegister_returnsEducationalTipsWhenInactive() {
        FirstRunStatus.setFirstRunTriggeredForTesting(false);
        mSharedPreferencesManager.removeKey(ChromePreferenceKeys.FIRST_CTA_START_TIMESTAMP);

        Collection<Integer> expectedModules = EducationalTipModuleUtils.getModuleTypes();
        Collection<Integer> actualModules = HomeTipsModulesProvider.getModuleTypesToRegister();

        assertArrayEquals(expectedModules.toArray(), actualModules.toArray());
    }
}
