// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.view.View;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;

/** Geometry helpers shared by the vertical tab strip's drag-and-drop code. */
@NullMarked
class VerticalTabDragUtils {

    /** Interface for querying a view's screen coordinates, overridable in unit tests. */
    @FunctionalInterface
    interface LocationProvider {
        void getLocationOnScreen(View view, int[] outLocation);
    }

    private static LocationProvider sLocationProvider = View::getLocationOnScreen;

    // Android drag dispatch is strictly confined to the UI thread; static scratch buffers
    // avoid heap allocations on 60-120Hz ACTION_DRAG_LOCATION frames.
    private static final int[] sTempSourceLoc = new int[2];
    private static final int[] sTempTargetLoc = new int[2];
    private static final float[] sTempMappedCoords = new float[2];

    private VerticalTabDragUtils() {}

    /**
     * Translates a point from {@code sourceView}'s coordinate space into {@code targetView}'s.
     *
     * <p>If {@code targetView} is null or identical to {@code sourceView}, the point is copied
     * through unchanged.
     *
     * @param sourceView The view {@code (sourceX, sourceY)} is relative to.
     * @param sourceX X coordinate relative to {@code sourceView}.
     * @param sourceY Y coordinate relative to {@code sourceView}.
     * @param targetView The view to translate the point into.
     * @param outCoords Length-2 array that receives the translated {x, y}.
     */
    static void mapCoordinatesToView(
            View sourceView,
            float sourceX,
            float sourceY,
            @Nullable View targetView,
            float[] outCoords) {
        ThreadUtils.assertOnUiThread();
        if (targetView == null || sourceView == targetView) {
            outCoords[0] = sourceX;
            outCoords[1] = sourceY;
            return;
        }
        Arrays.fill(sTempSourceLoc, 0);
        Arrays.fill(sTempTargetLoc, 0);
        sLocationProvider.getLocationOnScreen(sourceView, sTempSourceLoc);
        sLocationProvider.getLocationOnScreen(targetView, sTempTargetLoc);
        outCoords[0] = sourceX + sTempSourceLoc[0] - sTempTargetLoc[0];
        outCoords[1] = sourceY + sTempSourceLoc[1] - sTempTargetLoc[1];
    }

    /**
     * Translates a point from {@code sourceView}'s coordinate space into {@code targetView}'s and
     * returns whether the translated point falls within {@code targetView}'s bounds {@code [0,
     * width) x [0, height)}.
     *
     * <p>If {@code targetView} is null, returns false without modifying {@code outCoords}. Callers
     * must not read {@code outCoords} when this method returns false unless {@code targetView} is
     * known to be non-null.
     *
     * @param sourceView The view {@code (sourceX, sourceY)} is relative to.
     * @param sourceX X coordinate relative to {@code sourceView}.
     * @param sourceY Y coordinate relative to {@code sourceView}.
     * @param targetView The view to test against and translate into. If null, returns false.
     * @param outCoords Length-2 array that receives the translated {x, y} when targetView != null.
     */
    static boolean mapCoordinatesAndCheckBounds(
            View sourceView,
            float sourceX,
            float sourceY,
            @Nullable View targetView,
            float[] outCoords) {
        if (targetView == null) return false;

        mapCoordinatesToView(sourceView, sourceX, sourceY, targetView, outCoords);
        return outCoords[0] >= 0
                && outCoords[0] < targetView.getWidth()
                && outCoords[1] >= 0
                && outCoords[1] < targetView.getHeight();
    }

    /**
     * Returns whether {@code (sourceX, sourceY)} relative to {@code sourceView} falls within {@code
     * targetView}'s bounds {@code [0, width) x [0, height)}.
     */
    static boolean isPointInsideView(
            View sourceView, float sourceX, float sourceY, @Nullable View targetView) {
        return mapCoordinatesAndCheckBounds(
                sourceView, sourceX, sourceY, targetView, sTempMappedCoords);
    }

    static void setLocationProviderForTesting(LocationProvider provider) {
        sLocationProvider = provider;
        ResettersForTesting.register(() -> sLocationProvider = View::getLocationOnScreen);
    }
}
