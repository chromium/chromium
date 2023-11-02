// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

/**
 * Encapsulates the state of a recent Tap gesture; x, y position and the timestamp.
 * Instances of this class are immutable.
 */
class ContextualSearchTapState {
    private final float mX;
    private final float mY;
    private final long mTapTimeNanoseconds;

    /**
     * Constructs a Tap at the given x,y position and indicates if the tap was suppressed or not.
     * @param x The x coordinate of the current tap.
     * @param y The y coordinate of the current tap.
     * @param tapTimeNanoseconds The timestamp when the Tap occurred.
     */
    ContextualSearchTapState(float x, float y, long tapTimeNanoseconds) {
        mX = x;
        mY = y;
        mTapTimeNanoseconds = tapTimeNanoseconds;
    }

    /**
     * @return The x coordinate of the Tap.
     */
    float getX() {
        return mX;
    }

    /**
     * @return The y coordinate of the Tap.
     */
    float getY() {
        return mY;
    }

    /**
     * @return The time of the Tap in nanoseconds.
     */
    long tapTimeNanoseconds() {
        return mTapTimeNanoseconds;
    }
}
