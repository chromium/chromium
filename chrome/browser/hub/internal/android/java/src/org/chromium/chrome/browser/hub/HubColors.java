// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.DimenRes;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.theme.SurfaceColorUpdateUtils;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;
import org.chromium.ui.util.ValueUtils;
import org.chromium.ui.util.XrUtils;

/** Util class to handle various color operations shared between hub classes. */
@NullMarked
public final class HubColors {
    private static final String TAG = "HubColors";
    private static final int[][] SELECTED_AND_NORMAL_STATES =
            new int[][] {new int[] {android.R.attr.state_selected}, new int[] {}};
    private static final int[][] DISABLED_AND_NORMAL_STATES =
            new int[][] {new int[] {-android.R.attr.state_enabled}, new int[] {}};
    private static final int[][] HOVERED_STATE =
            new int[][] {new int[] {android.R.attr.state_hovered}};

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
                return SurfaceColorUpdateUtils.getGridTabSwitcherBackgroundColor(
                        context, /* isIncognito= */ false);
            case HubColorScheme.INCOGNITO:
                return SurfaceColorUpdateUtils.getGridTabSwitcherBackgroundColor(
                        context, /* isIncognito= */ true);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color toolbar action button uses per the given color scheme. */
    public static @ColorInt int getToolbarActionButtonIconColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getDefaultIconColorOnAccent1(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.default_icon_color_on_accent1_dark);
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
            Context context, @HubColorScheme int colorScheme, boolean isGtsUpdateEnabled) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SurfaceColorUpdateUtils.getHubPaneSwitcherSelectedIconColor(
                        context, /* isIncognito= */ false, isGtsUpdateEnabled);
            case HubColorScheme.INCOGNITO:
                return SurfaceColorUpdateUtils.getHubPaneSwitcherSelectedIconColor(
                        context, /* isIncognito= */ true, isGtsUpdateEnabled);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the color selected tab item selector should use per the given color scheme. */
    public static @ColorInt int geTabItemSelectorColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SurfaceColorUpdateUtils.geTabItemSelectorColor(
                        context, /* isIncognito= */ false);
            case HubColorScheme.INCOGNITO:
                return SurfaceColorUpdateUtils.geTabItemSelectorColor(
                        context, /* isIncognito= */ true);
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
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SurfaceColorUpdateUtils.getGtsSearchBoxBackgroundColor(
                        context, /* isIncognito= */ false);
            case HubColorScheme.INCOGNITO:
                return SurfaceColorUpdateUtils.getGtsSearchBoxBackgroundColor(
                        context, /* isIncognito= */ true);
            default:
                assert false;
                return ContextCompat.getColor(context, Resources.ID_NULL);
        }
    }

    /** Returns the hub tool bar action button background color as per the given color scheme. */
    public static @ColorInt int getToolbarActionButtonBackgroundColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SemanticColorUtils.getFilledButtonBgColor(context);
            case HubColorScheme.INCOGNITO:
                return ContextCompat.getColor(context, R.color.filled_button_bg_color_light);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the hub pane switcher background color as per the given color scheme. */
    public static @ColorInt int getPaneSwitcherBackgroundColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT:
                return SurfaceColorUpdateUtils.getPaneSwitcherBackgroundColor(
                        context, /* isIncognito= */ false);
            case HubColorScheme.INCOGNITO:
                return SurfaceColorUpdateUtils.getPaneSwitcherBackgroundColor(
                        context, /* isIncognito= */ true);
            default:
                assert false;
                return Color.TRANSPARENT;
        }
    }

    /** Returns the hub pane switcher tab item hover color as per the given color scheme. */
    public static @ColorInt int getPaneSwitcherTabItemHoverColor(
            Context context, @HubColorScheme int colorScheme) {
        switch (colorScheme) {
            case HubColorScheme.DEFAULT -> {
                return SemanticColorUtils.getColorOnSurface(context);
            }
            case HubColorScheme.INCOGNITO -> {
                return ContextCompat.getColor(
                        context, R.color.pane_switcher_tab_item_hover_incognito);
            }
            default -> {
                assert false;
                return Color.TRANSPARENT;
            }
        }
    }

    public static ColorStateList getActionButtonColor(Context context, @ColorInt int color) {
        @DimenRes int disabledAlpha = R.dimen.default_disabled_alpha;
        return generateDisabledAndNormalStatesColorStateList(context, color, disabledAlpha);
    }

    /**
     * Generates a {@link ColorStateList} with a specific color applied when the view is in a
     * hovered state.
     */
    public static ColorStateList generateHoveredStateColorStateList(Context context, int color) {
        @DimenRes int hoveredAlpha = R.dimen.hub_pane_switcher_tab_item_hover_alpha;
        int hoveredColor = getColorWithAlphaApplied(context, color, hoveredAlpha);
        return new ColorStateList(HOVERED_STATE, new int[] {hoveredColor});
    }

    private static ColorStateList generateDisabledAndNormalStatesColorStateList(
            Context context, int color, int disabledAlpha) {
        int[] colors = new int[] {getColorWithAlphaApplied(context, color, disabledAlpha), color};
        return new ColorStateList(DISABLED_AND_NORMAL_STATES, colors);
    }

    private static @ColorInt int getColorWithAlphaApplied(
            Context context, int color, @DimenRes int alphaRes) {
        Resources resources = context.getResources();
        float alpha = ValueUtils.getFloat(resources, alphaRes);
        int alphaScaled = Math.round(alpha * 255);

        return ColorUtils.setAlphaComponent(color, alphaScaled);
    }
}
