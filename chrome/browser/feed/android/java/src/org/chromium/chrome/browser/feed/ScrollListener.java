// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.IntDef;

/** Interface to listen to the scroll events from the scrollable container of NTP. */
public interface ScrollListener {
    /**
     * Constant used to denote that a scroll was performed but scroll delta could not be
     * calculated. This normally maps to a programmatic scroll.
     */
    int UNKNOWN_SCROLL_DELTA = Integer.MIN_VALUE;

    /** Called when the scroll state changes. */
    void onScrollStateChanged(@ScrollState int state);

    /**
     * Called when a scroll happens and provides the amount of pixels scrolled. {@link
     * #UNKNOWN_SCROLL_DELTA} will be specified if scroll delta would not be determined. An
     * example of this would be a scroll initiated programmatically.
     */
    void onScrolled(int dx, int dy);

    /**
     * Called when the vertical offset of the header (1st item) in the scrollable container changes.
     * This gives the new offset of the header which is inversely proportional with the scroll
     * offset. For example, if the scroll offset is 50px, the header offset will be -50px.
     */
    void onHeaderOffsetChanged(int verticalOffset);

    /**
     * Possible scroll states.
     *
     * <p>When adding new values, the value of {@link ScrollState#NEXT_VALUE} should be used and
     * incremented. When removing values, {@link ScrollState#NEXT_VALUE} should not be changed,
     * and those values should not be reused.
     */
    @IntDef({ScrollState.IDLE, ScrollState.DRAGGING, ScrollState.SETTLING, ScrollState.NEXT_VALUE})
    @interface ScrollState {
        /** Is not scrolling */
        int IDLE = 0;

        /** Is currently scrolling through external means such as user input. */
        int DRAGGING = 1;

        /** Is animating to a final position. */
        int SETTLING = 2;

        /** The next value that should be used when adding additional values to the IntDef. */
        int NEXT_VALUE = 3;
    }
}
