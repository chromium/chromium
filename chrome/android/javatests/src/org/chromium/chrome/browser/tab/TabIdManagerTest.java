// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Context;
import android.content.SharedPreferences;
import android.support.test.InstrumentationRegistry;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Tests for the TabIdManager. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabIdManagerTest {
    Context mContext;

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    @Before
    public void setUp() {
        mContext = new AdvancedMockContext(InstrumentationRegistry.getTargetContext());
    }

    /** Tests that IDs are stored and generated properly. */
    @Test
    @UiThreadTest
    @SmallTest
    public void testBasic() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(TabIdManager.PREF_NEXT_ID, 11684);
        editor.apply();

        TabIdManager manager = TabIdManager.getInstance(mContext);
        Assert.assertEquals(
                "Wrong Tab ID was generated", 11684, manager.generateValidId(Tab.INVALID_TAB_ID));

        Assert.assertEquals(
                "Wrong next Tab ID", 11685, prefs.getInt(TabIdManager.PREF_NEXT_ID, -1));
    }

    /** Tests that the max ID is updated properly. */
    @Test
    @UiThreadTest
    @SmallTest
    public void testIncrementIdCounterTo() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(TabIdManager.PREF_NEXT_ID, 11684);
        editor.apply();

        TabIdManager manager = TabIdManager.getInstance(mContext);
        Assert.assertEquals(
                "Wrong Tab ID was generated", 11684, manager.generateValidId(Tab.INVALID_TAB_ID));

        Assert.assertEquals(
                "Wrong next Tab ID", 11685, prefs.getInt(TabIdManager.PREF_NEXT_ID, -1));

        manager.incrementIdCounterTo(100);
        Assert.assertEquals(
                "Didn't stay the same", 11685, prefs.getInt(TabIdManager.PREF_NEXT_ID, -1));

        manager.incrementIdCounterTo(1000000);
        Assert.assertEquals(
                "Didn't increase", 1000000, prefs.getInt(TabIdManager.PREF_NEXT_ID, -1));
    }
}
