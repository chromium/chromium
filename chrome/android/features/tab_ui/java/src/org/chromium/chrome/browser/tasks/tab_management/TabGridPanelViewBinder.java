// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.ADD_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.ANIMATION_SOURCE_VIEW;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.COLLAPSE_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.CONTENT_TOP_MARGIN;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.DIALOG_BACKGROUND_RESOUCE_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.DIALOG_UNGROUP_BAR_TEXT_APPEARANCE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.HEADER_TITLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.IS_DIALOG_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.IS_MAIN_CONTENT_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.IS_POPUP_WINDOW_FOCUSABLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.IS_TITLE_TEXT_FOCUSED;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.MENU_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.PRIMARY_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.SCRIMVIEW_OBSERVER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.TINT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.TITLE_CURSOR_VISIBILITY;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.TITLE_TEXT_ON_FOCUS_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.TITLE_TEXT_ON_TOUCH_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.TITLE_TEXT_WATCHER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGridPanelProperties.UNGROUP_BAR_STATUS;

import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ViewBinder for TabGridDialog.
 */
class TabGridPanelViewBinder {
    /**
     * ViewHolder class to get access to all {@link View}s inside the TabGridDialog.
     */
    public static class ViewHolder {
        public final TabGroupUiToolbarView toolbarView;
        public final RecyclerView contentView;
        @Nullable
        public TabGridDialogParent dialogView;

        ViewHolder(TabGroupUiToolbarView toolbarView, RecyclerView contentView,
                @Nullable TabGridDialogParent dialogView) {
            this.toolbarView = toolbarView;
            this.contentView = contentView;
            this.dialogView = dialogView;
        }
    }

    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     * @param model The model to use.
     * @param viewHolder The ViewHolder to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (COLLAPSE_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setLeftButtonOnClickListener(model.get(COLLAPSE_CLICK_LISTENER));
        } else if (ADD_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setRightButtonOnClickListener(model.get(ADD_CLICK_LISTENER));
        } else if (HEADER_TITLE == propertyKey) {
            viewHolder.toolbarView.setTitle(model.get(HEADER_TITLE));
        } else if (CONTENT_TOP_MARGIN == propertyKey) {
            ((FrameLayout.LayoutParams) viewHolder.contentView.getLayoutParams()).topMargin =
                    model.get(CONTENT_TOP_MARGIN);
            viewHolder.contentView.requestLayout();
        } else if (PRIMARY_COLOR == propertyKey) {
            viewHolder.toolbarView.setPrimaryColor(model.get(PRIMARY_COLOR));
            viewHolder.contentView.setBackgroundColor(model.get(PRIMARY_COLOR));
        } else if (TINT == propertyKey) {
            viewHolder.toolbarView.setTint(model.get(TINT));
        } else if (SCRIMVIEW_OBSERVER == propertyKey) {
            viewHolder.dialogView.setScrimViewObserver(model.get(SCRIMVIEW_OBSERVER));
        } else if (IS_DIALOG_VISIBLE == propertyKey) {
            if (model.get(IS_DIALOG_VISIBLE)) {
                viewHolder.dialogView.resetDialog(viewHolder.toolbarView, viewHolder.contentView);
                viewHolder.dialogView.showDialog();
            } else {
                viewHolder.dialogView.hideDialog();
            }
        } else if (ANIMATION_SOURCE_VIEW == propertyKey) {
            viewHolder.dialogView.setupDialogAnimation(model.get(ANIMATION_SOURCE_VIEW));
        } else if (UNGROUP_BAR_STATUS == propertyKey) {
            viewHolder.dialogView.updateUngroupBar(model.get(UNGROUP_BAR_STATUS));
        } else if (DIALOG_BACKGROUND_RESOUCE_ID == propertyKey) {
            if (viewHolder.dialogView != null) {
                int backgroundResourceId = model.get(DIALOG_BACKGROUND_RESOUCE_ID);
                viewHolder.dialogView.updateDialogContainerBackgroundResource(backgroundResourceId);
                viewHolder.toolbarView.setBackgroundResource(backgroundResourceId);
            }
        } else if (DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateUngroupBarBackgroundColor(
                        model.get(DIALOG_UNGROUP_BAR_BACKGROUND_COLOR_ID));
            }
        } else if (DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateUngroupBarHoveredBackgroundColor(
                        model.get(DIALOG_UNGROUP_BAR_HOVERED_BACKGROUND_COLOR_ID));
            }
        } else if (DIALOG_UNGROUP_BAR_TEXT_APPEARANCE == propertyKey) {
            if (viewHolder.dialogView != null) {
                viewHolder.dialogView.updateUngroupBarTextAppearance(
                        model.get(DIALOG_UNGROUP_BAR_TEXT_APPEARANCE));
            }
        } else if (INITIAL_SCROLL_INDEX == propertyKey) {
            int index = (Integer) model.get(INITIAL_SCROLL_INDEX);
            ((LinearLayoutManager) viewHolder.contentView.getLayoutManager())
                    .scrollToPositionWithOffset(index, 0);
        } else if (IS_MAIN_CONTENT_VISIBLE == propertyKey) {
            viewHolder.contentView.setVisibility(View.VISIBLE);
        } else if (MENU_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setMenuButtonOnClickListener(model.get(MENU_CLICK_LISTENER));
        } else if (TITLE_TEXT_WATCHER == propertyKey) {
            viewHolder.toolbarView.setTitleTextOnChangedListener(model.get(TITLE_TEXT_WATCHER));
        } else if (TITLE_TEXT_ON_FOCUS_LISTENER == propertyKey) {
            viewHolder.toolbarView.setTitleTextOnFocusChangeListener(
                    model.get(TITLE_TEXT_ON_FOCUS_LISTENER));
        } else if (TITLE_CURSOR_VISIBILITY == propertyKey) {
            viewHolder.toolbarView.setTitleCursorVisibility(model.get(TITLE_CURSOR_VISIBILITY));
        } else if (IS_TITLE_TEXT_FOCUSED == propertyKey) {
            // Don't explicitly request focus since it should happen automatically.
            if (!model.get(IS_TITLE_TEXT_FOCUSED)) {
                viewHolder.toolbarView.clearTitleTextFocus();
            }
        } else if (IS_POPUP_WINDOW_FOCUSABLE == propertyKey) {
            viewHolder.dialogView.setPopupWindowFocusable(model.get(IS_POPUP_WINDOW_FOCUSABLE));
        } else if (TITLE_TEXT_ON_TOUCH_LISTENER == propertyKey) {
            viewHolder.toolbarView.setTitleOnTouchListener(model.get(TITLE_TEXT_ON_TOUCH_LISTENER));
        }
    }
}
