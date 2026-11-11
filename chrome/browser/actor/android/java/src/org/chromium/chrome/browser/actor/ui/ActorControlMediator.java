// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.content.res.Resources;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties.ContentDescriptionCallback;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for actor control view. */
@NullMarked
public class ActorControlMediator {
    private final PropertyModel mModel;

    ActorControlMediator(PropertyModel model) {
        mModel = model;
    }

    /**
     * Sets the content and state of the actor control view.
     *
     * @param title The title of the actor control view.
     * @param state The PeekViewUiState containing the desired UI properties.
     */
    void setContent(String title, PeekViewUiState state) {
        setContent(title, state, /* stepProgress= */ null);
    }

    /**
     * Sets the content and state of the actor control view, with optional custom step progress
     * text.
     *
     * @param title The title of the actor control view.
     * @param state The PeekViewUiState containing the desired UI properties.
     * @param stepProgress Optional step progress description when the task is running.
     */
    void setContent(String title, PeekViewUiState state, @Nullable String stepProgress) {
        String customDescription = !TextUtils.isEmpty(stepProgress) ? stepProgress : null;
        mModel.set(TabBottomSheetPeekProperties.PEEK_ICON_ID, state.peekIconResId);
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, title);
        mModel.set(
                TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                state.getTitleTextAppearanceResId());
        mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID, state.descriptionResId);
        mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_TEXT, customDescription);
        mModel.set(
                TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY,
                state.getDescriptionVisibility());
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT_ID, state.buttonTextResId);
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, state.getButtonVisibility());
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_ID, state.buttonIconResId);
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT_ID,
                state.buttonBackgroundResId);
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_TINT_ID, state.iconTintResId);
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_HORIZONTAL_PADDING_ID,
                state.buttonHorizontalPaddingResId);
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_CONTENT_DESCRIPTION_ID,
                state.buttonContentDescriptionResId);
        mModel.set(
                TabBottomSheetPeekProperties.CONTENT_DESCRIPTION_A11Y,
                buildContentDescriptionCallback(title, state, customDescription));
    }

    @Nullable ContentDescriptionCallback buildContentDescriptionCallback(
            String title, PeekViewUiState state, @Nullable String customDescription) {
        return (PeekViewUiState.DEFAULT.equals(state) && TextUtils.isEmpty(title))
                ? null
                : context -> calculateContentDescription(context, title, state, customDescription);
    }

    @VisibleForTesting
    static @Nullable String calculateContentDescription(
            Context context,
            String title,
            PeekViewUiState state,
            @Nullable String customDescription) {
        StringBuilder sb = new StringBuilder();
        if (!TextUtils.isEmpty(title)) {
            sb.append(title.trim());
        }
        String desc;
        if (!TextUtils.isEmpty(customDescription)) {
            desc = customDescription;
        } else {
            desc =
                    state.descriptionResId != Resources.ID_NULL
                            ? context.getString(state.descriptionResId)
                            : "";
        }
        if (state.getDescriptionVisibility() == View.VISIBLE && !TextUtils.isEmpty(desc)) {
            if (sb.length() > 0) {
                sb.append(", ");
            }
            sb.append(desc.trim());
        }
        String rawText = sb.toString();
        if (TextUtils.isEmpty(rawText)) {
            return null;
        }
        return context.getString(R.string.peek_state_accessible_label, rawText);
    }
}
