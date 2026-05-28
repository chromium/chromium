// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.core.content.ContextCompat;

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

        InnerGlowDrawable normalDrawable = InnerGlowDrawable.createMainWebpageGlow(context);
        InnerGlowDrawable normalDrawableForHover = InnerGlowDrawable.createMainWebpageGlow(context);

        int hoverColor = ContextCompat.getColor(context, R.color.actor_overlay_hover_color);
        Drawable hoverHighlight = new ColorDrawable(hoverColor);
        Drawable hoverDrawable =
                new LayerDrawable(new Drawable[] {hoverHighlight, normalDrawableForHover});

        StateListDrawable stateListDrawable = new StateListDrawable();
        stateListDrawable.addState(new int[] {android.R.attr.state_hovered}, hoverDrawable);
        stateListDrawable.addState(new int[] {}, normalDrawable);

        setBackground(stateListDrawable);
    }

    /**
     * Sets the margins of the view.
     *
     * @param left The left margin in pixels.
     * @param top The top margin in pixels.
     * @param right The right margin in pixels.
     * @param bottom The bottom margin in pixels.
     */
    public void setMargins(int left, int top, int right, int bottom) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        if (params == null) return;

        if (params.leftMargin != left
                || params.topMargin != top
                || params.rightMargin != right
                || params.bottomMargin != bottom) {
            params.leftMargin = left;
            params.topMargin = top;
            params.rightMargin = right;
            params.bottomMargin = bottom;
            setLayoutParams(params);
        }
    }

    /** Returns the take over task button. */
    public View getTakeOverButton() {
        return findViewById(R.id.take_over_task_button);
    }
}
