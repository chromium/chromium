// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link VerticalTabDragUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabDragUtilsUnitTest {

    private View mSourceView;
    private View mTargetView;

    @Before
    public void setUp() {
        mSourceView = new View(ContextUtils.getApplicationContext());
        mTargetView = new View(ContextUtils.getApplicationContext());
        // Only the size matters; screen locations come from the test LocationProvider.
        mTargetView.layout(0, 0, 200, 100);
    }

    @Test
    public void testMapCoordinatesToView_NullTargetOrSameView_CopiesCoordinatesUnchanged() {
        float[] outCoords = new float[2];

        VerticalTabDragUtils.mapCoordinatesToView(
                mSourceView, 15.5f, 25.5f, /* targetView= */ null, outCoords);
        assertEquals(15.5f, outCoords[0], 0.01f);
        assertEquals(25.5f, outCoords[1], 0.01f);

        VerticalTabDragUtils.mapCoordinatesToView(
                mSourceView, 30.0f, 40.0f, mSourceView, outCoords);
        assertEquals(30.0f, outCoords[0], 0.01f);
        assertEquals(40.0f, outCoords[1], 0.01f);
    }

    @Test
    public void testMapCoordinatesToView_TranslatesUsingScreenLocations() {
        VerticalTabDragUtils.setLocationProviderForTesting(
                (view, outLoc) -> {
                    if (view == mSourceView) {
                        outLoc[0] = 100;
                        outLoc[1] = 300;
                    } else if (view == mTargetView) {
                        outLoc[0] = 40;
                        outLoc[1] = 500;
                    }
                });

        float[] outCoords = new float[2];
        VerticalTabDragUtils.mapCoordinatesToView(mSourceView, 10f, 20f, mTargetView, outCoords);

        // outX = 10 + 100 - 40 = 70
        // outY = 20 + 300 - 500 = -180
        assertEquals(70f, outCoords[0], 0.01f);
        assertEquals(-180f, outCoords[1], 0.01f);
    }

    @Test
    public void testMapCoordinatesToView_PartialLocationProvider_ZeroesScratchBuffers() {
        // First call populates non-zero X and Y coordinates in static scratch buffers.
        VerticalTabDragUtils.setLocationProviderForTesting(
                (view, outLoc) -> {
                    outLoc[0] = 500;
                    outLoc[1] = 600;
                });
        float[] outCoords = new float[2];
        VerticalTabDragUtils.mapCoordinatesToView(mSourceView, 0f, 0f, mTargetView, outCoords);

        // Second call uses a partial test lambda that only sets outLoc[1], relying on zeroing of
        // outLoc[0].
        VerticalTabDragUtils.setLocationProviderForTesting(
                (view, outLoc) -> {
                    if (view == mTargetView) {
                        outLoc[1] = 200;
                    }
                });
        VerticalTabDragUtils.mapCoordinatesToView(mSourceView, 10f, 50f, mTargetView, outCoords);

        assertEquals(10f, outCoords[0], 0.01f);
        assertEquals(-150f, outCoords[1], 0.01f);
    }

    @Test
    public void testIsPointInsideView_NullTarget_ReturnsFalse() {
        assertFalse(
                VerticalTabDragUtils.isPointInsideView(
                        mSourceView, 50f, 50f, /* targetView= */ null));
    }

    @Test
    public void testIsPointInsideView_BoundaryConditions() {
        VerticalTabDragUtils.setLocationProviderForTesting(
                (view, outLoc) -> {
                    outLoc[0] = 0;
                    outLoc[1] = 0;
                });

        // Inclusive top-left (0, 0)
        assertTrue(VerticalTabDragUtils.isPointInsideView(mSourceView, 0f, 0f, mTargetView));

        // Inclusive interior near bottom-right (width - 1, height - 1)
        assertTrue(VerticalTabDragUtils.isPointInsideView(mSourceView, 199.9f, 99.9f, mTargetView));

        // Exclusive bottom-right edges (width, height)
        assertFalse(VerticalTabDragUtils.isPointInsideView(mSourceView, 200f, 50f, mTargetView));
        assertFalse(VerticalTabDragUtils.isPointInsideView(mSourceView, 50f, 100f, mTargetView));

        // Negative coordinates outside top/left
        assertFalse(VerticalTabDragUtils.isPointInsideView(mSourceView, -0.1f, 50f, mTargetView));
        assertFalse(VerticalTabDragUtils.isPointInsideView(mSourceView, 50f, -0.1f, mTargetView));
    }
}
