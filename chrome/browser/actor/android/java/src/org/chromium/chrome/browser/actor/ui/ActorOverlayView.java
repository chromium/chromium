// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;

/**
 * The root view for the Actor Overlay. Displays the overlay content on top of the browser content.
 */
@NullMarked
public class ActorOverlayView extends FrameLayout {
    public ActorOverlayView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setClickable(true);
        setFocusable(true);
    }

    /**
     * Sets the visibility of the view.
     *
     * @param visible True to show the view, false to hide it (GONE).
     */
    public void setVisible(boolean visible) {
        setVisibility(visible ? VISIBLE : GONE);
    }

    /**
     * Sets the top and bottom margins of the view.
     *
     * @param top The top margin in pixels.
     * @param bottom The bottom margin in pixels.
     */
    public void setMargins(int top, int bottom) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        if (params.topMargin != top || params.bottomMargin != bottom) {
            params.topMargin = top;
            params.bottomMargin = bottom;
            setLayoutParams(params);
        }
    }
}
