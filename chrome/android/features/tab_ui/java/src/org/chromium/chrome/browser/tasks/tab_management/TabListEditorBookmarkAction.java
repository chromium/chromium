// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorActionMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/** Bookmark action for the {@link TabListEditorMenu}. */
public class TabListEditorBookmarkAction extends TabListEditorAction {
    private Activity mActivity;
    private TabListEditorBookmarkActionDelegate mDelegate;

    /** Interface for passing params on bookmark action. */
    public interface TabListEditorBookmarkActionDelegate {
        /**
         * Bookmark selected tabs and show snackbar.
         * @param activity the current activity.
         * @param tabs the list of currently selected tabs to perform the action on.
         * @param snackbarManager the snackbarManager used to show the snackbar.
         */
        void bookmarkTabsAndShowSnackbar(
                Activity activity, List<Tab> tabs, SnackbarManager snackbarManager);
    }

    /**
     * Create an action for bookmarking tabs.
     * @param activity for loading resources.
     * @param showMode whether to show an action view.
     * @param buttonType the type of the action view.
     * @param iconPosition the position of the icon in the action view.
     */
    public static TabListEditorAction createAction(
            Activity activity,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition) {
        Drawable drawable = AppCompatResources.getDrawable(activity, R.drawable.star_outline_24dp);
        TabListEditorBookmarkActionDelegate delegate =
                new TabListEditorBookmarkActionDelegateImpl();
        return new TabListEditorBookmarkAction(
                activity, showMode, buttonType, iconPosition, drawable, delegate);
    }

    private TabListEditorBookmarkAction(
            Activity activity,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable drawable,
            TabListEditorBookmarkActionDelegate delegate) {
        super(
                R.id.tab_list_editor_bookmark_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.plurals.tab_selection_editor_bookmark_tabs_action_button,
                R.plurals.accessibility_tab_selection_editor_bookmark_tabs_action_button,
                drawable);
        mActivity = activity;
        mDelegate = delegate;
    }

    private static class TabListEditorBookmarkActionDelegateImpl
            implements TabListEditorBookmarkActionDelegate {
        @Override
        public void bookmarkTabsAndShowSnackbar(
                Activity activity, List<Tab> tabs, SnackbarManager snackbarManager) {
            if (tabs.isEmpty()) return;

            Profile profile = tabs.get(0).getProfile();
            BookmarkModel bookmarkModel = BookmarkModel.getForProfile(profile);
            bookmarkModel.finishLoadingBookmarkModel(
                    () -> {
                        BookmarkUtils.addBookmarksOnMultiSelect(
                                activity, bookmarkModel, tabs, snackbarManager);
                    });
        }
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        int size =
                editorSupportsActionOnRelatedTabs()
                        ? getTabCountIncludingRelatedTabs(getTabGroupModelFilter(), tabIds)
                        : tabIds.size();
        setEnabledAndItemCount(!tabIds.isEmpty(), size);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Bookmark action should not be enabled for no tabs.";
        SnackbarManager snackbarManager = getActionDelegate().getSnackbarManager();
        snackbarManager.dismissAllSnackbars();

        if (mDelegate != null) {
            assert snackbarManager != null;
            mDelegate.bookmarkTabsAndShowSnackbar(mActivity, tabs, snackbarManager);
        }
        TabUiMetricsHelper.recordSelectionEditorActionMetrics(
                TabListEditorActionMetricGroups.BOOKMARK);
        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return false;
    }

    void setDelegateForTesting(TabListEditorBookmarkActionDelegate delegate) {
        var oldValue = mDelegate;
        mDelegate = delegate;
        ResettersForTesting.register(() -> mDelegate = oldValue);
    }
}
