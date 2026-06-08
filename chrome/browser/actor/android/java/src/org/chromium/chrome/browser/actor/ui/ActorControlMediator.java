// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator for actor control view. */
@NullMarked
public class ActorControlMediator {
    private final Context mContext;
    private final PropertyModel mModel;

    ActorControlMediator(Context context, PropertyModel model) {
        mContext = context;
        mModel = model;
    }

    /**
     * Sets the content and state of the actor control view.
     *
     * @param title The title of the actor control view.
     * @param state The PeekViewUiState containing the desired UI properties.
     */
    void setContent(String title, PeekViewUiState state) {
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, title);
        mModel.set(
                TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE,
                state.getTitleTextAppearanceResId());
        mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_TEXT, state.getDescription(mContext));
        mModel.set(
                TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY,
                state.getDescriptionVisibility());
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT, state.getButtonText(mContext));
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY, state.getButtonVisibility());
        mModel.set(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON, state.buttonIconResId);
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT,
                state.getButtonBackgroundTint(mContext));
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_TINT, state.getIconTint(mContext));
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_HORIZONTAL_PADDING,
                state.getButtonHorizontalPadding(mContext));
        mModel.set(
                TabBottomSheetPeekProperties.ACTION_BUTTON_CONTENT_DESCRIPTION,
                state.getButtonContentDescription(mContext));
    }
}
