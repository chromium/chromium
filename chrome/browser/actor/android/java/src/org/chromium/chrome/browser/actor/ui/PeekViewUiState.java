// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.StringRes;

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

    private static final @DrawableRes int ICON_ACTING = R.drawable.ic_pause_white_24dp;
    private static final @DrawableRes int ICON_PAUSED = R.drawable.ic_play_arrow_white_24dp;
    private static final @DrawableRes int ICON_WAITING = R.drawable.material_ic_close_24dp;
    private static final @DrawableRes int ICON_DEFAULT = 0;

    // Description of the PeekView state. This is not tied to a specific task.
    public final @StringRes int descriptionResId;

    public final @DrawableRes int iconResId;
    public final @StateType int type;

    private PeekViewUiState(
            @StateType int type, @StringRes int descriptionResId, @DrawableRes int iconResId) {
        this.type = type;
        this.descriptionResId = descriptionResId;
        this.iconResId = iconResId;
    }

    public String getDescription(Context context) {
        if (descriptionResId != -1) {
            return context.getString(descriptionResId);
        }
        return "";
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o instanceof PeekViewUiState that) {
            return type == that.type
                    && descriptionResId == that.descriptionResId
                    && iconResId == that.iconResId;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(type, descriptionResId, iconResId);
    }

    // Static instances for each state
    public static final PeekViewUiState ACTING =
            new PeekViewUiState(StateType.ACTING, R.string.peek_state_acting, ICON_ACTING);

    public static final PeekViewUiState PAUSED =
            new PeekViewUiState(StateType.PAUSED, R.string.peek_state_paused, ICON_PAUSED);

    public static final PeekViewUiState WAITING =
            new PeekViewUiState(StateType.WAITING, R.string.peek_state_waiting, ICON_WAITING);

    public static final PeekViewUiState DEFAULT =
            new PeekViewUiState(StateType.DEFAULT, -1, ICON_DEFAULT);
}
