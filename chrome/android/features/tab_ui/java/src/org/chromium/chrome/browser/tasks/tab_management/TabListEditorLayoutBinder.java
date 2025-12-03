// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.CreationMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** This is a ViewBinder for TabListEditorLayout. */
@NullMarked
public class TabListEditorLayoutBinder {
    /**
     * This method binds the given model to the given view.
     *
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(
            PropertyModel model, TabListEditorLayout view, PropertyKey propertyKey) {
        if (TabListEditorProperties.CREATION_MODE == propertyKey) {
            // Read the mode from the model and pass it to the view's setter.
            TabListEditorActionViewLayout actionViewLayout =
                    view.findViewById(R.id.action_view_layout);
            @CreationMode int creationMode = model.get(TabListEditorProperties.CREATION_MODE);
            actionViewLayout.setCreationMode(creationMode);
            view.getToolbar().setCreationModeText(creationMode);
        } else if (propertyKey == TabListEditorProperties.DONE_BUTTON_CLICK_HANDLER) {
            view.getToolbar()
                    .getActionViewLayout()
                    .setDoneButtonOnClickListener(
                            model.get(TabListEditorProperties.DONE_BUTTON_CLICK_HANDLER));
        } else if (TabListEditorProperties.DONE_BUTTON_VISIBILITY == propertyKey) {
            view.getToolbar()
                    .getActionViewLayout()
                    .setDoneButtonVisibility(
                            model.get(TabListEditorProperties.DONE_BUTTON_VISIBILITY));
        } else if (TabListEditorProperties.IS_DONE_BUTTON_ENABLED == propertyKey) {
            view.getToolbar()
                    .getActionViewLayout()
                    .setIsDoneButtonEnabled(
                            model.get(TabListEditorProperties.IS_DONE_BUTTON_ENABLED));
        } else if (TabListEditorProperties.IS_VISIBLE == propertyKey) {
            if (model.get(TabListEditorProperties.IS_VISIBLE)) {
                view.show();
            } else {
                view.hide();
            }
        } else if (TabListEditorProperties.PRIMARY_COLOR == propertyKey) {
            view.setBackgroundColor(model.get(TabListEditorProperties.PRIMARY_COLOR));
        } else if (TabListEditorProperties.TOOLBAR_BACKGROUND_COLOR == propertyKey) {
            view.getToolbar()
                    .setToolbarBackgroundColor(
                            model.get(TabListEditorProperties.TOOLBAR_BACKGROUND_COLOR));
        } else if (TabListEditorProperties.TOOLBAR_BUTTON_TINT == propertyKey) {
            view.getToolbar().setButtonTint(model.get(TabListEditorProperties.TOOLBAR_BUTTON_TINT));
        } else if (TabListEditorProperties.TOOLBAR_NAVIGATION_LISTENER == propertyKey) {
            view.getToolbar()
                    .setNavigationOnClickListener(
                            model.get(TabListEditorProperties.TOOLBAR_NAVIGATION_LISTENER));
        } else if (TabListEditorProperties.TOOLBAR_TEXT_TINT == propertyKey) {
            view.getToolbar()
                    .setTextColorStateList(model.get(TabListEditorProperties.TOOLBAR_TEXT_TINT));
        } else if (TabListEditorProperties.TOOLBAR_TITLE == propertyKey) {
            view.getToolbar().setTitle(model.get(TabListEditorProperties.TOOLBAR_TITLE));
        } else if (TabListEditorProperties.TOP_MARGIN == propertyKey) {
            ViewGroup.MarginLayoutParams layoutParams = (MarginLayoutParams) view.getLayoutParams();
            layoutParams.topMargin = model.get(TabListEditorProperties.TOP_MARGIN);
            // Calling setLayoutParams to requestLayout() for margin to take effect.
            view.setLayoutParams(layoutParams);
        }
    }
}
