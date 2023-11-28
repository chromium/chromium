// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;

import androidx.collection.ArraySet;

import org.chromium.base.MathUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.NumberRollView;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;

import java.util.ArrayList;
import java.util.Set;

/**
 * A {@link LinearLayout} that displays only the TabListEditorMenuItem ActionViews that fit in
 * the space it contains. Managed by a {@link TabListEditorMenu}.
 */
public class TabListEditorActionViewLayout extends LinearLayout {
    /** All {@link TabListEditoreMenuItem} action views with menu items. */
    private final ArrayList<TabListEditorMenuItem> mMenuItemsWithActionView =
            new ArrayList<>();

    /** The {@link TabListEditoreMenuItem}s with visible action views. */
    private final Set<TabListEditorMenuItem> mVisibleActions = new ArraySet<>();

    /** {@link ListMenuButton} for showing the {@link TabListEditorMenu}. */
    private ListMenuButton mMenuButton;

    private LinearLayout.LayoutParams mActionViewParams;

    private Context mContext;
    private ActionViewLayoutDelegate mDelegate;
    private boolean mHasMenuOnlyItems;

    /** Delegate updates in response to which action views are visible. */
    public interface ActionViewLayoutDelegate {
        /**
         * @param visibleActions the list of {@link TabListEditorMenuItem}s with visible action
         * views.
         */
        public void setVisibleActionViews(Set<TabListEditorMenuItem> visibleActions);
    }

    public TabListEditorActionViewLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mActionViewParams =
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT,
                        0.0f);
        mActionViewParams.gravity = Gravity.CENTER_VERTICAL;
        mContext = context;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMenuButton = findViewById(R.id.list_menu_button);
        mMenuButton.tryToFitLargestItem(true);
    }

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
    public void add(TabListEditorMenuItem menuItem) {
        assert menuItem.getActionView() != null;

        mMenuItemsWithActionView.add(menuItem);
        update();
    }

    /** Clears the action views from this layout. */
    public void clear() {
        removeAllActionViews();
        mMenuItemsWithActionView.clear();
        mHasMenuOnlyItems = false;
        mMenuButton.setVisibility(View.GONE);
        update();
    }

    /** Dismisses the menu. */
    public void dismissMenu() {
        mMenuButton.dismiss();
    }

    private void removeAllActionViews() {
        for (TabListEditorMenuItem menuItem : mMenuItemsWithActionView) {
            final View actionView = menuItem.getActionView();
            if (this == actionView.getParent()) {
                removeView(menuItem.getActionView());
            }
        }
    }

    @Override
    public void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // Get empty size without action views.
        removeAllActionViews();
        mMenuButton.setVisibility(View.VISIBLE);
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        final int width = getMeasuredWidth();

        // The width that is required by visible views.
        int requiredWidth = getPaddingLeft() + getPaddingRight();
        // The width including the menu button assuming it is visible.
        int usedWidth = requiredWidth + mMenuButton.getMeasuredWidth();

        mVisibleActions.clear();

        // Add all action views that fit.
        boolean hasForcedAnyActionViewToMenu = false;
        final int childMeasureSpec =
                View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        for (TabListEditorMenuItem menuItem : mMenuItemsWithActionView) {
            final View actionView = menuItem.getActionView();
            actionView.measure(childMeasureSpec, childMeasureSpec);
            final int actionViewWidth = actionView.getMeasuredWidth();
            if (usedWidth + actionViewWidth > width || hasForcedAnyActionViewToMenu) {
                // The ActionView doesn't fit. Ensure it still has a LayoutParams.
                actionView.setLayoutParams(mActionViewParams);
                hasForcedAnyActionViewToMenu = true;
                continue;
            }

            // Add views in front of the menu button.
            addView(actionView, getChildCount() - 1, mActionViewParams);
            mVisibleActions.add(menuItem);
            usedWidth += actionViewWidth;
            requiredWidth += actionViewWidth;
        }
        if (mDelegate != null) {
            // Any items in mVisibleActions will appear in the Toolbar. The remaining items will be
            // forced into the overflow menu.
            mDelegate.setVisibleActionViews(mVisibleActions);
        }
        if (mHasMenuOnlyItems || hasForcedAnyActionViewToMenu) {
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
        int widthMeasureSpec =
                View.MeasureSpec.makeMeasureSpec(getMeasuredWidth(), View.MeasureSpec.AT_MOST);
        int heightMeasureSpec =
                View.MeasureSpec.makeMeasureSpec(getMeasuredHeight(), View.MeasureSpec.EXACTLY);
        measure(widthMeasureSpec, heightMeasureSpec);
    }
}
