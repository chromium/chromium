// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.BACKGROUND_COLOR;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.IMAGE_TILES_CONTAINER_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.INITIAL_SCROLL_INDEX;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.NEW_TAB_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.SHOW_GROUP_DIALOG_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.SHOW_GROUP_DIALOG_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.TINT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupUiProperties.WIDTH_PX_CALLBACK;

import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for TabGroupUi component. */
@NullMarked
class TabGroupUiViewBinder {
    /** ViewHolder class to get access to all {@link View}s inside the TabGroupUi. */
    public static class ViewHolder {
        public final TabGroupUiToolbarView toolbarView;
        public final RecyclerView contentView;

        ViewHolder(TabGroupUiToolbarView toolbarView, RecyclerView contentView) {
            this.toolbarView = toolbarView;
            this.contentView = contentView;
        }
    }

    /**
     * Binds the given model to the given view, updating the payload in propertyKey.
     *
     * @param model The model to use.
     * @param viewHolder The {@link ViewHolder} to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(PropertyModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (SHOW_GROUP_DIALOG_ON_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setShowGroupDialogButtonOnClickListener(
                    model.get(SHOW_GROUP_DIALOG_ON_CLICK_LISTENER));
            viewHolder.toolbarView.setImageTilesContainerOnClickListener(
                    model.get(SHOW_GROUP_DIALOG_ON_CLICK_LISTENER));
        } else if (NEW_TAB_BUTTON_ON_CLICK_LISTENER == propertyKey) {
            viewHolder.toolbarView.setNewTabButtonOnClickListener(
                    model.get(NEW_TAB_BUTTON_ON_CLICK_LISTENER));
        } else if (IS_MAIN_CONTENT_VISIBLE == propertyKey) {
            viewHolder.toolbarView.setMainContentVisibility(model.get(IS_MAIN_CONTENT_VISIBLE));
        } else if (BACKGROUND_COLOR == propertyKey) {
            viewHolder.toolbarView.setContentBackgroundColor(model.get(BACKGROUND_COLOR));
        } else if (SHOW_GROUP_DIALOG_BUTTON_VISIBLE == propertyKey) {
            viewHolder.toolbarView.setShowGroupDialogButtonVisible(
                    model.get(SHOW_GROUP_DIALOG_BUTTON_VISIBLE));
        } else if (IMAGE_TILES_CONTAINER_VISIBLE == propertyKey) {
            viewHolder.toolbarView.setImageTilesContainerVisible(
                    model.get(IMAGE_TILES_CONTAINER_VISIBLE));
        } else if (TINT == propertyKey) {
            viewHolder.toolbarView.setTint(model.get(TINT));
        } else if (INITIAL_SCROLL_INDEX == propertyKey) {
            scrollToIndex(viewHolder, model);
        } else if (WIDTH_PX_CALLBACK == propertyKey) {
            viewHolder.toolbarView.setWidthPxCallback(model.get(WIDTH_PX_CALLBACK));
        }
    }

    private static void scrollToIndex(ViewHolder viewHolder, PropertyModel model) {
        // This is a speculative fix for https://crbug.com/40948489. A crash is happening due to a
        // temporarily detached view to TabListRecyclerView not being removed from the RecyclerView
        // before it is recycled. This typically happens when an animating view gets recycled. While
        // this is arguably a bug with the recycler view itself, a common cause of this issue is a
        // programmatic scroll happening when the the RecyclerView is animating. The structure of
        // the runnable below should avoid this happening by skipping the scroll logic until after
        // the the view is done animating and is laid out.
        RecyclerView contentView = viewHolder.contentView;
        Runnable scrollRunnable =
                new Runnable() {
                    @Override
                    public void run() {
                        // Retry if animating or layout is incomplete.
                        if (contentView.isAnimating()
                                || contentView.getWidth() == 0
                                || contentView.getHeight() == 0) {
                            contentView.post(this);
                            return;
                        }
                        int index = model.get(INITIAL_SCROLL_INDEX);
                        LinearLayoutManager manager =
                                (LinearLayoutManager) assumeNonNull(contentView.getLayoutManager());
                        int showingItemsCount =
                                manager.findLastVisibleItemPosition()
                                        - manager.findFirstVisibleItemPosition();
                        // Try to scroll to a state where the selected tab is in the middle of the
                        // strip.
                        manager.scrollToPositionWithOffset(index - showingItemsCount / 2, 0);
                    }
                };
        scrollRunnable.run();
    }
}
