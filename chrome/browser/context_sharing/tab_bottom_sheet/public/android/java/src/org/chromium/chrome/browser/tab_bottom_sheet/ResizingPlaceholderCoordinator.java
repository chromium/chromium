// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;

/** Coordinates the view and visual transitions of resizing placeholders in Tab Bottom Sheet. */
@NullMarked
public interface ResizingPlaceholderCoordinator {
    /** Returns the root view of the placeholder to display behind WebContents. */
    View getView();

    /**
     * Updates the visible height of the placeholder and recalculates internal animations or alpha.
     *
     * @param visibleHeight The visible height in pixels.
     */
    void updateVisibleHeight(@Px int visibleHeight);

    /** Destroys the coordinator and cleans up any observers or resources. */
    default void destroy() {}
}
