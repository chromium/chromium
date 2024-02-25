// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.view.View;

import androidx.annotation.NonNull;

/** This class handles snap scroll for the search box on a {@link NewTabPage}. */
public interface SnapScrollHelper {
    /** @param view The view on which this class needs to handle snap scroll. */
    void setView(@NonNull View view);

    /** Update scroll offset and perform snap scroll if necessary. */
    void handleScroll();

    /**
     * Resets any pending callbacks to update the search box position, and add new callback to
     * update the search box position if necessary. This is used whenever {@link #handleScroll()} is
     * not reliable (e.g. when an item is dismissed, the items at the top of the viewport might not
     * move, and onScrolled() might not be called).
     * @param update Whether a new callback to update search box should be posted to {@link #mView}.
     */
    void resetSearchBoxOnScroll(boolean update);

    /**
     * @param scrollPosition The scroll position that the snap scroll calculation is based on.
     * @return The modified scroll position that accounts for snap scroll.
     */
    int calculateSnapPosition(int scrollPosition);
}
