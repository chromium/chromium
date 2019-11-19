// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.ViewGroup;

/**
 * Shows overscroll-like glow on the right edge when forward navigation reaches the end.
 */
abstract class NavigationGlow {
    protected final ViewGroup mParentView;

    public NavigationGlow(ViewGroup parentView) {
        mParentView = parentView;
    }

    /**
     * Prepares glow rendering by initialization of necessary objects and values.
     * @param startX X position of the touch event at the beginning.
     * @param startY Y position of the touch event at the beginning.
     */
    public abstract void prepare(float startX, float startY);

    /**
     * Called when user scroll is performed.
     * @param xDelta Amount of x scroll in pixel.
     */
    public abstract void onScroll(float xDelta);

    /**
     * Releases the glow UI in action.
     */
    public abstract void release();

    /**
     * Cancels the glow UI in action.
     */
    public abstract void reset();

    /**
     * Destroys internal objects when navigation logic is destroyed.
     */
    public abstract void destroy();
}
