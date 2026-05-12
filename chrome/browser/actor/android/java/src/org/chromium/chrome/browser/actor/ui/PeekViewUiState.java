// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;
import androidx.annotation.StyleRes;
import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

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
    public final @DimenRes int buttonHorizontalPaddingResId;
    public final @ColorRes int iconTintResId;
    public final @Visibility int buttonVisibility;
    public final @StringRes int buttonTextResId;
    public final @Visibility int descriptionVisibility;
    public final @StyleRes int titleTextAppearanceResId;
    public final @ColorRes int buttonBackgroundResId;

    private PeekViewUiState(
            @StateType int type,
            @StringRes int descriptionResId,
            @DrawableRes int buttonIconResId,
            @ColorRes int buttonBackgroundResId,
            @DimenRes int buttonHorizontalPaddingResId,
            @ColorRes int iconTintResId,
            @Visibility int buttonVisibility,
            @StringRes int buttonTextResId,
            @Visibility int descriptionVisibility,
            @StyleRes int titleTextAppearanceResId) {
        this.type = type;
        this.descriptionResId = descriptionResId;
        this.buttonIconResId = buttonIconResId;
        this.buttonBackgroundResId = buttonBackgroundResId;
        this.buttonHorizontalPaddingResId = buttonHorizontalPaddingResId;
        this.iconTintResId = iconTintResId;
        this.buttonVisibility = buttonVisibility;
        this.buttonTextResId = buttonTextResId;
        this.descriptionVisibility = descriptionVisibility;
        this.titleTextAppearanceResId = titleTextAppearanceResId;
    }

    /**
     * Returns the background color tint list for the actor control button.
     *
     * @param context The {@link Context} to use for retrieving resources.
     * @return The background color tint list for the actor control button.
     */
    public @Nullable ColorStateList getButtonBackgroundTint(Context context) {
        if (buttonBackgroundResId == Resources.ID_NULL) {
            return null;
        }
        return ColorStateList.valueOf(ContextCompat.getColor(context, buttonBackgroundResId));
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
     * Returns the visibility of the description view.
     *
     * @return The visibility of the description view.
     */
    public @Visibility int getDescriptionVisibility() {
        return descriptionVisibility;
    }

    /**
     * Returns the text appearance resource id for the title view.
     *
     * @return The text appearance resource id for the title view.
     */
    public @StyleRes int getTitleTextAppearanceResId() {
        return titleTextAppearanceResId;
    }

    /**
     * Returns the icon tint for the actor control button.
     *
     * @param context The {@link Context} to use for retrieving resources.
     * @return The icon tint for the actor control button.
     */
    public @Nullable ColorStateList getIconTint(Context context) {
        return (iconTintResId != Resources.ID_NULL)
                ? context.getColorStateList(iconTintResId)
                : null;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof PeekViewUiState)) {
            return false;
        }
        PeekViewUiState that = (PeekViewUiState) o;
        return type == that.type
                && descriptionResId == that.descriptionResId
                && buttonIconResId == that.buttonIconResId
                && buttonBackgroundResId == that.buttonBackgroundResId
                && buttonHorizontalPaddingResId == that.buttonHorizontalPaddingResId
                && iconTintResId == that.iconTintResId
                && buttonVisibility == that.buttonVisibility
                && buttonTextResId == that.buttonTextResId
                && descriptionVisibility == that.descriptionVisibility
                && titleTextAppearanceResId == that.titleTextAppearanceResId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                type,
                descriptionResId,
                buttonIconResId,
                buttonBackgroundResId,
                buttonHorizontalPaddingResId,
                iconTintResId,
                buttonVisibility,
                buttonTextResId,
                descriptionVisibility,
                titleTextAppearanceResId);
    }

    // Static instances for each state
    public static final PeekViewUiState ACTING =
            new PeekViewUiState(
                    /* type= */ StateType.ACTING,
                    /* descriptionResId= */ R.string.peek_state_acting,
                    /* buttonIconResId= */ ICON_ACTING,
                    /* buttonBackgroundResId= */ R.color.actor_control_icon_button_background_color,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ R.color.default_icon_color_tint_list,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ Resources.ID_NULL,
                    /* descriptionVisibility= */ View.VISIBLE,
                    /* titleTextAppearanceResId= */ R.style.TextAppearance_TextMediumThick_Primary);

    public static final PeekViewUiState PAUSED =
            new PeekViewUiState(
                    /* type= */ StateType.PAUSED,
                    /* descriptionResId= */ R.string.peek_state_paused,
                    /* buttonIconResId= */ ICON_PAUSED,
                    /* buttonBackgroundResId= */ R.color.actor_control_icon_button_background_color,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ R.color.default_icon_color_tint_list,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ Resources.ID_NULL,
                    /* descriptionVisibility= */ View.VISIBLE,
                    /* titleTextAppearanceResId= */ R.style.TextAppearance_TextMediumThick_Primary);

    public static final PeekViewUiState WAITING =
            new PeekViewUiState(
                    /* type= */ StateType.WAITING,
                    /* descriptionResId= */ R.string.peek_state_waiting,
                    /* buttonIconResId= */ ICON_WAITING,
                    /* buttonBackgroundResId= */ R.color.actor_view_button_background_color,
                    /* buttonHorizontalPaddingResId= */ R.dimen
                            .actor_control_view_button_horizontal_padding,
                    /* iconTintResId= */ Resources.ID_NULL,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ R.string.peek_state_view_button_label,
                    /* descriptionVisibility= */ View.VISIBLE,
                    /* titleTextAppearanceResId= */ R.style.TextAppearance_TextMediumThick_Primary);

    public static final PeekViewUiState DEFAULT =
            new PeekViewUiState(
                    /* type= */ StateType.DEFAULT,
                    /* descriptionResId= */ Resources.ID_NULL,
                    /* buttonIconResId= */ ICON_DEFAULT,
                    /* buttonBackgroundResId= */ Resources.ID_NULL,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ Resources.ID_NULL,
                    /* buttonVisibility= */ View.GONE,
                    /* buttonTextResId= */ Resources.ID_NULL,
                    /* descriptionVisibility= */ View.GONE,
                    /* titleTextAppearanceResId= */ R.style.TextAppearance_Headline2Thick);
}
