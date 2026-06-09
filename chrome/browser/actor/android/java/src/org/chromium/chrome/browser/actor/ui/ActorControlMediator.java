// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetPeekProperties;
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
        mModel.set(TabBottomSheetPeekProperties.TITLE_TEXT, title);
        mModel.set(
                TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE_ID,
                state.getTitleTextAppearanceResId());
        mModel.set(TabBottomSheetPeekProperties.DESCRIPTION_TEXT_ID, state.descriptionResId);
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
    }
}
