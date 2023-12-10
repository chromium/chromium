// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.PopupWindow;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.HashMap;
import java.util.Map;

/** This class is used to show the {@link SelectableListLayout} in a {@link PopupWindow}. */
class TabListEditorLayout extends SelectableListLayout<Integer> {
    private TabListEditorToolbar mToolbar;
    private ViewGroup mParentView;
    private boolean mIsInitialized;
    private boolean mIsShowing;

    private Map<View, Integer> mAccessibilityImportanceMap = new HashMap<>();

    // TODO(meiliang): inflates R.layout.tab_list_editor_layout in
    // TabListEditorCoordinator.
    public TabListEditorLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Initializes the RecyclerView and the toolbar for the layout. Also initializes the selection
     * editor layout provider if there is one.This must be called before calling show/hide.
     *
     * @param parentView The parent view to attach the {@link TabListEditorLayout}.
     * @param recyclerView The recycler view to be shown.
     * @param adapter The adapter that provides views that represent items in the recycler view.
     * @param selectionDelegate The {@link SelectionDelegate} that will inform the toolbar of
     *                            selection changes.
     */
    void initialize(
            ViewGroup parentView,
            RecyclerView recyclerView,
            RecyclerView.Adapter adapter,
            SelectionDelegate<Integer> selectionDelegate) {
        mIsInitialized = true;
        initializeRecyclerView(adapter, recyclerView);
        mToolbar =
                (TabListEditorToolbar)
                        initializeToolbar(
                                R.layout.tab_list_editor_toolbar,
                                selectionDelegate,
                                0,
                                0,
                                0,
                                null,
                                true);
        mParentView = parentView;
    }

    /** Add and shows the layout in the parent view. */
    public void show() {
        assert mIsInitialized;
        mIsShowing = true;
        clearBackgroundViewAccessibilityImportance();
        mParentView.addView(this);
    }

    /** Remove and hides the layout from parent view. */
    public void hide() {
        assert mIsInitialized && mIsShowing;
        mIsShowing = false;
        mParentView.removeView(this);
        restoreBackgroundViewAccessibilityImportance();
    }

    /**
     * @return The toolbar of the layout.
     */
    public TabListEditorToolbar getToolbar() {
        return mToolbar;
    }

    /** Destroy any members that needs clean up. */
    public void destroy() {
        super.onDestroyed();
    }

    private void clearBackgroundViewAccessibilityImportance() {
        assert mAccessibilityImportanceMap.size() == 0 && mParentView.indexOfChild(this) == -1;

        for (int i = 0; i < mParentView.getChildCount(); i++) {
            View view = mParentView.getChildAt(i);
            mAccessibilityImportanceMap.put(view, view.getImportantForAccessibility());
            view.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        }
        mAccessibilityImportanceMap.put(mParentView, mParentView.getImportantForAccessibility());
        mParentView.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    private void restoreBackgroundViewAccessibilityImportance() {
        assert mParentView.indexOfChild(this) == -1;

        for (int i = 0; i < mParentView.getChildCount(); i++) {
            View view = mParentView.getChildAt(i);

            assert mAccessibilityImportanceMap.containsKey(view);
            Integer importance = mAccessibilityImportanceMap.get(view);
            view.setImportantForAccessibility(
                    importance == null ? IMPORTANT_FOR_ACCESSIBILITY_AUTO : importance);
        }
        assert mAccessibilityImportanceMap.containsKey(mParentView);
        Integer importance = mAccessibilityImportanceMap.get(mParentView);
        mParentView.setImportantForAccessibility(
                importance == null ? IMPORTANT_FOR_ACCESSIBILITY_AUTO : importance);
        mAccessibilityImportanceMap.clear();
    }
}
