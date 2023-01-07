// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

/**
 * A delegate used to obtain information about scroll state and perform various scroll
 * functions.
 */
public interface FeedSurfaceScrollDelegate {
    /**
     * @return Whether the scroll view is initialized. If false, the other delegate methods
     *         may not be valid.
     */
    boolean isScrollViewInitialized();

    /**
     * Checks whether the child at a given position is visible.
     * @param position The position of the child to check.
     * @return True if the child is at least partially visible.
     */
    boolean isChildVisibleAtPosition(int position);

    /**
     * @return The vertical scroll offset of the view containing this layout in pixels. Not
     *         valid until scroll view is initialized.
     */
    int getVerticalScrollOffset();

    /**
     * Snaps the scroll point of the scroll view to prevent the user from scrolling to midway
     * through a transition.
     */
    void snapScroll();
}
