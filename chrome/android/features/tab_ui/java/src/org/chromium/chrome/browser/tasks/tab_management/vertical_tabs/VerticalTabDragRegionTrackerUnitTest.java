// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link VerticalTabDragRegionTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabDragRegionTrackerUnitTest {

    private int mEnterCount;
    private int mExitCount;
    private VerticalTabDragRegionTracker mTracker;

    @Before
    public void setUp() {
        mEnterCount = 0;
        mExitCount = 0;
        mTracker = new VerticalTabDragRegionTracker(() -> mEnterCount++, () -> mExitCount++);
    }

    private void assertCounts(int expectedEnters, int expectedExits) {
        assertEquals("enter effects", expectedEnters, mEnterCount);
        assertEquals("exit effects", expectedExits, mExitCount);
    }

    @Test
    public void testDragStart_IsSilentAndStartsOutside() {
        mTracker.onDragStarted(/* isInsideRegion= */ false);

        assertCounts(0, 0);
    }

    @Test
    public void testDragStart_IsSilentAndStartsInside() {
        mTracker.onDragStarted(/* isInsideRegion= */ true);

        assertCounts(0, 0);
    }

    @Test
    public void testDragStart_IsIdempotent() {
        // Every view registered with the drag handler delivers its own ACTION_DRAG_STARTED.
        mTracker.onDragStarted(/* isInsideRegion= */ false);
        mTracker.onDragStarted(/* isInsideRegion= */ false);
        mTracker.onDragStarted(/* isInsideRegion= */ false);

        assertCounts(0, 0);
    }

    @Test
    public void testLocation_FiresOnEdgesOnly() {
        mTracker.onDragStarted(/* isInsideRegion= */ false);

        mTracker.onLocation(/* isInsideRegion= */ true);
        assertCounts(1, 0);

        // Staying inside is not an edge.
        mTracker.onLocation(/* isInsideRegion= */ true);
        mTracker.onLocation(/* isInsideRegion= */ true);
        assertCounts(1, 0);

        mTracker.onLocation(/* isInsideRegion= */ false);
        assertCounts(1, 1);

        // Staying outside is not an edge either.
        mTracker.onLocation(/* isInsideRegion= */ false);
        assertCounts(1, 1);

        mTracker.onLocation(/* isInsideRegion= */ true);
        assertCounts(2, 1);
    }

    @Test
    public void testExitedContainer_FromInside_FiresExit() {
        mTracker.onDragStarted(/* isInsideRegion= */ false);
        mTracker.onLocation(/* isInsideRegion= */ true);
        assertCounts(1, 0);

        // The pointer left the window. No further location will report it, so this edge has to be
        // acted on directly.
        mTracker.onExitedContainer();
        assertCounts(1, 1);
    }

    @Test
    public void testExitedContainer_FromOutside_IsSilent() {
        mTracker.onDragStarted(/* isInsideRegion= */ false);

        mTracker.onExitedContainer();
        mTracker.onExitedContainer();

        assertCounts(0, 0);
    }

    @Test
    public void testNoDrag_IgnoresLocationAndExitedContainer() {
        // Without an active drag session, location and exitedContainer events are ignored.
        mTracker.onLocation(/* isInsideRegion= */ true);
        mTracker.onExitedContainer();
        assertCounts(0, 0);
    }

    @Test
    public void testDragEnded_IsSilentEvenFromInside() {
        mTracker.onDragStarted(/* isInsideRegion= */ false);
        mTracker.onLocation(/* isInsideRegion= */ true);
        assertCounts(1, 0);

        mTracker.onDragEnded();
        assertCounts(1, 0);
    }

    @Test
    public void testDragEnded_ResetsForTheNextSession() {
        mTracker.onDragStarted(/* isInsideRegion= */ false);
        mTracker.onLocation(/* isInsideRegion= */ true);
        mTracker.onDragEnded();

        // A second session must be able to report entering the region again.
        mTracker.onDragStarted(/* isInsideRegion= */ false);
        mTracker.onLocation(/* isInsideRegion= */ true);
        assertCounts(2, 0);
    }
}
