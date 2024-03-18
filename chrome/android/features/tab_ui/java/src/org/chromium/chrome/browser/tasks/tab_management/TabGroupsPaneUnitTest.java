// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

import java.util.function.DoubleConsumer;

/** Unit tests for {@link TabGroupsPane}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupsPaneUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private DoubleConsumer mOnToolbarAlphaChange;

    @Test
    public void testNotifyLoadHint() {
        TabGroupsPane pane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange);
        assertEquals(0, pane.getRootView().getChildCount());

        pane.notifyLoadHint(LoadHint.HOT);
        assertNotEquals(0, pane.getRootView().getChildCount());

        pane.notifyLoadHint(LoadHint.COLD);
        assertEquals(0, pane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileHot() {
        TabGroupsPane pane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange);
        pane.notifyLoadHint(LoadHint.HOT);
        pane.destroy();
        assertEquals(0, pane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_WhileCold() {
        TabGroupsPane pane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange);
        pane.notifyLoadHint(LoadHint.HOT);
        pane.notifyLoadHint(LoadHint.COLD);
        pane.destroy();
        assertEquals(0, pane.getRootView().getChildCount());
    }

    @Test
    public void testDestroy_NoLoadHint() {
        TabGroupsPane pane =
                new TabGroupsPane(
                        ApplicationProvider.getApplicationContext(),
                        LazyOneshotSupplier.fromValue(mTabGroupModelFilter),
                        mOnToolbarAlphaChange);
        pane.destroy();
        assertEquals(0, pane.getRootView().getChildCount());
    }
}
