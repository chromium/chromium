// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import android.content.Context;
import android.content.res.ColorStateList;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.styles.IncognitoColors;
import org.chromium.ui.R;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.ValueUtils;

/** Utility methods for Glic actions. */
@NullMarked
public class GlicActionUtils {

    private GlicActionUtils() {}

    /**
     * Returns the color state list for the disabled state in incognito mode.
     *
     * @param context The context used to resolve the color.
     * @return The color state list.
     */
    public static ColorStateList getIncognitoDisabledTint(Context context) {
        @ColorInt int onSurface = IncognitoColors.getColorOnSurface(context, true);
        float disabledAlpha =
                ValueUtils.getFloat(context.getResources(), R.dimen.default_disabled_alpha);
        @ColorInt
        int disabledColor = ColorUtils.setAlphaComponentWithFloat(onSurface, disabledAlpha);
        return ColorStateList.valueOf(disabledColor);
    }
}
