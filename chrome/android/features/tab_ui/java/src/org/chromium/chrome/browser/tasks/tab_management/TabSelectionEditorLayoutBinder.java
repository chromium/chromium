// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Rect;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This is a ViewBinder for TabSelectionEditorLayout.
 */
public class TabSelectionEditorLayoutBinder {
    /**
     * This method binds the given model to the given view.
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, TabSelectionEditorLayout view, PropertyKey propertyKey) {
        if (TabSelectionEditorProperties.IS_VISIBLE == propertyKey) {
            if (model.get(TabSelectionEditorProperties.IS_VISIBLE)) {
                view.show();
            } else {
                view.hide();
            }
        } else if (TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_LISTENER == propertyKey) {
            view.getToolbar().setActionButtonOnClickListener(
                    model.get(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_LISTENER));
        } else if (TabSelectionEditorProperties.TOOLBAR_NAVIGATION_LISTENER == propertyKey) {
            view.getToolbar().setNavigationOnClickListener(
                    model.get(TabSelectionEditorProperties.TOOLBAR_NAVIGATION_LISTENER));
        } else if (TabSelectionEditorProperties.PRIMARY_COLOR == propertyKey) {
            view.setBackgroundColor(model.get(TabSelectionEditorProperties.PRIMARY_COLOR));
        } else if (TabSelectionEditorProperties.TOOLBAR_BACKGROUND_COLOR == propertyKey) {
            view.getToolbar().setToolbarBackgroundColor(
                    model.get(TabSelectionEditorProperties.TOOLBAR_BACKGROUND_COLOR));
        } else if (TabSelectionEditorProperties.TOOLBAR_GROUP_BUTTON_TINT == propertyKey) {
            view.getToolbar().setButtonTint(
                    model.get(TabSelectionEditorProperties.TOOLBAR_GROUP_BUTTON_TINT));
        } else if (TabSelectionEditorProperties.TOOLBAR_TEXT_APPEARANCE == propertyKey) {
            view.getToolbar().setTextAppearance(
                    model.get(TabSelectionEditorProperties.TOOLBAR_TEXT_APPEARANCE));
        } else if (TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_TEXT == propertyKey) {
            view.getToolbar().setActionButtonText(
                    model.get(TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_TEXT));
        } else if (TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD
                == propertyKey) {
            view.getToolbar().setActionButtonEnablingThreshold(model.get(
                    TabSelectionEditorProperties.TOOLBAR_ACTION_BUTTON_ENABLING_THRESHOLD));
        } else if (TabSelectionEditorProperties.SELECTION_EDITOR_POSITION_RECT == propertyKey) {
            Rect positionRect =
                    model.get(TabSelectionEditorProperties.SELECTION_EDITOR_POSITION_RECT);
            if (positionRect == null) {
                return;
            }
            view.updateTabSelectionEditorPositionRect(positionRect);
        } else if (TabSelectionEditorProperties.SELECTION_EDITOR_GLOBAL_LAYOUT_LISTENER
                == propertyKey) {
            view.registerGlobalLayoutListener(model.get(
                    TabSelectionEditorProperties.SELECTION_EDITOR_GLOBAL_LAYOUT_LISTENER));
        }
    }
}
