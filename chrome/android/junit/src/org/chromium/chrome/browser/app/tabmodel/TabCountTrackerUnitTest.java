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
import org.chromium.chrome.browser.tabmodel.TabOrchestratorType;

/** Unit tests for {@link TabCountTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCountTrackerUnitTest {
    private static final String WINDOW_TAG_1 = "window_1";
    private static final String WINDOW_TAG_2 = "window_2";

    private TabCountTracker mTracker1;
    private TabCountTracker mTracker2;

    @Before
    public void setUp() {
        TabCountTracker.clearGlobalState();
        mTracker1 = new TabCountTracker(WINDOW_TAG_1, TabOrchestratorType.TABBED);
        mTracker2 = new TabCountTracker(WINDOW_TAG_2, TabOrchestratorType.TABBED);
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
    public void testOrchestratorTypeIsolation() {
        TabCountTracker tabbedTracker =
                new TabCountTracker(WINDOW_TAG_1, TabOrchestratorType.TABBED);
        TabCountTracker customTracker =
                new TabCountTracker(WINDOW_TAG_1, TabOrchestratorType.CUSTOM);
        TabCountTracker archivedTracker =
                new TabCountTracker(WINDOW_TAG_1, TabOrchestratorType.ARCHIVED);
        TabCountTracker headlessTracker =
                new TabCountTracker(WINDOW_TAG_1, TabOrchestratorType.HEADLESS);

        tabbedTracker.updateTabCount(/* incognito= */ false, 5);
        tabbedTracker.updateTabCount(/* incognito= */ true, 2);

        customTracker.updateTabCount(/* incognito= */ false, 3);
        customTracker.updateTabCount(/* incognito= */ true, 1);

        archivedTracker.updateTabCount(/* incognito= */ false, 7);
        archivedTracker.updateTabCount(/* incognito= */ true, 0);

        headlessTracker.updateTabCount(/* incognito= */ false, 9);
        headlessTracker.updateTabCount(/* incognito= */ true, 4);

        assertEquals(5, tabbedTracker.getRestoredTabCount(false));
        assertEquals(2, tabbedTracker.getRestoredTabCount(true));

        assertEquals(3, customTracker.getRestoredTabCount(false));
        assertEquals(1, customTracker.getRestoredTabCount(true));

        assertEquals(7, archivedTracker.getRestoredTabCount(false));
        assertEquals(0, archivedTracker.getRestoredTabCount(true));

        assertEquals(9, headlessTracker.getRestoredTabCount(false));
        assertEquals(4, headlessTracker.getRestoredTabCount(true));

        // Clearing individual models on one orchestrator does not affect the others.
        tabbedTracker.clearTabCount(false);
        assertEquals(0, tabbedTracker.getRestoredTabCount(false));
        assertEquals(2, tabbedTracker.getRestoredTabCount(true));

        assertEquals(3, customTracker.getRestoredTabCount(false));
        assertEquals(1, customTracker.getRestoredTabCount(true));
        assertEquals(7, archivedTracker.getRestoredTabCount(false));
        assertEquals(0, archivedTracker.getRestoredTabCount(true));
        assertEquals(9, headlessTracker.getRestoredTabCount(false));
        assertEquals(4, headlessTracker.getRestoredTabCount(true));

        tabbedTracker.clearTabCount(true);
        assertEquals(0, tabbedTracker.getRestoredTabCount(false));
        assertEquals(0, tabbedTracker.getRestoredTabCount(true));

        assertEquals(3, customTracker.getRestoredTabCount(false));
        assertEquals(1, customTracker.getRestoredTabCount(true));
        assertEquals(7, archivedTracker.getRestoredTabCount(false));
        assertEquals(0, archivedTracker.getRestoredTabCount(true));
        assertEquals(9, headlessTracker.getRestoredTabCount(false));
        assertEquals(4, headlessTracker.getRestoredTabCount(true));
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
        TabCountTracker headlessTracker1 =
                new TabCountTracker(WINDOW_TAG_1, TabOrchestratorType.HEADLESS);
        headlessTracker1.updateTabCount(false, 8);
        headlessTracker1.updateTabCount(true, 4);

        mTracker2.updateTabCount(false, 10);
        mTracker2.updateTabCount(true, 6);

        mTracker1.clearCurrentWindow();

        assertEquals(0, mTracker1.getRestoredTabCount(false));
        assertEquals(0, mTracker1.getRestoredTabCount(true));
        assertEquals(0, headlessTracker1.getRestoredTabCount(false));
        assertEquals(0, headlessTracker1.getRestoredTabCount(true));

        assertEquals(10, mTracker2.getRestoredTabCount(false));
        assertEquals(6, mTracker2.getRestoredTabCount(true));
    }

    @Test
    public void testCleanupWindow_Static() {
        mTracker1.updateTabCount(false, 8);
        mTracker2.updateTabCount(false, 12);
        TabCountTracker headlessTracker1 =
                new TabCountTracker(WINDOW_TAG_1, TabOrchestratorType.HEADLESS);
        headlessTracker1.updateTabCount(false, 4);
        headlessTracker1.updateTabCount(true, 2);

        TabCountTracker.cleanupWindow(WINDOW_TAG_1);

        assertEquals(0, mTracker1.getRestoredTabCount(false));
        assertEquals(0, headlessTracker1.getRestoredTabCount(false));
        assertEquals(0, headlessTracker1.getRestoredTabCount(true));
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
