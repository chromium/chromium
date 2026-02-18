// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabCountTrackerUnitTest}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCountTrackerUnitTest {
    private static final String WINDOW_TAG_1 = "window_1";
    private static final String WINDOW_TAG_2 = "window_2";

    private TabCountTracker mTracker1;
    private TabCountTracker mTracker2;

    @Before
    public void setUp() {
        TabCountTracker.clearGlobalState();
        mTracker1 = new TabCountTracker(WINDOW_TAG_1);
        mTracker2 = new TabCountTracker(WINDOW_TAG_2);
    }

    @After
    public void tearDown() {
        TabCountTracker.clearGlobalState();
    }

    @Test
    public void testUpdateAndGetCount() {
        mTracker1.updateTabCount(/* incognito= */ false, 5);
        mTracker1.updateTabCount(/* incognito= */ true, 3);

        assertEquals(5, mTracker1.getRestoredTabCount(false));
        assertEquals(3, mTracker1.getRestoredTabCount(true));
    }

    @Test
    public void testDefaultCountIsZero() {
        assertEquals(0, mTracker1.getRestoredTabCount(false));
        assertEquals(0, mTracker1.getRestoredTabCount(true));
    }

    @Test
    public void testWindowIsolation() {
        mTracker1.updateTabCount(false, 10);
        mTracker2.updateTabCount(false, 20);

        assertEquals(10, mTracker1.getRestoredTabCount(false));
        assertEquals(20, mTracker2.getRestoredTabCount(false));
    }

    @Test
    public void testClearTabCount() {
        mTracker1.updateTabCount(false, 5);
        mTracker1.updateTabCount(true, 5);

        mTracker1.clearTabCount(false);

        assertEquals(0, mTracker1.getRestoredTabCount(false));
        assertEquals(5, mTracker1.getRestoredTabCount(true));
    }

    @Test
    public void testClearCurrentWindow() {
        mTracker1.updateTabCount(false, 5);
        mTracker1.updateTabCount(true, 5);
        mTracker2.updateTabCount(false, 10);

        mTracker1.clearCurrentWindow();

        assertEquals(0, mTracker1.getRestoredTabCount(false));
        assertEquals(0, mTracker1.getRestoredTabCount(true));
        assertEquals(10, mTracker2.getRestoredTabCount(false));
    }

    @Test
    public void testCleanupWindow_Static() {
        mTracker1.updateTabCount(false, 8);
        mTracker2.updateTabCount(false, 12);

        TabCountTracker.cleanupWindow(WINDOW_TAG_1);

        assertEquals(0, mTracker1.getRestoredTabCount(false));
        assertEquals(12, mTracker2.getRestoredTabCount(false));
    }

    @Test
    public void testClearGlobalState() {
        mTracker1.updateTabCount(false, 1);
        mTracker2.updateTabCount(false, 2);

        TabCountTracker.clearGlobalState();

        assertEquals(0, mTracker1.getRestoredTabCount(false));
        assertEquals(0, mTracker2.getRestoredTabCount(false));
    }
}
