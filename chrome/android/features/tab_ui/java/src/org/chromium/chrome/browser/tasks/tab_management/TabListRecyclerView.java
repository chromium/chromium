// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Pair;
import android.view.MotionEvent;
import android.view.View;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.RunOnNextLayout;
import org.chromium.chrome.browser.hub.RunOnNextLayoutDelegate;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** A custom RecyclerView implementation for the tab grid, to handle show/hide logic in class. */
class TabListRecyclerView extends RecyclerView
        implements TabListMediator.TabGridAccessibilityHelper, RunOnNextLayout {
    private boolean mBlockTouchInput;
    private ImageView mShadowImageView;
    // Null unless item animations are disabled.
    @Nullable private RecyclerView.ItemAnimator mDisabledAnimatorHolder;

    private final RunOnNextLayoutDelegate mRunOnNextLayoutDelegate;

    private TabListItemAnimator mTabListItemAnimator;

    /** Basic constructor to use during inflation from xml. */
    public TabListRecyclerView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
        mRunOnNextLayoutDelegate = new RunOnNextLayoutDelegate(this);
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        runOnNextLayoutRunnables();
    }

    @Override
    public void runOnNextLayout(Runnable runnable) {
        mRunOnNextLayoutDelegate.runOnNextLayout(runnable);
    }

    @Override
    public void runOnNextLayoutRunnables() {
        mRunOnNextLayoutDelegate.runOnNextLayoutRunnables();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent e) {
        if (mBlockTouchInput) return true;

        return super.dispatchTouchEvent(e);
    }

    /**
     * Set whether to block touch inputs. For example, during an animated transition the
     * TabListRecyclerView may still be visible, but interacting with it could trigger repeat
     * animations or unexpected state changes.
     *
     * @param blockTouchInput Whether the touch inputs should be blocked.
     */
    void setBlockTouchInput(boolean blockTouchInput) {
        mBlockTouchInput = blockTouchInput;
    }

    void setDisableItemAnimations(boolean disable) {
        if (disable) {
            ItemAnimator animator = getItemAnimator();
            if (animator == null) return;

            mDisabledAnimatorHolder = animator;
            setItemAnimator(null);
        } else if (mDisabledAnimatorHolder != null) {
            setItemAnimator(mDisabledAnimatorHolder);
            mDisabledAnimatorHolder = null;
        }
    }

    void setupCustomItemAnimator() {
        // Kill switch is defaulted to enabled and can be shut off to false via config if issues are
        // discovered.
        if (ChromeFeatureList.sGtsCloseTabAnimationKillSwitch.isEnabled()) {
            if (mTabListItemAnimator == null) {
                mTabListItemAnimator = new TabListItemAnimator();
                setItemAnimator(mTabListItemAnimator);
            }
        }
    }

    /**
     * @param tabIndex The index in the RecyclerView of the tab.
     * @param tabId The tab ID of the tab.
     * @return The {@link Rect} of the thumbnail of the tab in global coordinates.
     */
    @NonNull
    Rect getRectOfTabThumbnail(int tabIndex, int tabId) {
        SimpleRecyclerViewAdapter.ViewHolder holder =
                (SimpleRecyclerViewAdapter.ViewHolder) findViewHolderForAdapterPosition(tabIndex);
        Rect rect = new Rect();
        if (holder == null || tabIndex == TabModel.INVALID_TAB_INDEX) return rect;
        assert holder.model.get(TabProperties.TAB_ID) == tabId;
        ViewLookupCachingFrameLayout root = (ViewLookupCachingFrameLayout) holder.itemView;
        View v = root.fastFindViewById(R.id.tab_thumbnail);
        if (v != null) v.getGlobalVisibleRect(rect);
        return rect;
    }

    /**
     * @param selectedTabIndex The index in the RecyclerView of the selected tab.
     * @param selectedTabId The tab ID of the selected tab.
     * @return The {@link Rect} of the thumbnail of the current tab, relative to the
     *         {@link TabListRecyclerView} coordinates.
     */
    @Nullable
    Rect getRectOfCurrentThumbnail(int selectedTabIndex, int selectedTabId) {
        SimpleRecyclerViewAdapter.ViewHolder holder =
                (SimpleRecyclerViewAdapter.ViewHolder)
                        findViewHolderForAdapterPosition(selectedTabIndex);
        if (holder == null || selectedTabIndex == TabModel.INVALID_TAB_INDEX) return null;
        assert holder.model.get(TabProperties.TAB_ID) == selectedTabId;
        ViewLookupCachingFrameLayout root = (ViewLookupCachingFrameLayout) holder.itemView;
        return getRectOfComponent(root.fastFindViewById(R.id.tab_thumbnail));
    }

    private Rect getRectOfComponent(View v) {
        // If called before a thumbnail view exists or for list view then exit with null.
        if (v == null) return null;

        Rect recyclerViewRect = new Rect();
        Rect componentRect = new Rect();
        getGlobalVisibleRect(recyclerViewRect);
        v.getGlobalVisibleRect(componentRect);

        // Get the relative position.
        componentRect.offset(-recyclerViewRect.left, -recyclerViewRect.top);
        return componentRect;
    }

    /**
     * @return the position and offset of the first visible element in the list.
     */
    @NonNull
    RecyclerViewPosition getRecyclerViewPosition() {
        LinearLayoutManager layoutManager = (LinearLayoutManager) getLayoutManager();
        int position = layoutManager.findFirstVisibleItemPosition();
        int offset = 0;
        if (position != RecyclerView.NO_POSITION) {
            View firstVisibleView = layoutManager.findViewByPosition(position);
            if (firstVisibleView != null) {
                offset = firstVisibleView.getTop();
            }
        }
        return new RecyclerViewPosition(position, offset);
    }

    /**
     * @param recyclerViewPosition the position and offset to scroll the recycler view to.
     */
    void setRecyclerViewPosition(@NonNull RecyclerViewPosition recyclerViewPosition) {
        ((LinearLayoutManager) getLayoutManager())
                .scrollToPositionWithOffset(
                        recyclerViewPosition.getPosition(), recyclerViewPosition.getOffset());
    }

    /**
     * This method finds out the index of the hovered tab's viewHolder in {@code recyclerView}.
     *
     * @param recyclerView The recyclerview that owns the tabs' viewHolders.
     * @param view The view of the selected tab.
     * @param dX The X offset of the selected tab.
     * @param dY The Y offset of the selected tab.
     * @param threshold The percentage area threshold as a decimal to judge whether two tabs are
     *     overlapped.
     * @return The index of the hovered tab.
     */
    static int getHoveredTabIndex(
            RecyclerView recyclerView, View view, float dX, float dY, float threshold) {
        for (int i = 0; i < recyclerView.getAdapter().getItemCount(); i++) {
            ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(i);
            if (viewHolder == null) continue;
            View child = viewHolder.itemView;
            if (child.getLeft() == view.getLeft() && child.getTop() == view.getTop()) {
                continue;
            }
            if (isOverlap(child, view, (int) dX, (int) dY, threshold)) {
                return i;
            }
        }
        return -1;
    }

    private static boolean isOverlap(View child, View view, int dX, int dY, float threshold) {
        Rect childRect =
                new Rect(
                        child.getLeft(),
                        child.getTop(),
                        child.getLeft() + child.getWidth(),
                        child.getTop() + child.getHeight());
        Rect viewRect =
                new Rect(
                        view.getLeft() + dX,
                        view.getTop() + dY,
                        view.getLeft() + view.getWidth() + dX,
                        view.getTop() + view.getHeight() + dY);

        // Reuse the child rect as the overlap when choosing if the overlap qualifies for a merge.
        if (!childRect.setIntersect(childRect, viewRect)) return false;

        return childRect.width() * childRect.height()
                > viewRect.width() * viewRect.height() * threshold;
    }

    // TabGridAccessibilityHelper implementation.
    // TODO(crbug.com/40110745): Add e2e tests for implementation below when tab grid is enabled for
    // accessibility mode.
    @Override
    @SuppressLint("NewApi")
    public List<AccessibilityAction> getPotentialActionsForView(View view) {
        List<AccessibilityAction> actions = new ArrayList<>();
        int position = getChildAdapterPosition(view);
        if (position == -1) {
            return actions;
        }
        GridLayoutManager layoutManager = (GridLayoutManager) getLayoutManager();
        int spanCount = layoutManager.getSpanCount();
        Context context = getContext();

        AccessibilityAction leftAction =
                new AccessibilityNodeInfo.AccessibilityAction(
                        R.id.move_tab_left,
                        context.getString(R.string.accessibility_tab_movement_left));
        AccessibilityAction rightAction =
                new AccessibilityNodeInfo.AccessibilityAction(
                        R.id.move_tab_right,
                        context.getString(R.string.accessibility_tab_movement_right));
        AccessibilityAction topAction =
                new AccessibilityNodeInfo.AccessibilityAction(
                        R.id.move_tab_up,
                        context.getString(R.string.accessibility_tab_movement_up));
        AccessibilityAction downAction =
                new AccessibilityNodeInfo.AccessibilityAction(
                        R.id.move_tab_down,
                        context.getString(R.string.accessibility_tab_movement_down));
        actions.addAll(
                new ArrayList<>(Arrays.asList(leftAction, rightAction, topAction, downAction)));

        // Decide whether the tab can be moved left/right based on current index and span count.
        if (position % spanCount == 0) {
            actions.remove(leftAction);
        } else if (position % spanCount == spanCount - 1) {
            actions.remove(rightAction);
        }
        // Cannot move up if the tab is in the first row.
        if (position < spanCount) {
            actions.remove(topAction);
        }
        // Cannot move down if current tab is the last X tab where X is the span count.
        if (getSwappableItemCount() - position <= spanCount) {
            actions.remove(downAction);
        }
        // Cannot move the last tab to its right.
        if (position == getSwappableItemCount() - 1) {
            actions.remove(rightAction);
        }
        return actions;
    }

    private int getSwappableItemCount() {
        int count = 0;
        for (int i = 0; i < getAdapter().getItemCount(); i++) {
            if (getAdapter().getItemViewType(i) == TabProperties.UiType.TAB) count++;
        }
        return count;
    }

    @Override
    public Pair<Integer, Integer> getPositionsOfReorderAction(View view, int action) {
        int currentPosition = getChildAdapterPosition(view);
        GridLayoutManager layoutManager = (GridLayoutManager) getLayoutManager();
        int spanCount = layoutManager.getSpanCount();
        int targetPosition = -1;

        if (action == R.id.move_tab_left) {
            targetPosition = currentPosition - 1;
        } else if (action == R.id.move_tab_right) {
            targetPosition = currentPosition + 1;
        } else if (action == R.id.move_tab_up) {
            targetPosition = currentPosition - spanCount;
        } else if (action == R.id.move_tab_down) {
            targetPosition = currentPosition + spanCount;
        }
        return new Pair<>(currentPosition, targetPosition);
    }

    @Override
    public boolean isReorderAction(int action) {
        return action == R.id.move_tab_left
                || action == R.id.move_tab_right
                || action == R.id.move_tab_up
                || action == R.id.move_tab_down;
    }
}
