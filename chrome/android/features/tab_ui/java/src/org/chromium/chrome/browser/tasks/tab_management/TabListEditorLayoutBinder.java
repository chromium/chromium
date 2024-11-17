// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** This is a ViewBinder for TabListEditorLayout. */
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
        if (TabListEditorProperties.IS_VISIBLE == propertyKey) {
            if (model.get(TabListEditorProperties.IS_VISIBLE)) {
                view.show();
            } else {
                view.hide();
            }
        } else if (TabListEditorProperties.TOOLBAR_NAVIGATION_LISTENER == propertyKey) {
            view.getToolbar()
                    .setNavigationOnClickListener(
                            model.get(TabListEditorProperties.TOOLBAR_NAVIGATION_LISTENER));
        } else if (TabListEditorProperties.PRIMARY_COLOR == propertyKey) {
            view.setBackgroundColor(model.get(TabListEditorProperties.PRIMARY_COLOR));
        } else if (TabListEditorProperties.TOOLBAR_BACKGROUND_COLOR == propertyKey) {
            view.getToolbar()
                    .setToolbarBackgroundColor(
                            model.get(TabListEditorProperties.TOOLBAR_BACKGROUND_COLOR));
        } else if (TabListEditorProperties.TOOLBAR_TEXT_TINT == propertyKey) {
            view.getToolbar()
                    .setTextColorStateList(model.get(TabListEditorProperties.TOOLBAR_TEXT_TINT));
        } else if (TabListEditorProperties.TOOLBAR_BUTTON_TINT == propertyKey) {
            view.getToolbar().setButtonTint(model.get(TabListEditorProperties.TOOLBAR_BUTTON_TINT));
        } else if (TabListEditorProperties.RELATED_TAB_COUNT_PROVIDER == propertyKey) {
            view.getToolbar()
                    .setRelatedTabCountProvider(
                            model.get(TabListEditorProperties.RELATED_TAB_COUNT_PROVIDER));
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
