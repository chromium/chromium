// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for TabBottomSheetPeekView. */
@NullMarked
class TabBottomSheetPeekViewBinder {

    /**
     * This method binds the given model to the given view.
     *
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update.
     */
    public static void bind(
            PropertyModel model, TabBottomSheetPeekView view, PropertyKey propertyKey) {
        if (TabBottomSheetPeekProperties.TITLE_TEXT == propertyKey) {
            view.setTitle(model.get(TabBottomSheetPeekProperties.TITLE_TEXT));
        } else if (TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE == propertyKey) {
            view.setTitleTextAppearance(
                    model.get(TabBottomSheetPeekProperties.TITLE_TEXT_APPEARANCE));
        } else if (TabBottomSheetPeekProperties.DESCRIPTION_TEXT == propertyKey) {
            view.setDescriptionText(model.get(TabBottomSheetPeekProperties.DESCRIPTION_TEXT));
        } else if (TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY == propertyKey) {
            view.setDescriptionVisibility(
                    model.get(TabBottomSheetPeekProperties.DESCRIPTION_VISIBILITY));
        } else if (TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT == propertyKey) {
            view.setActionButtonText(model.get(TabBottomSheetPeekProperties.ACTION_BUTTON_TEXT));
        } else if (TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY == propertyKey) {
            view.setActionButtonVisibility(
                    model.get(TabBottomSheetPeekProperties.ACTION_BUTTON_VISIBILITY));
        } else if (TabBottomSheetPeekProperties.ACTION_BUTTON_ICON == propertyKey) {
            view.setActionButtonIcon(model.get(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON));
        } else if (TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT == propertyKey) {
            view.setActionButtonBackgroundTint(
                    model.get(TabBottomSheetPeekProperties.ACTION_BUTTON_BACKGROUND_TINT));
        } else if (TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_TINT == propertyKey) {
            view.setActionButtonIconTint(
                    model.get(TabBottomSheetPeekProperties.ACTION_BUTTON_ICON_TINT));
        } else if (TabBottomSheetPeekProperties.ACTION_BUTTON_HORIZONTAL_PADDING == propertyKey) {
            view.setActionButtonHorizontalPadding(
                    model.get(TabBottomSheetPeekProperties.ACTION_BUTTON_HORIZONTAL_PADDING));
        } else if (TabBottomSheetPeekProperties.ACTION_BUTTON_CONTENT_DESCRIPTION == propertyKey) {
            view.setActionButtonContentDescription(
                    model.get(TabBottomSheetPeekProperties.ACTION_BUTTON_CONTENT_DESCRIPTION));
        } else if (TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED == propertyKey) {
            view.setActionButtonClickListener(
                    v -> model.get(TabBottomSheetPeekProperties.ON_ACTION_BUTTON_CLICKED).run());
        } else if (TabBottomSheetPeekProperties.ON_CLOSE_CLICKED == propertyKey) {
            view.setCloseClickListener(
                    v -> model.get(TabBottomSheetPeekProperties.ON_CLOSE_CLICKED).run());
        } else if (TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED == propertyKey) {
            view.setPeekViewClickListener(
                    v -> model.get(TabBottomSheetPeekProperties.ON_PEEK_VIEW_CLICKED).run());
        }
    }
}
