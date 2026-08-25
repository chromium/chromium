// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.graphics.Canvas;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;

/** Encapsulates the UI logic for the Recent Tabs screen. */
@NullMarked
public class RecentTabsCoordinator
        implements TouchEnabledDelegate, InvalidationAwareThumbnailProvider {

    private final Activity mActivity;
    private final ViewGroup mView;
    private final View mRecentTabsRoot;
    private final ExpandableListView mListView;
    private final ContextMenuManager mContextMenuManager;
    private final RecentTabsRowAdapter mAdapter;
    private final RecentTabsManager mRecentTabsManager;
    private final NonNullObservableSupplier<Integer> mTabStripHeightSupplier;
    private final Callback<Integer> mTabStripHeightChangeCallback;
    private @Nullable EdgeToEdgePadAdjuster mPadAdjuster;

    private boolean mSnapshotContentChanged;
    private int mSnapshotListPosition;
    private int mSnapshotListTop;
    private int mSnapshotWidth;
    private int mSnapshotHeight;
    private @Nullable String mTargetSessionTag;
    private boolean mIsTouchEnabled = true;
    private boolean mIsDestroyed;

    /**
     * Constructs a new {@link RecentTabsCoordinator}.
     *
     * @param activity The activity this coordinator is scoped to.
     * @param recentTabsManager The manager providing domain data and operations for recent tabs.
     * @param navigationDelegate Delegate for handling navigation actions from the UI.
     * @param tabStripHeightSupplier Supplier for the dynamic height of the tab strip.
     * @param edgeToEdgeSupplier Supplier for the edge-to-edge controller.
     * @param parent Optional parent view group used to inflate layout params.
     */
    public RecentTabsCoordinator(
            Activity activity,
            RecentTabsManager recentTabsManager,
            NativePageNavigationDelegate navigationDelegate,
            NonNullObservableSupplier<Integer> tabStripHeightSupplier,
            MonotonicObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @Nullable ViewGroup parent) {
        mActivity = activity;
        mRecentTabsManager = recentTabsManager;

        mRecentTabsManager.setUpdatedCallback(this::onUpdated);
        LayoutInflater inflater = LayoutInflater.from(activity);

        mContextMenuManager =
                new ContextMenuManager(
                        navigationDelegate, this, mActivity::closeContextMenu, "RecentTabs");

        mView = (ViewGroup) inflater.inflate(R.layout.recent_tabs_page, parent, false);
        if (parent == null) {
            // Restore match_parent since there is no parent injected here.
            mView.setLayoutParams(
                    new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
        }
        mRecentTabsRoot = mView.findViewById(R.id.recent_tabs_root);
        mListView = mView.findViewById(R.id.odp_listview);
        mAdapter = new RecentTabsRowAdapter(activity, recentTabsManager, mContextMenuManager);
        mListView.setAdapter(mAdapter);
        mListView.setItemsCanFocus(true);
        mListView.setOnChildClickListener(this::onChildClick);
        mListView.setGroupIndicator(null);
        mListView.setOnGroupCollapseListener(this::onGroupCollapse);
        mListView.setOnGroupExpandListener(this::onGroupExpand);
        mListView.setOnCreateContextMenuListener(this::onCreateContextMenu);

        mPadAdjuster =
                EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                        mListView, edgeToEdgeSupplier);
        mTabStripHeightSupplier = tabStripHeightSupplier;
        mTabStripHeightChangeCallback =
                height -> {
                    mView.setPadding(
                            mView.getPaddingLeft(),
                            height,
                            mView.getPaddingRight(),
                            mView.getPaddingBottom());
                };
        mTabStripHeightSupplier.addSyncObserverAndPostIfNonNull(mTabStripHeightChangeCallback);
    }

    /**
     * Updates the top/bottom margins and vertical translation of the recent tabs root view.
     *
     * @param topMargin Top margin in pixels.
     * @param bottomMargin Bottom margin in pixels.
     * @param translationY Vertical translation in pixels.
     */
    public void updateMargins(int topMargin, int bottomMargin, int translationY) {
        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) mRecentTabsRoot.getLayoutParams();
        if (layoutParams.topMargin != topMargin || layoutParams.bottomMargin != bottomMargin) {
            layoutParams.topMargin = topMargin;
            layoutParams.bottomMargin = bottomMargin;
            mRecentTabsRoot.setLayoutParams(layoutParams);
        }
        mRecentTabsRoot.setTranslationY(translationY);
    }

    /** Returns the top margin in pixels currently applied to the recent tabs root view. */
    public int getRootViewTopMargin() {
        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) mRecentTabsRoot.getLayoutParams();
        return layoutParams.topMargin;
    }

    @Override
    public boolean shouldCaptureThumbnail() {
        if (mView.getWidth() == 0 || mView.getHeight() == 0) {
            return false;
        }

        View topItem = mListView.getChildAt(0);
        return mSnapshotContentChanged
                || mSnapshotListPosition != mListView.getFirstVisiblePosition()
                || mSnapshotListTop != (topItem == null ? 0 : topItem.getTop())
                || mView.getWidth() != mSnapshotWidth
                || mView.getHeight() != mSnapshotHeight;
    }

    @Override
    public void captureThumbnail(Canvas canvas) {
        ViewUtils.captureBitmap(mView, canvas);
        mSnapshotContentChanged = false;
        mSnapshotListPosition = mListView.getFirstVisiblePosition();
        View topItem = mListView.getChildAt(0);
        mSnapshotListTop = topItem == null ? 0 : topItem.getTop();
        mSnapshotWidth = mView.getWidth();
        mSnapshotHeight = mView.getHeight();
    }

    Callback<Integer> getTabStripHeightChangeCallbackForTesting() {
        return mTabStripHeightChangeCallback;
    }

    /** Returns the root {@link ViewGroup} containing the Recent Tabs UI hierarchy. */
    public ViewGroup getView() {
        return mView;
    }

    ExpandableListView getListViewForTesting() {
        return mListView;
    }

    /** Destroys the coordinator, unregistering observers and freeing resources. */
    public void destroy() {
        mIsDestroyed = true;
        mTabStripHeightSupplier.removeObserver(mTabStripHeightChangeCallback);
        if (mPadAdjuster != null) {
            mPadAdjuster.destroy();
            mPadAdjuster = null;
        }
        mRecentTabsManager.destroy();
        mAdapter.notifyDataSetInvalidated();
        mListView.setAdapter((RecentTabsRowAdapter) null);
    }

    @VisibleForTesting
    boolean onChildClick(
            ExpandableListView parent,
            @Nullable View v,
            int groupPosition,
            int childPosition,
            long id) {
        if (!mIsTouchEnabled) return true;
        return mAdapter.getGroup(groupPosition).onChildClick(childPosition);
    }

    boolean performChildClickForTesting(int groupPosition, int childPosition) {
        return onChildClick(mListView, null, groupPosition, childPosition, 0);
    }

    @Override
    public void setTouchEnabled(boolean enabled) {
        mIsTouchEnabled = enabled;
    }

    private void onGroupExpand(int groupPosition) {
        mAdapter.getGroup(groupPosition).setCollapsed(false);
        mSnapshotContentChanged = true;
    }

    private void onGroupCollapse(int groupPosition) {
        mAdapter.getGroup(groupPosition).setCollapsed(true);
        mSnapshotContentChanged = true;
    }

    private void onUpdated() {
        if (mIsDestroyed) return;
        mAdapter.notifyDataSetChanged();
        for (int i = 0; i < mAdapter.getGroupCount(); i++) {
            if (mAdapter.getGroup(i).isCollapsed()) {
                mListView.collapseGroup(i);
            } else {
                mListView.expandGroup(i);
            }
        }
        mSnapshotContentChanged = true;
        scrollToTargetSession();
    }

    @VisibleForTesting
    void onCreateContextMenu(ContextMenu menu, View v, @Nullable ContextMenuInfo menuInfo) {
        // Would prefer to have this context menu view managed internal to RecentTabsGroupView.
        // Unfortunately, setting either onCreateContextMenuListener or onLongClickListener
        // disables the native onClick (expand/collapse) behaviour of the group view.

        // Due to issues with theming the android context menu,
        // we switch to using ContextMenuAdapter that is managed internally.
        // Due to the reason listed above, we use onCreateContextMenu to catch any long presses
        // from the user, and then keep a boolean internally to disable click events during a long
        // press.
        menu.clear();
        if (!(menuInfo instanceof ExpandableListView.ExpandableListContextMenuInfo info)) {
            return;
        }

        int type = ExpandableListView.getPackedPositionType(info.packedPosition);
        int groupPosition = ExpandableListView.getPackedPositionGroup(info.packedPosition);

        View anchorView = info.targetView;
        if (anchorView == null) return;

        if (groupPosition < 0 || groupPosition >= mAdapter.getGroupCount()) return;

        if (type == ExpandableListView.PACKED_POSITION_TYPE_GROUP) {
            mAdapter.getGroup(groupPosition).onCreateContextMenuForGroup(mActivity, anchorView);
        } else if (type == ExpandableListView.PACKED_POSITION_TYPE_CHILD) {
            int childPosition = ExpandableListView.getPackedPositionChild(info.packedPosition);
            mAdapter.getGroup(groupPosition)
                    .onCreateContextMenuForChild(childPosition, mActivity, anchorView);
        }
    }

    /**
     * Sets the foreign session tag to target and scroll to when data is loaded.
     *
     * @param sessionTag The tag of the foreign session to scroll to, or null to clear.
     */
    public void setTargetSessionTag(@Nullable String sessionTag) {
        mTargetSessionTag = sessionTag;
        scrollToTargetSession();
    }

    private void scrollToTargetSession() {
        if (mTargetSessionTag == null) return;

        final int groupPosition = mAdapter.getGroupPositionForForeignSession(mTargetSessionTag);
        if (groupPosition != -1) {
            mListView.expandGroup(groupPosition);
            // Post the scroll to selection. This is needed because expandGroup() requests a layout
            // pass asynchronously. Scrolling immediately would use the old collapsed heights.
            mListView.post(
                    () -> {
                        if (mIsDestroyed) return;
                        mListView.setSelectedGroup(groupPosition);
                    });
            mTargetSessionTag = null;
        }
    }
}
