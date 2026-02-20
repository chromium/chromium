// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;

/**
 * The root view for the Actor Overlay. Displays the overlay content on top of the browser content.
 */
@NullMarked
public class ActorOverlayView extends FrameLayout {
    public ActorOverlayView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the visibility of the view.
     *
     * @param visible True to show the view, false to hide it (GONE).
     */
    public void setVisible(boolean visible) {
        setVisibility(visible ? VISIBLE : GONE);
    }
}
