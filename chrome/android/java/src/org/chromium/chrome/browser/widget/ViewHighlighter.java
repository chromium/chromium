// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;

/**
 * A helper class to draw an overlay layer on the top of a view to enable highlighting. The overlay
 * layer can be specified to be a circle or a rectangle.
 */
public class ViewHighlighter {
    /**
     * Represents the delay between when menu anchor/toolbar handle/expand button was tapped and
     * when the menu or bottom sheet opened up. Unfortunately this is sensitive because if it is too
     * small, we might clear the state before the menu item got a chance to be highlighted. If it is
     * too large, user might tap somewhere else and then open the menu/bottom sheet to see the UI
     * still highlighted.
     */
    public static final int IPH_MIN_DELAY_BETWEEN_TWO_HIGHLIGHTS = 200;

    /**
     * Create a highlight layer over the view.
     * @param view The view to be highlighted.
     * @param circular Whether the highlight should be a circle or rectangle.
     */
    public static void turnOnHighlight(View view, boolean circular) {
        if (view == null) return;

        boolean highlighted = view.getTag(R.id.highlight_state) != null
                ? (boolean) view.getTag(R.id.highlight_state)
                : false;
        if (highlighted) return;

        PulseDrawable pulseDrawable = circular
                ? PulseDrawable.createCircle(ContextUtils.getApplicationContext())
                : PulseDrawable.createHighlight();

        Resources resources = ContextUtils.getApplicationContext().getResources();
        Drawable background = (Drawable) view.getBackground();
        if (background != null) {
            background = background.getConstantState().newDrawable(resources);
        }

        LayerDrawable drawable = ApiCompatibilityUtils.createLayerDrawable(background == null
                        ? new Drawable[] {pulseDrawable}
                        : new Drawable[] {background, pulseDrawable});
        view.setBackground(drawable);
        view.setTag(R.id.highlight_state, true);

        pulseDrawable.start();
    }

    /**
     * Turns off the highlight from the view. The original background of the view is restored.
     * @param view The associated view.
     */
    public static void turnOffHighlight(View view) {
        if (view == null) return;

        boolean highlighted = view.getTag(R.id.highlight_state) != null
                ? (boolean) view.getTag(R.id.highlight_state)
                : false;
        if (!highlighted) return;
        view.setTag(R.id.highlight_state, false);

        Resources resources = ContextUtils.getApplicationContext().getResources();
        Drawable existingBackground = view.getBackground();
        if (existingBackground instanceof LayerDrawable) {
            LayerDrawable layerDrawable = (LayerDrawable) existingBackground;
            if (layerDrawable.getNumberOfLayers() >= 2) {
                view.setBackground(
                        layerDrawable.getDrawable(0).getConstantState().newDrawable(resources));
            } else {
                view.setBackground(null);
            }
        }
    }
}