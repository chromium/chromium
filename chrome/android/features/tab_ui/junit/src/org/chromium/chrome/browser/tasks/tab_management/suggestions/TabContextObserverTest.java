// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for TabContextObserver
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabContextObserverTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private TabModelSelector mTabModelSelector;

    @Mock
    private TabModelFilterProvider mTabModelFitlerProvider;

    @Mock
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    static class TabContextObserverTestHelper extends TabContextObserver {
        private int mChangeReason;

        public TabContextObserverTestHelper(TabModelSelector selector) {
            super(selector);
        }

        @Override
        public void onTabContextChanged(@TabContextChangeReason int changeReason) {
            mChangeReason = changeReason;
        }

        public int getChangeReason() {
            return mChangeReason;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mTabModelFitlerProvider).when(mTabModelSelector).getTabModelFilterProvider();
        doNothing()
                .when(mTabModelFitlerProvider)
                .addTabModelFilterObserver(any(TabModelObserver.class));
        doNothing().when(mTabModelSelectorTabObserver).destroy();
    }

    @Test
    public void testAddTab() {
        TabContextObserverTestHelper tabContextObserverTestHelper =
                new TabContextObserverTestHelper(mTabModelSelector);
        tabContextObserverTestHelper.mTabModelObserver.didAddTab(
                null, 0, TabCreationState.LIVE_IN_FOREGROUND);
        Assert.assertEquals(TabContextObserver.TabContextChangeReason.TAB_ADDED,
                tabContextObserverTestHelper.getChangeReason());
    }

    @Test
    public void testMoveTab() {
        TabContextObserverTestHelper tabContextObserverTestHelper =
                new TabContextObserverTestHelper(mTabModelSelector);
        tabContextObserverTestHelper.mTabModelObserver.didMoveTab(null, 0, 0);
        Assert.assertEquals(TabContextObserver.TabContextChangeReason.TAB_MOVED,
                tabContextObserverTestHelper.getChangeReason());
    }

    @Test
    public void testCloseTab() {
        TabContextObserverTestHelper tabContextObserverTestHelper =
                new TabContextObserverTestHelper(mTabModelSelector);
        tabContextObserverTestHelper.mTabModelObserver.willCloseTab(null, false);
        Assert.assertEquals(TabContextObserver.TabContextChangeReason.TAB_CLOSED,
                tabContextObserverTestHelper.getChangeReason());
    }

    @Test
    public void testUndoClosedTab() {
        TabContextObserverTestHelper tabContextObserverTestHelper =
                new TabContextObserverTestHelper(mTabModelSelector);
        tabContextObserverTestHelper.mTabModelObserver.tabClosureUndone(null);
        Assert.assertEquals(TabContextObserver.TabContextChangeReason.TAB_CLOSURE_UNDONE,
                tabContextObserverTestHelper.getChangeReason());
    }

    @Test
    public void testDidFirstVisuallyNonEmptyPaint() {
        TabContextObserverTestHelper tabContextObserverTestHelper =
                new TabContextObserverTestHelper(mTabModelSelector);
        tabContextObserverTestHelper.mTabModelSelectorTabObserver.didFirstVisuallyNonEmptyPaint(
                null);
        Assert.assertEquals(
                TabContextObserver.TabContextChangeReason.FIRST_VISUALLY_NON_EMPTY_PAINT,
                tabContextObserverTestHelper.getChangeReason());
    }

    @Test
    public void testDestroy() {
        TabContextObserverTestHelper tabContextObserverTestHelper =
                new TabContextObserverTestHelper(mTabModelSelector);
        tabContextObserverTestHelper.mTabModelSelectorTabObserver = mTabModelSelectorTabObserver;
        tabContextObserverTestHelper.destroy();
        verify(mTabModelSelectorTabObserver, times(1)).destroy();
    }
}
