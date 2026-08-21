// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Handler;
import android.os.Looper;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabmodel.TabGroupObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListItemAnimator;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Draws a vertical line to the side of tab items that belong to a tab group. */
@NullMarked
class VerticalTabGroupSpineDecoration extends RecyclerView.ItemDecoration {
    // TabListItemAnimator sequences removals before moves, so the spine's collapse duration
    // must account for both.
    private static final long COLLAPSE_DURATION_MS =
            TabListItemAnimator.DEFAULT_REMOVE_DURATION + TabListItemAnimator.DEFAULT_MOVE_DURATION;
    private static final long EXPAND_DURATION_MS = TabListItemAnimator.DEFAULT_MOVE_DURATION;

    private final TabListModel mModel;
    private final TabModelSelector mTabModelSelector;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final Runnable mInvalidationTrigger;
    private final Set<Token> mDrawnGroups = new HashSet<>();
    private final Set<Token> mCollapsingOrExpandingGroupIds = new HashSet<>();
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF mRectF = new RectF();
    private final int mSpineRadius;
    private final float mSpineWidth;
    private final int mMarginBottom;
    private final List<View> mSortedChildren = new ArrayList<>();
    private final ChildPositionComparator mChildPositionComparator = new ChildPositionComparator();
    private final GroupInfo mCurrentGroupInfo = new GroupInfo();

    private @Nullable TabModel mCurrentTabModel;

    private final TabGroupObserver mTabGroupObserver =
            new TabGroupObserver() {
                @Override
                public void didChangeTabGroupColor(
                        Token tabGroupId, @TabGroupColorId int newColor) {
                    mInvalidationTrigger.run();
                }

                @Override
                public void didChangeTabGroupCollapsed(
                        Token tabGroupId, boolean isCollapsed, boolean animate) {
                    long durationMs = isCollapsed ? COLLAPSE_DURATION_MS : EXPAND_DURATION_MS;

                    mCollapsingOrExpandingGroupIds.add(tabGroupId);
                    mHandler.postDelayed(
                            () -> {
                                mCollapsingOrExpandingGroupIds.remove(tabGroupId);
                                mInvalidationTrigger.run();
                            },
                            durationMs);
                }
            };

    /**
     * Constructor for {@link VerticalTabGroupSpineDecoration}.
     *
     * @param context The {@link Context} used to retrieve dimension resources.
     * @param invalidationTrigger A {@link Runnable} triggered to invalidate the recycler view.
     * @param model The {@link TabListModel} containing tab properties.
     * @param tabModelSelector The {@link TabModelSelector} used to fetch tab models.
     */
    public VerticalTabGroupSpineDecoration(
            Context context,
            Runnable invalidationTrigger,
            TabListModel model,
            TabModelSelector tabModelSelector) {
        mInvalidationTrigger = invalidationTrigger;
        mModel = model;
        mTabModelSelector = tabModelSelector;
        Resources res = context.getResources();
        mSpineWidth = res.getDimensionPixelSize(R.dimen.vertical_tab_spine_width);
        mSpineRadius = res.getDimensionPixelSize(R.dimen.vertical_tab_spine_radius);
        boolean isTablet = VerticalTabUtils.isTablet(context);
        mMarginBottom =
                res.getDimensionPixelSize(
                        isTablet
                                ? R.dimen.vertical_tab_item_margin_bottom_tablet
                                : R.dimen.vertical_tab_item_margin_bottom);

        mCurrentTabModelObserver = this::onCurrentTabModelChanged;
        mTabModelSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndCallIfNonNull(mCurrentTabModelObserver);
    }

    @Override
    public void onDraw(Canvas c, RecyclerView parent, RecyclerView.State state) {
        TabModel tabModel = mTabModelSelector.getCurrentModel();
        if (tabModel == null || mModel.isEmpty()) return;

        boolean isIncognito = tabModel.isIncognitoBranded();
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        float left;
        float right;
        if (isRtl) {
            right = parent.getWidth() - parent.getPaddingRight();
            left = right - mSpineWidth;
        } else {
            left = parent.getPaddingLeft();
            right = left + mSpineWidth;
        }

        mDrawnGroups.clear();
        boolean anyTabBeingDragged = sortAndCheckDragging(parent);

        float deferredTop = 0;
        float deferredBottom = 0;
        int deferredColor = 0;
        boolean hasDeferredSpine = false;

        int sortedChildCount = mSortedChildren.size();
        for (int i = 0; i < sortedChildCount; i++) {
            if (!tryUpdateGroupInfo(tabModel, parent, sortedChildCount, i)) continue;

            float top = calculateTop();
            float bottom = calculateBottom(parent, anyTabBeingDragged);
            if (bottom <= top) continue;

            int color = mCurrentGroupInfo.getColor(parent.getContext(), isIncognito, tabModel);
            // If the group is being dragged, defer its spine drawing to make it the most top.
            if (mCurrentGroupInfo.mGroupDraggingY != null) {
                deferredTop = top;
                deferredBottom = bottom;
                deferredColor = color;
                hasDeferredSpine = true;
                continue;
            }

            drawSpine(c, left, top, right, bottom, color);
        }

        if (hasDeferredSpine) {
            drawSpine(c, left, deferredTop, right, deferredBottom, deferredColor);
        }
    }

    public void destroy() {
        mHandler.removeCallbacksAndMessages(null);
        mTabModelSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeTabGroupObserver(mTabGroupObserver);
            mCurrentTabModel = null;
        }
    }

    private void drawSpine(Canvas c, float left, float top, float right, float bottom, int color) {
        mPaint.setColor(color);
        mRectF.set(left, top, right, bottom);
        c.drawRoundRect(mRectF, mSpineRadius, mSpineRadius, mPaint);
    }

    private void onCurrentTabModelChanged(@Nullable TabModel tabModel) {
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeTabGroupObserver(mTabGroupObserver);
        }
        mCurrentTabModel = tabModel;
        if (mCurrentTabModel != null) {
            mCurrentTabModel.addTabGroupObserver(mTabGroupObserver);
        }
    }

    /**
     * Collects and sorts the visible child views of the recycler view.
     *
     * @param parent The recycler view containing child views.
     * @return True if any of the collected visible tabs are currently being dragged.
     */
    private boolean sortAndCheckDragging(RecyclerView parent) {
        boolean anyVisibleTabDragging = false;
        mSortedChildren.clear();
        int childCount = parent.getChildCount();
        for (int i = 0; i < childCount; i++) {
            View child = parent.getChildAt(i);
            int pos = parent.getChildAdapterPosition(child);
            if (pos != RecyclerView.NO_POSITION && pos < mModel.size()) {
                mSortedChildren.add(child);

                // Check if this child is being dragged
                PropertyModel model = mModel.get(pos).model;
                if (model != null && model.get(TabProperties.DRAGGING_Y) != null) {
                    anyVisibleTabDragging = true;
                }
            }
        }

        // RecyclerView heavily recycles and reorders views during fast scrolling and
        // group expand/collapse animations. Because of this, `parent.getChildAt(i)`
        // often returns views out of their logical adapter order (e.g. newly bound
        // child tabs are appended to the end of the ViewGroup). We must sort the
        // attached views by their adapter position to guarantee we can accurately
        // identify the "first" and "last" visible tabs for each group and properly
        // connect the spine to subsequent items.
        mChildPositionComparator.setParent(parent);
        mSortedChildren.sort(mChildPositionComparator);
        mChildPositionComparator.setParent(null);
        return anyVisibleTabDragging;
    }

    /**
     * Calculates the top boundary of the spine.
     *
     * <ul>
     *   <li>Case 1 (Collapsing/Expanding), Case 5 (External shifts), Case 6 (All other cases): Top
     *       translates dynamically so the spine moves smoothly with the group.
     *   <li>Case 3 (Group drag): Use groupDraggingY and header's bottom.
     *   <li>Case 4 (Internal child drag): Top is static to keep the spine anchored.
     * </ul>
     */
    private float calculateTop() {
        View view = mCurrentGroupInfo.mView;
        assert view != null;
        float transY =
                mCurrentGroupInfo.mIsChildBeingDraggedInternally ? 0 : view.getTranslationY();
        if (mCurrentGroupInfo.mIsHeader) {
            Float draggingY = mCurrentGroupInfo.mGroupDraggingY;
            // For a group header, start the line right below the header card (plus draggingY or
            // translationY, and margin).
            return view.getBottom() + ((draggingY != null) ? draggingY : transY) + mMarginBottom;
        }

        // For a normal child tab (meaning the group header scrolled off-screen),
        // start the vertical line directly from the top edge of this card.
        return view.getTop() + transY;
    }

    private float calculateBottom(RecyclerView parent, boolean anyTabBeingDragged) {
        // Case 1 (Collapsing/Expanding): Connects to the next sibling's translated top.
        if (mCurrentGroupInfo.mIsBeingCollapsedOrExpanded
                && mCurrentGroupInfo.mNextSiblingView != null) {
            return mCurrentGroupInfo.mNextSiblingView.getTop()
                    + mCurrentGroupInfo.mNextSiblingView.getTranslationY()
                    - mMarginBottom;
        }

        // Case 2 (Collapsed): Hide spine
        if (mCurrentGroupInfo.mIsCollapsed) {
            return Integer.MIN_VALUE;
        }

        View lastGroupView = mCurrentGroupInfo.mLastGroupView;
        assert lastGroupView != null;
        // Case 3 (Group drag): If group header is being dragged, use draggingY.
        if (mCurrentGroupInfo.mGroupDraggingY != null) {
            return lastGroupView.getBottom() + mCurrentGroupInfo.mGroupDraggingY;
        }

        // Case 4 (Internal child drag): Anchored to the last static tab's bottom.
        if (mCurrentGroupInfo.mIsChildBeingDraggedInternally) {
            return lastGroupView.getBottom();
        }

        // Case 5 (External shifts): Stays at the last tab's translated bottom.
        float targetBottom = lastGroupView.getBottom() + lastGroupView.getTranslationY();
        boolean hasSubsequentItems =
                parent.getChildAdapterPosition(lastGroupView) < mModel.size() - 1;
        View lastStableGroupView = mCurrentGroupInfo.mLastStableGroupView;
        if (anyTabBeingDragged || hasSubsequentItems || lastStableGroupView == null) {
            return targetBottom;
        }

        // Case 6 (All other cases): Animates to the last tab's bottom using alpha.
        float startBottom = lastStableGroupView.getBottom() + lastStableGroupView.getTranslationY();
        return startBottom + ((targetBottom - startBottom) * lastGroupView.getAlpha());
    }

    private boolean tryUpdateGroupInfo(
            TabModel tabModel, RecyclerView parent, int parentChildCount, int currentIndex) {
        View view = mSortedChildren.get(currentIndex);
        PropertyModel model = mModel.get(parent.getChildAdapterPosition(view)).model;
        if (model == null) return false;

        Token headerId = model.get(TabProperties.TAB_GROUP_HEADER_ID);
        boolean isHeader = headerId != null;
        Token groupId = isHeader ? headerId : model.get(TabProperties.TAB_GROUP_ID);
        if (groupId == null || !mDrawnGroups.add(groupId)) {
            return false;
        }

        mCurrentGroupInfo.reset(groupId, view);
        mCurrentGroupInfo.mColorId = model.get(TabProperties.TAB_GROUP_CARD_COLOR);
        mCurrentGroupInfo.mIsHeader = isHeader;
        mCurrentGroupInfo.mIsBeingCollapsedOrExpanded =
                mCollapsingOrExpandingGroupIds.contains(groupId);
        mCurrentGroupInfo.mLastStableGroupView = view;

        Float childDraggingY = !isHeader ? model.get(TabProperties.DRAGGING_Y) : null;
        for (int j = currentIndex + 1; j < parentChildCount; j++) {
            View v = mSortedChildren.get(j);
            PropertyModel nextModel = mModel.get(parent.getChildAdapterPosition(v)).model;
            Token nextTabId =
                    (nextModel == null) ? null : nextModel.get(TabProperties.TAB_GROUP_ID);
            if (groupId.equals(nextTabId)) {
                if (v.getAlpha() >= 1f) {
                    mCurrentGroupInfo.mLastStableGroupView = v;
                }
                mCurrentGroupInfo.mLastGroupView = v;

                Float draggingY = nextModel.get(TabProperties.DRAGGING_Y);
                if (draggingY != null) {
                    childDraggingY = draggingY;
                }
            } else {
                mCurrentGroupInfo.mNextSiblingView = v;
                break;
            }
        }

        mCurrentGroupInfo.mIsChildBeingDraggedInternally = childDraggingY != null;
        if (isHeader) {
            mCurrentGroupInfo.mIsCollapsed = model.get(TabProperties.IS_COLLAPSED);
            mCurrentGroupInfo.mGroupDraggingY = model.get(TabProperties.DRAGGING_Y);

            // If there is only one child, and the child is being dragged, then the whole tab group
            // is being dragged.
            if (mCurrentGroupInfo.mIsChildBeingDraggedInternally
                    && tabModel.getTabCountForGroup(groupId) <= 1) {
                mCurrentGroupInfo.mIsChildBeingDraggedInternally = false;
                mCurrentGroupInfo.mGroupDraggingY = childDraggingY;
            }
        }
        return true;
    }

    private static class GroupInfo {
        @Nullable View mView;
        @Nullable Integer mColorId;
        boolean mIsHeader;
        boolean mIsBeingCollapsedOrExpanded;
        boolean mIsCollapsed;
        // A non-null draggingY indicates the entire group is being dragged.
        @Nullable Float mGroupDraggingY;
        boolean mIsChildBeingDraggedInternally;

        @Nullable View mLastGroupView;
        @Nullable View mLastStableGroupView;
        @Nullable View mNextSiblingView;

        private @Nullable Token mGroupId;

        void reset(Token groupId, View view) {
            mGroupId = groupId;
            mView = view;
            mColorId = null;
            mIsHeader = false;
            mIsBeingCollapsedOrExpanded = false;
            mIsCollapsed = false;
            mGroupDraggingY = null;
            mIsChildBeingDraggedInternally = false;

            mLastGroupView = view;
            mLastStableGroupView = view;
            mNextSiblingView = null;
        }

        private int getColor(Context context, boolean isIncognito, TabModel tabModel) {
            assert mGroupId != null;
            int colorId =
                    mColorId != null ? mColorId : tabModel.getTabGroupColorWithFallback(mGroupId);
            return TabGroupColorPickerUtils.getTabGroupColorPickerItemColor(
                    context, colorId, isIncognito);
        }
    }

    private static class ChildPositionComparator implements Comparator<View> {
        private @Nullable RecyclerView mParent;

        public void setParent(@Nullable RecyclerView parent) {
            mParent = parent;
        }

        @Override
        public int compare(View v1, View v2) {
            assert mParent != null;
            return Integer.compare(
                    mParent.getChildAdapterPosition(v1), mParent.getChildAdapterPosition(v2));
        }
    }
}
