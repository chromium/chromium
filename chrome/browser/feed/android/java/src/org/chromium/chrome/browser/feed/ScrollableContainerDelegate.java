// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.view.View;

/**
 * Delegate that represents the scrollable container that contains the different NTP views (e.g.,
 * omnibox, feed, etc.).
 */
public interface ScrollableContainerDelegate {
    /** Adds a |listener| for the scroll events from the root surface. */
    void addScrollListener(ScrollListener listener);

    /** Removes a |listener| for the scroll events from the root surface. */
    void removeScrollListener(ScrollListener listener);

    /** Gets the absolute value of the vertical scroll offset on the root surface. */
    int getVerticalScrollOffset();

    /** Gets the height of the view of the root surface. */
    int getRootViewHeight();

    /**
     * Gets the top position of the |childView| relative to the view of the container. The 2 views
     * need to have a hierarchical relation in the view tree. It can be an indirect relation, e.g.,
     * the |childView| is the grand grand child of the container view down the tree.
     */
    int getTopPositionRelativeToContainerView(View childView);
}
