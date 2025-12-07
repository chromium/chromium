// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchConfigManager.ShareTabsWithOsStateListener;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link AuxiliarySearchConfigManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchConfigManagerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ShareTabsWithOsStateListener mListener;

    private AuxiliarySearchConfigManager mConfigManager;

    @Before
    public void setUp() {
        mConfigManager = AuxiliarySearchConfigManager.getInstance();
    }

    @Test
    @SmallTest
    public void testAddAndRemoveListener() {
        assertEquals(0, mConfigManager.getObserverListSizeForTesting());

        mConfigManager.addListener(mListener);
        assertEquals(1, mConfigManager.getObserverListSizeForTesting());

        mConfigManager.notifyShareTabsStateChanged(true);
        SharedPreferencesManager prefManager = ChromeSharedPreferences.getInstance();
        assertTrue(
                prefManager.readBoolean(
                        ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED, false));

        verify(mListener).onConfigChanged(eq(true));

        mConfigManager.removeListener(mListener);
        assertEquals(0, mConfigManager.getObserverListSizeForTesting());

        prefManager.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED);
    }
}
