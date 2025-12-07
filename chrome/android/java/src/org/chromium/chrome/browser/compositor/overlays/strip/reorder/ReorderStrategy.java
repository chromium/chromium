// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import android.graphics.PointF;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTabDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.ReorderType;

@NullMarked
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
            StripLayoutView interactingView,
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
    @Nullable StripLayoutView getInteractingView();

    /**
     * Called to trigger an animated reorder when not in reorder mode. This can be triggered through
     * keyboard shortcuts.
     *
     * @param tabDelegate The {@link StripLayoutTabDelegate} for updating tab visuals.
     * @param stripViews The list of {@link StripLayoutView}.
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param reorderingView The view to reorder.
     * @param toLeft {@code True} if reordering the view to the left.
     */
    void reorderViewInDirection(
            StripLayoutTabDelegate tabDelegate,
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            StripLayoutView reorderingView,
            boolean toLeft);

    /** Returns true if auto-scroll is allowed during reorder. */
    default boolean shouldAllowAutoScroll() {
        return true;
    }
}
