// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Tests for the TabIdManager. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabIdManagerTest {
    private SharedPreferencesManager mPrefs;

    @Before
    public void setUp() {
        mPrefs = ChromeSharedPreferences.getInstance();
        mPrefs.writeInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, 11684);
        TabIdManager.resetInstanceForTesting();
    }

    /** Tests that IDs are stored and generated properly. */
    @Test
    public void testBasic() {
        TabIdManager manager = TabIdManager.getInstance();
        Assert.assertEquals(
                "Wrong Tab ID was generated", 11684, manager.generateValidId(Tab.INVALID_TAB_ID));
        Assert.assertEquals(
                "Wrong next Tab ID",
                11685,
                mPrefs.readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, -1));

        Assert.assertEquals(
                "Passing valid ID < mIdCounter should return that ID",
                100,
                manager.generateValidId(100));
        Assert.assertEquals(
                "Next generated ID should remain unchanged after valid ID < mIdCounter",
                11685,
                manager.generateValidId(Tab.INVALID_TAB_ID));

        Assert.assertEquals(
                "Passing valid ID >= mIdCounter should return that ID",
                20000,
                manager.generateValidId(20000));
        Assert.assertEquals(
                "Next generated ID should advance after valid ID >= mIdCounter",
                20001,
                manager.generateValidId(Tab.INVALID_TAB_ID));
        Assert.assertEquals(
                "Wrong next Tab ID",
                20002,
                mPrefs.readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, -1));
    }

    /** Tests that the max ID is updated properly. */
    @Test
    public void testIncrementIdCounterTo() {
        TabIdManager manager = TabIdManager.getInstance();
        mPrefs.removeKey(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID);

        manager.incrementIdCounterTo(100);
        manager.incrementIdCounterTo(11684);
        Assert.assertFalse(
                "SharedPreferences should not be written when id <= mIdCounter",
                mPrefs.contains(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID));

        manager.incrementIdCounterTo(11685);
        Assert.assertEquals(
                "Didn't increase when id == mIdCounter + 1",
                11685,
                mPrefs.readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, -1));

        manager.incrementIdCounterTo(1000000);
        Assert.assertEquals(
                "Didn't increase",
                1000000,
                mPrefs.readInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, -1));
        Assert.assertEquals(
                "Wrong Tab ID was generated", 1000000, manager.generateValidId(Tab.INVALID_TAB_ID));
    }
}
