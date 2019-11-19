// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import androidx.annotation.Px;

/**
 * This class holds the size of any component shown at the bottom of the screen. The height can be
 * used to either compute an offset for bottom bars (e.g. CCTs or PWAs) or to push up the content
 * area.
 */
public interface CompositorViewResizer {
    /**
     * Observers are notified when the size of the component changes.
     */
    interface Observer {
        /**
         * Called when the component height changes.
         * @param height The new height of the component.
         */
        void onHeightChanged(@Px int height);
    }

    /**
     * Returns the height of the component.
     * @return A height in pixels.
     */
    @Px
    int getHeight();

    /**
     * Registered observers are called whenever the components size changes until unregistered.
     * Does not guarantee order.
     * @param observer a {@link CompositorViewResizer.Observer}.
     */
    void addObserver(Observer observer);

    /**
     * Removes a registered observer if present.
     * @param observer a registered {@link CompositorViewResizer.Observer}.
     */
    void removeObserver(Observer observer);
}