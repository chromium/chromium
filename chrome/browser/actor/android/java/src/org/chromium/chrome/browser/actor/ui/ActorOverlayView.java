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
