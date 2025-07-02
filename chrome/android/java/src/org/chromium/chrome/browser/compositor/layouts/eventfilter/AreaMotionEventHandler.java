// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.eventfilter;

import org.chromium.build.annotations.NullMarked;

/** Interface that describes motion event callbacks with a specific trigger area. */
@NullMarked
public interface AreaMotionEventHandler extends MotionEventHandler {
    @Override
    default void onHoverExit() {
        onHoverExit(/* inArea= */ false);
    }

    /**
     * Called on hover exit event.
     *
     * @param inArea {@code true} if the exit event occurred due to a motion event (such as click)
     *     within the trigger area. {@code false} if the event occurred due to the pointer leaving
     *     the bounds of the trigger area.
     */
    void onHoverExit(boolean inArea);
}
