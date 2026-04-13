// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.android.material.color.MaterialColors;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/** Represents the visual state of the Actor Control UI. */
@NullMarked
public class PeekViewUiState {

    @IntDef({StateType.ACTING, StateType.PAUSED, StateType.WAITING, StateType.DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    @interface StateType {
        int ACTING = 0;
        int PAUSED = 1;
        int WAITING = 2;
        int DEFAULT = 3;
    }

    @IntDef({View.VISIBLE, View.INVISIBLE, View.GONE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Visibility {}

    private static final @DrawableRes int ICON_ACTING = R.drawable.ic_pause_white_24dp;
    private static final @DrawableRes int ICON_PAUSED = R.drawable.ic_play_arrow_white_24dp;
    private static final @DrawableRes int ICON_WAITING = Resources.ID_NULL;
    private static final @DrawableRes int ICON_DEFAULT = Resources.ID_NULL;

    public final @StateType int type;

    // The status of the actor task based on its current state (acting, paused, or waiting)
    public final @StringRes int descriptionResId;

    // UI properties for the actor control button
    public final @DrawableRes int buttonIconResId;
    public final @AttrRes int buttonBackgroundAttr;
    public final @DimenRes int buttonCornerRadiusResId;
    public final @DimenRes int buttonHorizontalPaddingResId;
    public final @ColorRes int iconTintResId;
    public final @Visibility int buttonVisibility;
    public final @StringRes int buttonTextResId;

    private PeekViewUiState(
            @StateType int type,
            @StringRes int descriptionResId,
            @DrawableRes int buttonIconResId,
            @AttrRes int buttonBackgroundAttr,
            @DimenRes int buttonCornerRadiusResId,
            @DimenRes int buttonHorizontalPaddingResId,
            @ColorRes int iconTintResId,
            @Visibility int buttonVisibility,
            @StringRes int buttonTextResId) {
        this.type = type;
        this.descriptionResId = descriptionResId;
        this.buttonIconResId = buttonIconResId;
        this.buttonBackgroundAttr = buttonBackgroundAttr;
        this.buttonCornerRadiusResId = buttonCornerRadiusResId;
        this.buttonHorizontalPaddingResId = buttonHorizontalPaddingResId;
        this.iconTintResId = iconTintResId;
        this.buttonVisibility = buttonVisibility;
        this.buttonTextResId = buttonTextResId;
    }

    /**
     * Returns the background color tint list for the actor control button.
     *
     * @param context The {@link Context} to use for retrieving resources.
     * @return The background color tint list for the actor control button.
     */
    public ColorStateList getButtonBackgroundTint(Context context) {
        return ColorStateList.valueOf(
                MaterialColors.getColor(context, buttonBackgroundAttr, /* defaultValue= */ 0));
    }

    /**
     * Returns the corner radius for the actor control button.
     *
     * @param context The {@link Context} to use for retrieving resources.
     * @return The corner radius for the actor control button.
     */
    public int getButtonCornerRadius(Context context) {
        return (buttonCornerRadiusResId != Resources.ID_NULL)
                ? context.getResources().getDimensionPixelSize(buttonCornerRadiusResId)
                : 0;
    }

    /**
     * Returns the horizontal padding for the actor control button.
     *
     * @param context The {@link Context} to use for retrieving resources.
     * @return The horizontal padding for the actor control button in pixels.
     */
    public int getButtonHorizontalPadding(Context context) {
        return (buttonHorizontalPaddingResId != Resources.ID_NULL)
                ? context.getResources().getDimensionPixelSize(buttonHorizontalPaddingResId)
                : 0;
    }

    /**
     * Returns the description for the current state of the actor task.
     *
     * @param context The {@link Context} to use for retrieving resources.
     * @return The description for the current state of the actor task.
     */
    public String getDescription(Context context) {
        return (descriptionResId != Resources.ID_NULL) ? context.getString(descriptionResId) : "";
    }

    /**
     * Returns the text displayed on the actor control button.
     *
     * @param context The {@link Context} to use for retrieving resources.
     * @return The text displayed on the actor control button.
     */
    public @Nullable String getButtonText(Context context) {
        return (buttonTextResId != Resources.ID_NULL) ? context.getString(buttonTextResId) : null;
    }

    /**
     * Returns the visibility of the actor control button.
     *
     * @return The visibility of the actor control button.
     */
    public @Visibility int getButtonVisibility() {
        return buttonVisibility;
    }

    /**
     * Returns the icon tint for the actor control button.
     *
     * @param context The {@link Context} to use for retrieving resources.
     * @return The icon tint for the actor control button.
     */
    public @Nullable ColorStateList getIconTint(Context context) {
        return (iconTintResId != Resources.ID_NULL)
                ? AppCompatResources.getColorStateList(context, iconTintResId)
                : null;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (!(o instanceof PeekViewUiState)) return false;
        PeekViewUiState that = (PeekViewUiState) o;
        return type == that.type
                && descriptionResId == that.descriptionResId
                && buttonIconResId == that.buttonIconResId
                && buttonBackgroundAttr == that.buttonBackgroundAttr
                && buttonCornerRadiusResId == that.buttonCornerRadiusResId
                && buttonHorizontalPaddingResId == that.buttonHorizontalPaddingResId
                && iconTintResId == that.iconTintResId
                && buttonVisibility == that.buttonVisibility
                && buttonTextResId == that.buttonTextResId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                type,
                descriptionResId,
                buttonIconResId,
                buttonBackgroundAttr,
                buttonCornerRadiusResId,
                buttonHorizontalPaddingResId,
                iconTintResId,
                buttonVisibility,
                buttonTextResId);
    }

    // Static instances for each state
    public static final PeekViewUiState ACTING =
            new PeekViewUiState(
                    /* type= */ StateType.ACTING,
                    /* descriptionResId= */ R.string.peek_state_acting,
                    /* buttonIconResId= */ ICON_ACTING,
                    /* buttonBackgroundAttr= */ R.attr.colorSurfaceContainer,
                    /* buttonCornerRadiusResId= */ R.dimen
                            .actor_control_button_corner_radius_play_pause,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ R.color.default_icon_color_tint_list,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ Resources.ID_NULL);

    public static final PeekViewUiState PAUSED =
            new PeekViewUiState(
                    /* type= */ StateType.PAUSED,
                    /* descriptionResId= */ R.string.peek_state_paused,
                    /* buttonIconResId= */ ICON_PAUSED,
                    /* buttonBackgroundAttr= */ R.attr.colorSurfaceContainerLow,
                    /* buttonCornerRadiusResId= */ R.dimen
                            .actor_control_button_corner_radius_play_pause,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ R.color.default_icon_color_tint_list,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ Resources.ID_NULL);

    public static final PeekViewUiState WAITING =
            new PeekViewUiState(
                    /* type= */ StateType.WAITING,
                    /* descriptionResId= */ R.string.peek_state_waiting,
                    /* buttonIconResId= */ ICON_WAITING,
                    /* buttonBackgroundAttr= */ R.attr.colorPrimary,
                    /* buttonCornerRadiusResId= */ R.dimen
                            .actor_control_button_corner_radius_waiting,
                    /* buttonHorizontalPaddingResId= */ R.dimen
                            .actor_control_button_horizontal_padding_waiting,
                    /* iconTintResId= */ Resources.ID_NULL,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ R.string.peek_state_view_button_label);

    public static final PeekViewUiState DEFAULT =
            new PeekViewUiState(
                    /* type= */ StateType.DEFAULT,
                    /* descriptionResId= */ Resources.ID_NULL,
                    /* buttonIconResId= */ ICON_DEFAULT,
                    /* buttonBackgroundAttr= */ Resources.ID_NULL,
                    /* buttonCornerRadiusResId= */ Resources.ID_NULL,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ Resources.ID_NULL,
                    /* buttonVisibility= */ View.GONE,
                    /* buttonTextResId= */ Resources.ID_NULL);
}
