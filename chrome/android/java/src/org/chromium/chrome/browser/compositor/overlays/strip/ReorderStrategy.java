// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

interface ReorderStrategy {
    /**
     * Updates the location of the reordering tab. This 1. visually offsets the tab (clamped to the
     * bounds of the strip) and 2. triggers a reorder (with animations) if the threshold is reached.
     *
     * @param stripViews The list of {@link StripLayoutView}.
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     * @param deltaX The change in position for the reordering tab based on dragging and scrolling.
     */
    void updateReorderPosition(
            StripLayoutView[] stripViews,
            StripLayoutGroupTitle[] groupTitles,
            StripLayoutTab[] stripTabs,
            float deltaX);

    /**
     * Stop reorder mode and clear any relevant state. Don't call if not in reorder mode.
     *
     * @param groupTitles The list of {@link StripLayoutGroupTitle}.
     * @param stripTabs The list of {@link StripLayoutTab}.
     */
    void stopReorderMode(StripLayoutGroupTitle[] groupTitles, StripLayoutTab[] stripTabs);
}
