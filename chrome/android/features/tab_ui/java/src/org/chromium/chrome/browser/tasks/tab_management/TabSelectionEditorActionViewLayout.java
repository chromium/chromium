// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.VisibleForTesting;
import androidx.collection.ArraySet;

import org.chromium.base.MathUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.NumberRollView;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.Set;

/**
 * A {@link LinearLayout} that displays only the TabSelectionEditorMenuItem ActionViews that fit in
 * the space it contains. Managed by a {@link TabSelectionEditorMenu}.
 */
public class TabSelectionEditorActionViewLayout extends LinearLayout {
    /**
     * All {@link TabSelectionEditoreMenuItem} action views with menu items.
     */
    private final ArrayList<TabSelectionEditorMenuItem> mMenuItemsWithActionView =
            new ArrayList<>();
    /**
     * The {@link TabSelectionEditoreMenuItem}s with visible action views.
     */
    private final Set<TabSelectionEditorMenuItem> mVisibleActions = new ArraySet<>();

    /**
     * {@link ListMenuButton} for showing the {@link TabSelectionEditorMenu}.
     */
    private ListMenuButton mMenuButton;
    private LinearLayout.LayoutParams mActionViewParams;

    private Context mContext;
    private ActionViewLayoutDelegate mDelegate;
    private boolean mHasMenuOnlyItems;

    /**
     * Delegate updates in response to which action views are visible.
     */
    public interface ActionViewLayoutDelegate {
        /**
         * @param visibleActions the list of {@link TabSelectionEditorMenuItem}s with visible action
         * views.
         */
        public void setVisibleActionViews(Set<TabSelectionEditorMenuItem> visibleActions);
    }

    public TabSelectionEditorActionViewLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mActionViewParams = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT, 0.0f);
        mActionViewParams.gravity = Gravity.CENTER_VERTICAL;
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMenuButton = findViewById(R.id.list_menu_button);
        mMenuButton.tryToFitLargestItem(true);
    }

    @VisibleForTesting
    ListMenuButton getListMenuButtonForTesting() {
        return mMenuButton;
    }

    /**
     * @param delegate for handling menu button presses.
     */
    public void setListMenuButtonDelegate(ListMenuButtonDelegate delegate) {
        mMenuButton.setDelegate(delegate);
    }

    /**
     * @param delegate the delegate to notify of action view changes.
     */
    public void setActionViewLayoutDelegate(ActionViewLayoutDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * @param hasMenuOnlyItems whether the menu has items which are only shown in the menu.
     */
    public void setHasMenuOnlyItems(boolean hasMenuOnlyItems) {
        mHasMenuOnlyItems = hasMenuOnlyItems;
        update();
    }

    /**
     * @param menuItem a menu item with an action view to attempt to show prioritized by insertion
     * order.
     */
    public void add(TabSelectionEditorMenuItem menuItem) {
        assert menuItem.getActionView() != null;

        mMenuItemsWithActionView.add(menuItem);
        update();
    }

    /**
     * Clears the action views from this layout.
     */
    public void clear() {
        removeAllActionViews();
        mMenuItemsWithActionView.clear();
        mHasMenuOnlyItems = false;
        mMenuButton.setVisibility(View.GONE);
        update();
    }

    /**
     * Dismisses the menu.
     */
    public void dismissMenu() {
        mMenuButton.dismiss();
    }

    private void removeAllActionViews() {
        for (TabSelectionEditorMenuItem menuItem : mMenuItemsWithActionView) {
            final View actionView = menuItem.getActionView();
            if (this == actionView.getParent()) {
                removeView(menuItem.getActionView());
            }
        }
    }

    private boolean isUsingTabSelectionEditorV2Features() {
        return TabUiFeatureUtilities.isTabSelectionEditorV2Enabled(mContext) && mDelegate != null
                && (!mMenuItemsWithActionView.isEmpty() || mHasMenuOnlyItems);
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (!isUsingTabSelectionEditorV2Features()) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            final int width = getMeasuredWidth();

            if (getChildCount() < 3 || !(getChildAt(1) instanceof ButtonCompat)) return;

            // Child 1 will be the button.
            getChildAt(1).measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
            int requiredWidth =
                    getPaddingLeft() + getPaddingRight() + getChildAt(1).getMeasuredWidth();
            // Make the number roll view use the remaining space.
            makeNumberRollViewFill(MathUtils.clamp(width - requiredWidth, 0, width));
            // Get the final measurement.
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        // Get empty size without spacer and action views.
        removeAllActionViews();
        mMenuButton.setVisibility(View.VISIBLE);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        final int width = getMeasuredWidth();

        // Calculate the remaining room used by the menu, number roll view, etc.
        int usedRoom = getPaddingLeft() + getPaddingRight();
        int requiredWidth = usedRoom;
        for (int i = 0; i < getChildCount(); i++) {
            // NumberRollView size is dynamically restricted.
            if (getChildAt(i) instanceof NumberRollView) continue;

            usedRoom += getChildAt(i).getMeasuredWidth();
        }

        mVisibleActions.clear();

        // Add all action views leaving room using the remaining room.
        boolean allActionViewsShown = true;
        for (TabSelectionEditorMenuItem menuItem : mMenuItemsWithActionView) {
            final View actionView = menuItem.getActionView();
            actionView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);
            final int actionViewWidth = actionView.getMeasuredWidth();
            if (usedRoom + actionViewWidth > width || !allActionViewsShown) {
                // The ActionView was removed. Ensure it still has a LayoutParams.
                actionView.setLayoutParams(mActionViewParams);
                allActionViewsShown = false;
                continue;
            }

            // Add views in front of the menu button.
            addView(actionView, getChildCount() - 1, mActionViewParams);
            mVisibleActions.add(menuItem);
            usedRoom += actionViewWidth;
            requiredWidth += actionViewWidth;
        }
        mDelegate.setVisibleActionViews(mVisibleActions);
        if (mHasMenuOnlyItems || !allActionViewsShown) {
            mMenuButton.setVisibility(View.VISIBLE);
            requiredWidth += mMenuButton.getMeasuredWidth();
        } else {
            mMenuButton.setVisibility(View.GONE);
        }

        // Make the number roll view use the remaining space.
        makeNumberRollViewFill(MathUtils.clamp(width - requiredWidth, 0, width));

        // Get the final measurement.
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    private void makeNumberRollViewFill(int maxWidth) {
        View firstView = getChildAt(0);
        if (firstView instanceof NumberRollView) {
            NumberRollView numberRollView = (NumberRollView) firstView;
            LinearLayout.LayoutParams params =
                    (LinearLayout.LayoutParams) numberRollView.getLayoutParams();
            params.width = maxWidth;
            numberRollView.setLayoutParams(params);
        }
    }

    private void update() {
        measure(View.MeasureSpec.EXACTLY, View.MeasureSpec.AT_MOST);
    }
}
