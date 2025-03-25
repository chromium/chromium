// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.when;

import android.graphics.Point;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabContextMenuData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabContextMenuDataUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;

    private UserDataHost mUserDataHost;

    @Before
    public void setUp() {
        mUserDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
    }

    @After
    public void tearDown() {
        mUserDataHost.destroy();
    }

    @Test
    public void testUserData() {
        assertNull(TabContextMenuData.getForTab(mTab));

        TabContextMenuData data = TabContextMenuData.getOrCreateForTab(mTab);
        assertNotNull(data);
        assertEquals(data, TabContextMenuData.getForTab(mTab));

        assertNull(data.getLastTriggeringTouchPositionDp());

        int x = 9;
        int y = 8;
        data.setLastTriggeringTouchPositionDp(x, y);
        assertEquals(new Point(x, y), data.getLastTriggeringTouchPositionDp());

        data.setLastTriggeringTouchPositionDp(null);
        assertNull(data.getLastTriggeringTouchPositionDp());
    }

    @Test(expected = AssertionError.class)
    public void testUserData_DestroyedTab() {
        when(mTab.isDestroyed()).thenReturn(true);
        assertNull(TabContextMenuData.getForTab(mTab));
        assertNull(TabContextMenuData.getOrCreateForTab(mTab));
    }
}
