// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.res.Resources;
import android.view.View;

import androidx.annotation.ColorRes;
import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.ViewVisibility;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/** Represents Glic/Actor's visual state of the Peek View UI. */
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

    private static final @DrawableRes int BUTTON_ICON_ACTING = R.drawable.ic_pause_white_24dp;
    private static final @DrawableRes int BUTTON_ICON_PAUSED = R.drawable.ic_play_arrow_white_24dp;
    private static final @DrawableRes int BUTTON_ICON_WAITING = Resources.ID_NULL;
    private static final @DrawableRes int BUTTON_ICON_DEFAULT = Resources.ID_NULL;

    public final @StateType int type;

    // The status of the actor task based on its current state (acting, paused, or waiting)
    public final @StringRes int descriptionResId;

    // UI properties for the actor control button
    public final @DrawableRes int buttonIconResId;
    public final @DimenRes int buttonHorizontalPaddingResId;
    public final @ColorRes int iconTintResId;
    public final @ViewVisibility int buttonVisibility;
    public final @StringRes int buttonTextResId;
    public final @ViewVisibility int descriptionVisibility;
    public final @ColorRes int buttonBackgroundResId;
    public final @StringRes int buttonContentDescriptionResId;

    private PeekViewUiState(
            @StateType int type,
            @StringRes int descriptionResId,
            @DrawableRes int buttonIconResId,
            @ColorRes int buttonBackgroundResId,
            @DimenRes int buttonHorizontalPaddingResId,
            @ColorRes int iconTintResId,
            @ViewVisibility int buttonVisibility,
            @StringRes int buttonTextResId,
            @ViewVisibility int descriptionVisibility,
            @StringRes int buttonContentDescriptionResId) {
        this.type = type;
        this.descriptionResId = descriptionResId;
        this.buttonIconResId = buttonIconResId;
        this.buttonBackgroundResId = buttonBackgroundResId;
        this.buttonHorizontalPaddingResId = buttonHorizontalPaddingResId;
        this.iconTintResId = iconTintResId;
        this.buttonVisibility = buttonVisibility;
        this.buttonTextResId = buttonTextResId;
        this.descriptionVisibility = descriptionVisibility;
        this.buttonContentDescriptionResId = buttonContentDescriptionResId;
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
                && buttonContentDescriptionResId == that.buttonContentDescriptionResId;
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
                buttonContentDescriptionResId);
    }

    // Static instances for each state
    public static final PeekViewUiState ACTING =
            new PeekViewUiState(
                    /* type= */ StateType.ACTING,
                    /* descriptionResId= */ R.string.peek_state_acting,
                    /* buttonIconResId= */ BUTTON_ICON_ACTING,
                    /* buttonBackgroundResId= */ R.color.actor_control_icon_button_background_color,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ R.color.default_icon_color_tint_list,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ Resources.ID_NULL,
                    /* descriptionVisibility= */ View.VISIBLE,
                    /* buttonContentDescriptionResId= */ R.string
                            .peek_state_pause_button_a11y_label);

    public static final PeekViewUiState PAUSED =
            new PeekViewUiState(
                    /* type= */ StateType.PAUSED,
                    /* descriptionResId= */ R.string.peek_state_paused,
                    /* buttonIconResId= */ BUTTON_ICON_PAUSED,
                    /* buttonBackgroundResId= */ R.color.actor_control_icon_button_background_color,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ R.color.default_icon_color_tint_list,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ Resources.ID_NULL,
                    /* descriptionVisibility= */ View.VISIBLE,
                    /* buttonContentDescriptionResId= */ R.string
                            .peek_state_play_button_a11y_label);

    public static final PeekViewUiState WAITING =
            new PeekViewUiState(
                    /* type= */ StateType.WAITING,
                    /* descriptionResId= */ R.string.peek_state_waiting,
                    /* buttonIconResId= */ BUTTON_ICON_WAITING,
                    /* buttonBackgroundResId= */ R.color.actor_view_button_background_color,
                    /* buttonHorizontalPaddingResId= */ R.dimen
                            .actor_control_view_button_horizontal_padding,
                    /* iconTintResId= */ Resources.ID_NULL,
                    /* buttonVisibility= */ View.VISIBLE,
                    /* buttonTextResId= */ R.string.peek_state_view_button_label,
                    /* descriptionVisibility= */ View.VISIBLE,
                    /* buttonContentDescriptionResId= */ Resources.ID_NULL);

    public static final PeekViewUiState DEFAULT =
            new PeekViewUiState(
                    /* type= */ StateType.DEFAULT,
                    /* descriptionResId= */ Resources.ID_NULL,
                    /* buttonIconResId= */ BUTTON_ICON_DEFAULT,
                    /* buttonBackgroundResId= */ Resources.ID_NULL,
                    /* buttonHorizontalPaddingResId= */ Resources.ID_NULL,
                    /* iconTintResId= */ Resources.ID_NULL,
                    /* buttonVisibility= */ View.GONE,
                    /* buttonTextResId= */ Resources.ID_NULL,
                    /* descriptionVisibility= */ View.GONE,
                    /* buttonContentDescriptionResId= */ Resources.ID_NULL);
}
