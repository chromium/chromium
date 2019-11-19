// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Rect;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.PopupWindow;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.tab_ui.R;

/**
 * This class is used to show the {@link SelectableListLayout} in a {@link PopupWindow}.
 */
class TabSelectionEditorLayout extends SelectableListLayout<Integer> {
    private final PopupWindow mWindow;
    private TabSelectionEditorToolbar mToolbar;
    private View mParentView;
    private ViewTreeObserver.OnGlobalLayoutListener mParentLayoutListener;
    private @Nullable Rect mPositionRect;
    private boolean mIsInitialized;

    // TODO(meiliang): inflates R.layout.tab_selection_editor_layout in
    // TabSelectionEditorCoordinator.
    public TabSelectionEditorLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mWindow = new PopupWindow(
                this, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
    }

    /**
     * Initializes the RecyclerView and the toolbar for the layout. Also initializes the selection
     * editor layout provider if there is one.This must be called before calling show/hide.
     *
     * @param parentView The parent view for the {@link PopupWindow}.
     * @param recyclerView The recycler view to be shown.
     * @param adapter The adapter that provides views that represent items in the recycler view.
     * @param selectionDelegate The {@link SelectionDelegate} that will inform the toolbar of
     *                            selection changes.
     */
    void initialize(View parentView, RecyclerView recyclerView, RecyclerView.Adapter adapter,
            SelectionDelegate<Integer> selectionDelegate) {
        mIsInitialized = true;
        initializeRecyclerView(adapter, recyclerView);
        mToolbar =
                (TabSelectionEditorToolbar) initializeToolbar(R.layout.tab_selection_editor_toolbar,
                        selectionDelegate, 0, 0, 0, null, false, true);
        mParentView = parentView;
    }

    /**
     * Shows the layout in a {@link PopupWindow}.
     */
    public void show() {
        assert mIsInitialized;
        if (mPositionRect == null) {
            mWindow.showAtLocation(mParentView, Gravity.CENTER, 0, 0);
            return;
        }
        mWindow.setWidth(mPositionRect.width());
        mWindow.setHeight(mPositionRect.height());
        mWindow.showAtLocation(
                mParentView, Gravity.NO_GRAVITY, mPositionRect.left, mPositionRect.top);
    }

    /**
     * Hides the {@link PopupWindow}.
     */
    public void hide() {
        assert mIsInitialized;
        if (mWindow.isShowing()) mWindow.dismiss();
    }

    /**
     * @return The toolbar of the layout.
     */
    public TabSelectionEditorToolbar getToolbar() {
        return mToolbar;
    }

    /**
     * Register a {@link ViewTreeObserver.OnGlobalLayoutListener} handling TabSelectionEditor
     * related changes when parent view global layout changed.
     */
    public void registerGlobalLayoutListener(ViewTreeObserver.OnGlobalLayoutListener listener) {
        if (mParentView == null || listener == null) return;
        if (mParentLayoutListener != null) {
            mParentView.getViewTreeObserver().removeOnGlobalLayoutListener(mParentLayoutListener);
        }
        mParentLayoutListener = listener;
        mParentView.getViewTreeObserver().addOnGlobalLayoutListener(listener);
    }

    /**
     * Update the {@link Rect} used to show the selection editor. If the editor is currently
     * showing, update its positioning.
     * @param rect  The {@link Rect} to update mPositionRect.
     */
    public void updateTabSelectionEditorPositionRect(@NonNull Rect rect) {
        assert rect != null;
        mPositionRect = rect;
        if (mWindow.isShowing()) {
            mWindow.update(rect.left, rect.top, rect.width(), rect.height());
        }
    }

    /**
     * Destroy any members that needs clean up.
     */
    public void destroy() {
        if (mParentView != null && mParentLayoutListener != null) {
            mParentView.getViewTreeObserver().removeOnGlobalLayoutListener(mParentLayoutListener);
        }
    }

    @VisibleForTesting
    Rect getPositionRectForTesting() {
        return mPositionRect;
    }
}
