// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.common;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.util.TypedValue;
import android.view.View;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** This helper class contains shared utilities used for the touch-to-fill functionality. */
public class TouchToFillUtil {
    private TouchToFillUtil() {}

    /**
     * A helper method to set the background and ripple effect for the given view.
     *
     * @param view The {@link View} on which background and ripple effect will be applied.
     * @param background The GradientDrawable to be used as the base background for the view.
     * @param context he Context used to access theme attributes and resources.
     */
    public static void addColorAndRippleToBackground(
            View view, GradientDrawable background, Context context) {
        TypedValue themeRes = new TypedValue();
        context.getTheme().resolveAttribute(android.R.attr.colorControlHighlight, themeRes, true);
        RippleDrawable rippleDrawable =
                new RippleDrawable(ColorStateList.valueOf(themeRes.data), background, null);
        view.setBackground(rippleDrawable);
        view.setBackgroundTintList(
                ColorStateList.valueOf(SemanticColorUtils.getDefaultBgColorElev1(context)));
    }
}
