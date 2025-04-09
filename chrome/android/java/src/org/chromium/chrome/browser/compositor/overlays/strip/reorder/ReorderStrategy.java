// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.graphics.PointF;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;

public interface ReorderStrategy {
    /**
     * Begin reordering the interacting view.
     *
     * @param stripViews The list of {@link StripLayoutView}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param stripGroupTitles The list of {@link StripLayoutGroupTitle}.
     * @param interactingView The interacting {@link StripLayoutView}.
     * @param startPoint The (x,y) coordinate that the reorder action began at.
     */
    void startReorderMode(
            StripLayoutView[] stripViews,
            StripLayoutTab[] stripTabs,
            StripLayoutGroupTitle[] stripGroupTitles,
            @NonNull StripLayoutView interactingView,
            PointF startPoint);

    /**
     * Updates the location of the reordering tab. This 1. visually offsets the tab (clamped to the
     * bounds of the strip) and 2. triggers a reorder (with animations) if the threshold is reached.
     *
     * @param stripViews The list of {@link StripLayoutView}.
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param endX The x position where this update ended.
     * @param deltaX The change in position for the reordering tab based on dragging and scrolling.
     * @param reorderType The type {@link ReorderType} of reorder for this update event.
     */
    void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float endX,
            float deltaX,
            @ReorderType int reorderType);

    /**
     * Stop reorder mode and clear any relevant state. Don't call if not in reorder mode.
     *
     * @param stripViews The list of {@link StripLayoutView}.
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     */
    void stopReorderMode(StripLayoutView[] stripViews, StripLayoutGroupTitle[] groupTitles);

    /** Returns the dragged {@link StripLayoutView} for the reorder. */
    StripLayoutView getInteractingView();
}
