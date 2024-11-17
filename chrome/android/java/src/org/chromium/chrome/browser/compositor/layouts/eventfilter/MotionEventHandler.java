// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

/** Interface that describes motion event callbacks. */
public interface MotionEventHandler {
    /**
     * Called on down touch event.
     *
     * @param x         The X position of the event in the host view space in dp.
     * @param y         The Y position of the event in the host view space in dp.
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons   State of all buttons that are pressed.
     */
    void onDown(float x, float y, boolean fromMouse, int buttons);

    /** Called on up or cancel touch event. */
    void onUpOrCancel();

    /**
     * Called on drag/scroll touch event
     *
     * @param x  The X position of the event in the host view space in dp.
     * @param y  The Y position of the event in the host view space in dp.
     * @param dx The change in X since the last movement event.
     * @param dy The change in Y since the last movement event.
     * @param tx The total change in X since the start of this drag.
     * @param ty The total change in Y since the start of this drag.
     */
    void drag(float x, float y, float dx, float dy, float tx, float ty);

    /**
     * Called on click touch event.
     *
     * @param x         The X position of the event in the host view space in dp.
     * @param y         The Y position of the event in the host view space in dp.
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons   State of all buttons that were pressed when onDown was invoked.
     */
    void click(float x, float y, boolean fromMouse, int buttons);

    /**
     * Called on fling touch event.
     *
     * @param x The X position of the event in the host view space in dp.
     * @param y The Y position of the event in the host view space in dp.
     */
    void fling(float x, float y, float velocityX, float velocityY);

    /**
     * Called on long press touch event.
     *
     * @param x The X position of the event in the host view space in dp.
     * @param y The Y position of the event in the host view space in dp.
     */
    void onLongPress(float x, float y);

    /**
     * Called on pinch touch event.
     *
     * @param x0         The X position of the first finger in the host view space in dp.
     * @param y0         The Y position of the first finger in the host view space in dp.
     * @param x1         The X position of the second finger in the host view space in dp.
     * @param y1         The Y position of the second finger in the host view space in dp.
     * @param firstEvent Whether the onPinch call is the first of multiple.
     */
    void onPinch(float x0, float y0, float x1, float y1, boolean firstEvent);

    /**
     * Called on hover enter event.
     *
     * @param x The X position at the end of the event in the host view space in dp.
     * @param y The Y position at the end of the event in the host view space in dp.
     */
    void onHoverEnter(float x, float y);

    /**
     * Called on hover move event.
     *
     * @param x The X position at the end of the event in the host view space in dp.
     * @param y The Y position at the end of the event in the host view space in dp.
     */
    void onHoverMove(float x, float y);

    /** Called on hover exit event. */
    void onHoverExit();
}
