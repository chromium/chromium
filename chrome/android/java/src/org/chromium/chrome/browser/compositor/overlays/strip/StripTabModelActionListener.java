// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Implementation of {@link TabModelActionListener} for the tab strip that helps manage the correct
 * event sequence for {@link TabRemover} and {@link TabUngrouper} invocations.
 */
@NullMarked
public class StripTabModelActionListener implements TabModelActionListener {
    /** An enum representing the type of event. */
    @IntDef({ActionType.DRAG_OFF_STRIP, ActionType.REORDER, ActionType.CLOSE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ActionType {
        /** This action is dragging a tab off the strip. */
        int DRAG_OFF_STRIP = 0;

        /** This action is reordering tabs. */
        int REORDER = 1;

        /** This action is closing tabs. */
        int CLOSE = 2;
    }

    private final int mRootId;
    private final @ActionType int mActionType;
    private final @Nullable View mToolbarContainerView;
    private final ObservableSupplierImpl<Integer> mGroupIdToHideSupplier;
    private final @Nullable Runnable mBeforeSyncDialogRunnable;
    private final @Nullable Runnable mOnSuccess;

    /**
     * @param rootId The root ID of the tab group being acted on.
     * @param actionType The {@link ActionType} of the operation.
     * @param groupIdToHideSupplier A supplier to set with the {@code rootId} to hide the group if
     *     applicable.
     * @param toolbarContainerView The container view of the toolbar to control drag actions.
     * @param beforeSyncDialogRunnable A runnable invoked if the sync dialog will show before the
     *     show occurs.
     * @param onSuccess Invoked if the action is immediately continued or completed with a positive
     *     action.
     */
    public StripTabModelActionListener(
            int rootId,
            @ActionType int actionType,
            ObservableSupplierImpl<Integer> groupIdToHideSupplier,
            @Nullable View toolbarContainerView,
            @Nullable Runnable beforeSyncDialogRunnable,
            @Nullable Runnable onSuccess) {
        mRootId = rootId;
        mActionType = actionType;
        mGroupIdToHideSupplier = groupIdToHideSupplier;
        mToolbarContainerView = toolbarContainerView;
        mBeforeSyncDialogRunnable = beforeSyncDialogRunnable;
        mOnSuccess = onSuccess;
    }

    @Override
    public void willPerformActionOrShowDialog(@DialogType int dialogType, boolean willSkipDialog) {
        // If this method is called for DialogType.SYNC we are interrupting an operation that would
        // remove the last tab in a group that will be interrupted. For any other dialogType the
        // operation will go to completion without interruption.
        if (dialogType != DialogType.SYNC || willSkipDialog) return;

        if (mBeforeSyncDialogRunnable != null) mBeforeSyncDialogRunnable.run();

        if (mToolbarContainerView != null) {
            mToolbarContainerView.cancelDragAndDrop();
        }

        assert Objects.equals(mGroupIdToHideSupplier.get(), Tab.INVALID_TAB_ID);
        mGroupIdToHideSupplier.set(mRootId);
    }

    @Override
    public void onConfirmationDialogResult(
            @DialogType int dialogType, @ActionConfirmationResult int result) {
        switch (result) {
            case ActionConfirmationResult.IMMEDIATE_CONTINUE:
                assert mActionType != ActionType.DRAG_OFF_STRIP; // fallthrough
            case ActionConfirmationResult.CONFIRMATION_POSITIVE:
                if (mOnSuccess != null) mOnSuccess.run();
                break;
            case ActionConfirmationResult.CONFIRMATION_NEGATIVE:
                // Restore the hidden group.
                if (dialogType == DialogType.SYNC) {
                    mGroupIdToHideSupplier.set(Tab.INVALID_TAB_ID);
                }
                break;
            default:
                assert false : "Not reached.";
        }
    }
}
