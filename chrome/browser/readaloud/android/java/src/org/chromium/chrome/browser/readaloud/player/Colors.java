// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.view.View;
import android.widget.ProgressBar;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Common functions for dynamic colors needed by both players. */
@NullMarked
public class Colors {
    /**
     * Returns the color that should be used for the player background.
     *
     * @param context Activity context.
     * @return Color in ARGB 8888 format.
     */
    public static @ColorInt int getMiniPlayerBackgroundColor(Context context) {
        return getPlayerBackgroundColor(context);
    }

    /**
     * Set a bottom sheet content view's background color.
     *
     * @param sheetContent Bottom sheet content.
     */
    public static void setBottomSheetContentBackground(View sheetContent) {
        // Just calling setBackgroundColor() hides the bottom sheet container's rounded corners. We
        // need to create a drawable with rounded corners on which the color can be set.
        Context context = sheetContent.getContext();
        float radius = context.getResources().getDimension(R.dimen.bottom_sheet_corner_radius);
        // Round the top left and top right corners.
        ShapeDrawable background =
                new ShapeDrawable(
                        new RoundRectShape(
                                /* outerRadii= */ new float[] {
                                    radius, radius, radius, radius, 0f, 0f, 0f, 0f
                                },
                                /* inset= */ null,
                                /* innerRadii= */ null));
        background.setTint(getPlayerBackgroundColor(context));
        sheetContent.setBackground(background);
    }

    /**
     * Sets the color of the given progress bar.
     *
     * @param bar Progress bar to be modified.
     */
    public static void setProgressBarColor(ProgressBar bar) {
        Context context = bar.getContext();
        bar.setProgressTintList(
                ColorStateList.valueOf(
                        ColorUtils.inNightMode(context)
                                ? SemanticColorUtils.getDefaultIconColorAccent1(context)
                                : SemanticColorUtils.getDefaultIconColor(context)));
    }

    private static @ColorInt int getPlayerBackgroundColor(Context context) {
        return SemanticColorUtils.getDefaultBgColor(context);
    }
}
