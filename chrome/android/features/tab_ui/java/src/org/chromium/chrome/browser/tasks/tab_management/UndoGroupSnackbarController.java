// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.UndoGroupMetadata;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import java.util.Locale;

/**
 * A controller that listens to {@link TabGroupModelFilterObserver#showUndoGroupSnackbar} and shows
 * a undo snackbar.
 */
@NullMarked
public class UndoGroupSnackbarController implements SnackbarManager.SnackbarController {
    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private final SnackbarManager mSnackbarManager;
    private final TabGroupModelFilterObserver mTabGroupModelFilterObserver;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;

    /**
     * @param context The current Android context.
     * @param tabModelSelector The current {@link TabModelSelector}.
     * @param snackbarManager Manages the snackbar.
     */
    public UndoGroupSnackbarController(
            Context context, TabModelSelector tabModelSelector, SnackbarManager snackbarManager) {
        mContext = context;
        mTabModelSelector = tabModelSelector;
        mSnackbarManager = snackbarManager;
        mTabGroupModelFilterObserver =
                new TabGroupModelFilterObserver() {
                    @Override
                    public void willMoveTabOutOfGroup(
                            Tab movedTab, @Nullable Token destinationTabGroupId) {
                        // Fix for b/338511492 is to dismiss the snackbar if an ungroup operation
                        // happens because information that allowed the group action to be undone
                        // may no longer be usable (incorrect indices, group IDs, etc.).
                        dismissSnackbars();
                    }

                    @Override
                    public void showUndoGroupSnackbar(UndoGroupMetadata undoGroupMetadata) {
                        showUndoGroupSnackbarInternal(undoGroupMetadata);
                    }
                };

        getFilter(/* isIncognito= */ false).addTabGroupObserver(mTabGroupModelFilterObserver);
        getFilter(/* isIncognito= */ true).addTabGroupObserver(mTabGroupModelFilterObserver);

        mCurrentTabModelObserver = (tabModel) -> dismissSnackbars();

        mTabModelSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);

        mTabModelSelectorTabModelObserver =
                new TabModelSelectorTabModelObserver(mTabModelSelector) {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        dismissSnackbars();
                    }

                    @Override
                    public void willCloseTab(Tab tab, boolean didCloseAlone) {
                        dismissSnackbars();
                    }

                    @Override
                    public void onFinishingTabClosure(
                            Tab tab, @TabClosingSource int closingSource) {
                        dismissSnackbars();
                    }
                };
    }

    /**
     * Cleans up this class, removes {@link Callback<TabModel>} from {@link
     * TabModelSelector#getCurrentTabModelSupplier()} and {@link TabGroupModelFilterObserver} from
     * {@link TabGroupModelFilter}.
     */
    public void destroy() {
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            getFilter(/* isIncognito= */ false)
                    .removeTabGroupObserver(mTabGroupModelFilterObserver);
            getFilter(/* isIncognito= */ true).removeTabGroupObserver(mTabGroupModelFilterObserver);
        }
        mTabModelSelectorTabModelObserver.destroy();
    }

    private void dismissSnackbars() {
        mSnackbarManager.dismissSnackbars(UndoGroupSnackbarController.this);
    }

    private void showUndoGroupSnackbarInternal(UndoGroupMetadata undoGroupMetadata) {
        TabGroupModelFilter filter = getFilter(undoGroupMetadata.isIncognito());
        int mergedGroupSize = filter.getTabCountForGroup(undoGroupMetadata.getTabGroupId());

        String content = String.format(Locale.getDefault(), "%d", mergedGroupSize);
        String templateText;
        if (mergedGroupSize == 1) {
            templateText = mContext.getString(R.string.undo_bar_group_tab_message);
        } else {
            templateText = mContext.getString(R.string.undo_bar_group_tabs_message);
        }
        mSnackbarManager.showSnackbar(
                Snackbar.make(
                                content,
                                this,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_TAB_GROUP_MANUAL_CREATION_UNDO)
                        .setTemplateText(templateText)
                        .setAction(mContext.getString(R.string.undo), undoGroupMetadata));
    }

    @Override
    public void onAction(@Nullable Object actionData) {
        assumeNonNull(actionData);
        UndoGroupMetadata undoGroupMetadata = (UndoGroupMetadata) actionData;
        TabGroupModelFilter filter = getFilter(undoGroupMetadata.isIncognito());
        filter.performUndoGroupOperation(undoGroupMetadata);
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        assumeNonNull(actionData);
        UndoGroupMetadata undoGroupMetadata = (UndoGroupMetadata) actionData;
        TabGroupModelFilter filter = getFilter(undoGroupMetadata.isIncognito());
        filter.undoGroupOperationExpired(undoGroupMetadata);
    }

    private TabGroupModelFilter getFilter(boolean isIncognito) {
        return assumeNonNull(
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getTabGroupModelFilter(isIncognito));
    }
}
