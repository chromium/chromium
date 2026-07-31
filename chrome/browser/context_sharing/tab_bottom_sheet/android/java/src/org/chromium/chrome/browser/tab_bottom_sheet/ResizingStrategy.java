// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;

/**
 * Strategy interface for deciding when to show the resizing placeholder in {@link
 * WebViewResizingHelper} and managing the resizing lock lifecycle.
 */
@NullMarked
public interface ResizingStrategy {
    /**
     * Called when the sheet offset or height bounds change.
     *
     * @param offsetPx The current sheet offset in pixels.
     * @param peekHeightPx The peek height bound of the sheet in pixels.
     * @param halfHeightPx The half height bound of the sheet in pixels.
     * @param fullHeightPx The full height bound of the sheet in pixels.
     */
    void onSheetOffsetChanged(
            float offsetPx, float peekHeightPx, float halfHeightPx, float fullHeightPx);

    /** Called when the sheet starts or stops resizing/scrolling. */
    void onSheetResizingStatusChanged(boolean isResizing);

    /** Destroys the strategy, releasing any held lock and unregistering observers. */
    void destroy();
}
