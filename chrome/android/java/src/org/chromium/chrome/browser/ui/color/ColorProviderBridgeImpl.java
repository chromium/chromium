// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.color;

import android.content.Context;
import android.util.TypedValue;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.color.AndroidColorRole;
import org.chromium.ui.color.ColorProviderBridge;
import org.chromium.ui.util.ColorUtils;

/**
 * Concrete implementation of ColorProviderBridge resolving theme colors directly using R.attr
 * compile-time constants to avoid reflection.
 */
@NullMarked
public class ColorProviderBridgeImpl implements ColorProviderBridge {

    private static final int[] COLOR_ATTRS = new int[AndroidColorRole.MAX_VALUE + 1];

    // LINT.IfChange(AndroidColorRoleAttrs)
    static {
        COLOR_ATTRS[AndroidColorRole.PRIMARY] = R.attr.colorPrimary;
        COLOR_ATTRS[AndroidColorRole.ON_PRIMARY] = R.attr.colorOnPrimary;
        COLOR_ATTRS[AndroidColorRole.PRIMARY_CONTAINER] = R.attr.colorPrimaryContainer;
        COLOR_ATTRS[AndroidColorRole.ON_PRIMARY_CONTAINER] = R.attr.colorOnPrimaryContainer;
        COLOR_ATTRS[AndroidColorRole.SECONDARY] = R.attr.colorSecondary;
        COLOR_ATTRS[AndroidColorRole.ON_SECONDARY] = R.attr.colorOnSecondary;
        COLOR_ATTRS[AndroidColorRole.SECONDARY_CONTAINER] = R.attr.colorSecondaryContainer;
        COLOR_ATTRS[AndroidColorRole.ON_SECONDARY_CONTAINER] = R.attr.colorOnSecondaryContainer;
        COLOR_ATTRS[AndroidColorRole.TERTIARY] = R.attr.colorTertiary;
        COLOR_ATTRS[AndroidColorRole.ON_TERTIARY] = R.attr.colorOnTertiary;
        COLOR_ATTRS[AndroidColorRole.TERTIARY_CONTAINER] = R.attr.colorTertiaryContainer;
        COLOR_ATTRS[AndroidColorRole.ON_TERTIARY_CONTAINER] = R.attr.colorOnTertiaryContainer;
        COLOR_ATTRS[AndroidColorRole.BACKGROUND] = android.R.attr.colorBackground;
        COLOR_ATTRS[AndroidColorRole.ON_BACKGROUND] = R.attr.colorOnBackground;
        COLOR_ATTRS[AndroidColorRole.SURFACE] = R.attr.colorSurface;
        COLOR_ATTRS[AndroidColorRole.ON_SURFACE] = R.attr.colorOnSurface;
        COLOR_ATTRS[AndroidColorRole.SURFACE_VARIANT] = R.attr.colorSurfaceVariant;
        COLOR_ATTRS[AndroidColorRole.ON_SURFACE_VARIANT] = R.attr.colorOnSurfaceVariant;
        COLOR_ATTRS[AndroidColorRole.OUTLINE] = R.attr.colorOutline;
        COLOR_ATTRS[AndroidColorRole.OUTLINE_VARIANT] = R.attr.colorOutlineVariant;
        COLOR_ATTRS[AndroidColorRole.ERROR] = R.attr.colorError;
        COLOR_ATTRS[AndroidColorRole.ON_ERROR] = R.attr.colorOnError;
        COLOR_ATTRS[AndroidColorRole.ERROR_CONTAINER] = R.attr.colorErrorContainer;
        COLOR_ATTRS[AndroidColorRole.ON_ERROR_CONTAINER] = R.attr.colorOnErrorContainer;
        COLOR_ATTRS[AndroidColorRole.INVERSE_SURFACE] = R.attr.colorSurfaceInverse;
        COLOR_ATTRS[AndroidColorRole.INVERSE_ON_SURFACE] = R.attr.colorOnSurfaceInverse;
        COLOR_ATTRS[AndroidColorRole.INVERSE_PRIMARY] = R.attr.colorPrimaryInverse;
    }

    // LINT.ThenChange(//ui/color/android/android_color_roles.h:AndroidColorRole)

    @Override
    public long[] getThemeColors(Context context) {
        if (context == null) {
            return new long[0];
        }

        int numColors = AndroidColorRole.MAX_VALUE + 1;
        long[] colors = new long[numColors];
        TypedValue value = new TypedValue();

        for (int i = 0; i < numColors; i++) {
            colors[i] = resolveColor(context, COLOR_ATTRS[i], value);
        }
        return colors;
    }

    private static long resolveColor(Context context, int attrId, TypedValue value) {
        if (attrId == 0) {
            return ColorUtils.INVALID_COLOR;
        }

        if (context.getTheme().resolveAttribute(attrId, value, true)) {
            if (value.type >= TypedValue.TYPE_FIRST_COLOR_INT
                    && value.type <= TypedValue.TYPE_LAST_COLOR_INT) {
                return (long) value.data;
            } else if (value.resourceId != 0) {
                return (long) context.getColor(value.resourceId);
            }
        }
        return ColorUtils.INVALID_COLOR;
    }
}
