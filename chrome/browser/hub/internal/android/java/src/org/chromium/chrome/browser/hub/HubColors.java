// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.ValueUtils;
import org.chromium.ui.util.XrUtils;

/** Util class to handle various color operations shared between hub classes. */
public final class HubColors {
    private static final String TAG = "HubColors";
    private static final int[][] SELECTED_AND_NORMAL_STATES =
            new int[][] {new int[] {android.R.attr.state_selected}, new int[] {}};
    private static final int[][] DISABLED_AND_NORMAL_STATES =
            new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}};

    private HubColors() {}

    /** Returns the color scheme from a pane with a fallback for null. */
    public static @HubColorScheme int getColorSchemeSafe(@Nullable Pane pane) {
        return pane == null ? HubColorScheme.DEFAULT : pane.getColorScheme();
    }

    /** Returns the background color generic surfaces should use per the given color scheme. */
    public static @ColorInt int getBackgroundColor(
            Context context, @HubColorScheme int colorScheme) {
        // On an XRDevice in FSM the background color of the Hub view is set to transparent always.
        if (XrUtils.getInstance().isFsmOnXrDevice()) return Color.TRANSPARENT;

        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getDefaultBgColor(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.default_bg_color_dark);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color most icons should use per the given color scheme. */
    public static @ColorInt int getIconColor(Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getDefaultIconColor(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.default_icon_color_light);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color selected icons should use per the given color scheme. */
    public static @ColorInt int getSelectedIconColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getDefaultIconColorAccent1(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.default_control_color_active_dark);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Convenience method to make a selectable {@link ColorStateList} from two input colors. */
    public static ColorStateList getSelectableIconList(
            @ColorInt int selectedColor, @ColorInt int normalColor) {
        int[] colors = new int[] {selectedColor, normalColor};
        return new ColorStateList(SELECTED_AND_NORMAL_STATES, colors);
    }

    /** Returns the color of the hairline for a color scheme. */
    public static @ColorInt int getHairlineColor(Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getDividerLineBgColor(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.divider_line_bg_color_light);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color of the search box hint text. */
    public static @ColorInt int getSearchBoxHintTextColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return MaterialColors.getColor(context, R.attr.colorOnSurfaceVariant, TAG);
            case HubColorScheme.INCOGNITO:
                return context.getColor(R.color.default_text_color_secondary_light);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color of the background for the search box. */
    public static @ColorInt int getSearchBoxBgColor(
            Context context, @HubColorScheme int colorScheme) {
        @ColorRes int backgroundColorRes;
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                backgroundColorRes = R.color.color_primary_with_alpha_10;
                break;
            case HubColorScheme.INCOGNITO:
                backgroundColorRes = R.color.baseline_neutral_20;
                break;
            default:
                assert false;
                backgroundColorRes = Resources.ID_NULL;
        }
        return ContextCompat.getColor(context, backgroundColorRes);
    }

    public static ColorStateList getActionButtonColor(Context context, @ColorInt int color) {
        @DimenRes int disabledAlpha = R.dimen.default_disabled_alpha;
        return generateDisabledAndNormalStatesColorStateList(context, color, disabledAlpha);
    }

    @NonNull
    private static ColorStateList generateDisabledAndNormalStatesColorStateList(
            Context context, int color, int disabledAlpha) {
        Resources resources = context.getResources();
        float alpha = ValueUtils.getFloat(resources, disabledAlpha);
        int alphaScaled = Math.round(alpha * 255);
        int[] colors = new int[] {ColorUtils.setAlphaComponent(color, alphaScaled), color};
        return new ColorStateList(DISABLED_AND_NORMAL_STATES, colors);
    }
}
