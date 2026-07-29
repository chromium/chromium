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
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.UndoGroupMetadata;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarThrottle;
import org.chromium.ui.util.TokenHolder;

import java.util.Locale;

/**
 * A controller that listens to {@link TabGroupObserver#showUndoGroupSnackbar} and shows an undo
 * snackbar. Also implements {@link UndoBarThrottle} to delay snackbar display during active UI
 * operations (e.g., drag-and-drop).
 */
@NullMarked
public class UndoGroupSnackbarController
        implements SnackbarManager.SnackbarController, UndoBarThrottle {
    private final Context mContext;
    private final TabModelSelector mTabModelSelector;
    private final SnackbarManager mSnackbarManager;
    private final TabGroupObserver mTabGroupObserver;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final TabModelSelectorTabModelObserver mTabModelSelectorTabModelObserver;
    private final TokenHolder mThrottle = new TokenHolder(this::maybeShowUndoGroupSnackbar);
    private @Nullable UndoGroupMetadata mPendingUndoGroupMetadata;

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
        mTabGroupObserver =
                new TabGroupObserver() {
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
                        if (mThrottle.hasTokens()) {
                            // Only the most recent undo group snackbar should be preserved while
                            // throttled. Expire any existing pending operation so we do not leak
                            // detached tab groups in the TabModel.
                            expirePendingUndoGroupMetadata();
                            mPendingUndoGroupMetadata = undoGroupMetadata;
                        } else {
                            showUndoGroupSnackbarInternal(undoGroupMetadata);
                        }
                    }
                };

        getTabModel(/* isIncognito= */ false).addTabGroupObserver(mTabGroupObserver);
        getTabModel(/* isIncognito= */ true).addTabGroupObserver(mTabGroupObserver);

        mCurrentTabModelObserver = (tabModel) -> dismissSnackbars();

        mTabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndPostIfNonNull(mCurrentTabModelObserver);

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
     * TabModelSelector#getCurrentTabModelSupplier()} and {@link TabGroupObserver} from {@link
     * TabModel}.
     */
    public void destroy() {
        expirePendingUndoGroupMetadata();
        if (mTabModelSelector != null) {
            mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            getTabModel(/* isIncognito= */ false).removeTabGroupObserver(mTabGroupObserver);
            getTabModel(/* isIncognito= */ true).removeTabGroupObserver(mTabGroupObserver);
        }
        mTabModelSelectorTabModelObserver.destroy();
    }

    // Implement UndoBarThrottle interface.

    @Override
    public int startThrottling() {
        return mThrottle.acquireToken();
    }

    @Override
    public void stopThrottling(int token) {
        mThrottle.releaseToken(token);
    }

    // Implement SnackbarManager.SnackbarController interface.

    @Override
    public void onAction(@Nullable Object actionData) {
        assumeNonNull(actionData);
        UndoGroupMetadata undoGroupMetadata = (UndoGroupMetadata) actionData;
        TabModel tabModel = getTabModel(undoGroupMetadata.isIncognito());
        tabModel.performUndoGroupOperation(undoGroupMetadata);
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        assumeNonNull(actionData);
        UndoGroupMetadata undoGroupMetadata = (UndoGroupMetadata) actionData;
        TabModel tabModel = getTabModel(undoGroupMetadata.isIncognito());
        tabModel.undoGroupOperationExpired(undoGroupMetadata);
    }

    private void maybeShowUndoGroupSnackbar() {
        if (mPendingUndoGroupMetadata != null) {
            showUndoGroupSnackbarInternal(mPendingUndoGroupMetadata);
            mPendingUndoGroupMetadata = null;
        }
    }

    private void dismissSnackbars() {
        expirePendingUndoGroupMetadata();
        mSnackbarManager.dismissSnackbars(UndoGroupSnackbarController.this);
    }

    private void expirePendingUndoGroupMetadata() {
        if (mPendingUndoGroupMetadata != null) {
            TabModel tabModel = getTabModel(mPendingUndoGroupMetadata.isIncognito());
            tabModel.undoGroupOperationExpired(mPendingUndoGroupMetadata);
            mPendingUndoGroupMetadata = null;
        }
    }

    private void showUndoGroupSnackbarInternal(UndoGroupMetadata undoGroupMetadata) {
        TabModel tabModel = getTabModel(undoGroupMetadata.isIncognito());
        int mergedGroupSize = tabModel.getTabCountForGroup(undoGroupMetadata.getTabGroupId());

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

    private TabModel getTabModel(boolean isIncognito) {
        return mTabModelSelector.getModel(isIncognito);
    }
}
