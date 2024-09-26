// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ExpandableListView;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab_ui.InvalidationAwareThumbnailProvider;
import org.chromium.chrome.browser.ui.native_page.BasicSmoothTransitionDelegate;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.ViewUtils;

/**
 * The native recent tabs page. Lists recently closed tabs, open windows and tabs from the user's
 * synced devices, and snapshot documents sent from Chrome to Mobile in an expandable list view.
 */
public class RecentTabsPage
        implements NativePage,
                ExpandableListView.OnChildClickListener,
                ExpandableListView.OnGroupCollapseListener,
                ExpandableListView.OnGroupExpandListener,
                RecentTabsManager.UpdatedCallback,
                View.OnAttachStateChangeListener,
                View.OnCreateContextMenuListener,
                InvalidationAwareThumbnailProvider,
                BrowserControlsStateProvider.Observer {
    private final Activity mActivity;
    @Nullable private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ExpandableListView mListView;
    private final String mTitle;
    private final ViewGroup mView;

    private RecentTabsManager mRecentTabsManager;
    private RecentTabsRowAdapter mAdapter;
    private NativePageHost mPageHost;

    private boolean mSnapshotContentChanged;
    private int mSnapshotListPosition;
    private int mSnapshotListTop;
    private int mSnapshotWidth;
    private int mSnapshotHeight;

    /** Whether {@link #mView} is attached to the application window. */
    private boolean mIsAttachedToWindow;

    private final ObservableSupplier<Integer> mTabStripHeightSupplier;
    private Callback<Integer> mTabStripHeightChangeCallback;
    private SmoothTransitionDelegate mSmoothTransitionDelegate;

    /**
     * Constructor returns an instance of RecentTabsPage.
     *
     * @param activity The activity this view belongs to.
     * @param recentTabsManager The RecentTabsManager which provides the model data.
     * @param pageHost The NativePageHost used to provide a history navigation delegate object.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} used to provide
     *     offset values.
     * @param tabStripHeightSupplier Supplier for the tab strip height.
     */
    public RecentTabsPage(
            Activity activity,
            RecentTabsManager recentTabsManager,
            NativePageHost pageHost,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<Integer> tabStripHeightSupplier) {
        mActivity = activity;
        mRecentTabsManager = recentTabsManager;
        mPageHost = pageHost;
        Resources resources = activity.getResources();

        mTitle = resources.getString(R.string.recent_tabs);
        mRecentTabsManager.setUpdatedCallback(this);
        LayoutInflater inflater = LayoutInflater.from(activity);
        mView = (ViewGroup) inflater.inflate(R.layout.recent_tabs_page, null);
        mListView = mView.findViewById(R.id.odp_listview);
        mAdapter = new RecentTabsRowAdapter(activity, recentTabsManager);
        mListView.setAdapter(mAdapter);
        mListView.setOnChildClickListener(this);
        mListView.setGroupIndicator(null);
        mListView.setOnGroupCollapseListener(this);
        mListView.setOnGroupExpandListener(this);
        mListView.setOnCreateContextMenuListener(this);

        mView.addOnAttachStateChangeListener(this);

        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity)) {
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mBrowserControlsStateProvider.addObserver(this);
            onBottomControlsHeightChanged(
                    mBrowserControlsStateProvider.getBottomControlsHeight(),
                    mBrowserControlsStateProvider.getBottomControlsMinHeight());
        } else {
            mBrowserControlsStateProvider = null;
        }

        mTabStripHeightSupplier = tabStripHeightSupplier;
        mView.setPadding(0, mTabStripHeightSupplier.get(), 0, 0);
        mTabStripHeightChangeCallback =
                newHeight ->
                        mView.setPadding(
                                mView.getPaddingLeft(),
                                newHeight,
                                mView.getPaddingRight(),
                                mView.getPaddingBottom());
        mTabStripHeightSupplier.addObserver(mTabStripHeightChangeCallback);

        onUpdated();
    }

    // NativePage overrides

    @Override
    public String getUrl() {
        return UrlConstants.RECENT_TABS_URL;
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public int getBackgroundColor() {
        return SemanticColorUtils.getDefaultBgColor(mActivity);
    }

    @Override
    public boolean needsToolbarShadow() {
        return true;
    }

    @Override
    public View getView() {
        return mView;
    }

    @Override
    public String getHost() {
        return UrlConstants.RECENT_TABS_HOST;
    }

    @Override
    public SmoothTransitionDelegate enableSmoothTransition() {
        if (mSmoothTransitionDelegate == null) {
            mSmoothTransitionDelegate = new BasicSmoothTransitionDelegate(getView());
        }
        return mSmoothTransitionDelegate;
    }

    @Override
    public void destroy() {
        assert !mIsAttachedToWindow : "Destroy called before removed from window";
        mRecentTabsManager.destroy();
        mRecentTabsManager = null;
        mPageHost = null;
        mAdapter.notifyDataSetInvalidated();
        mAdapter = null;
        mListView.setAdapter((RecentTabsRowAdapter) null);

        mView.removeOnAttachStateChangeListener(this);
        if (mBrowserControlsStateProvider != null) {
            mBrowserControlsStateProvider.removeObserver(this);
        }

        mTabStripHeightSupplier.removeObserver(mTabStripHeightChangeCallback);
    }

    @Override
    public void updateForUrl(String url) {}

    @Override
    public int getHeightOverlappedWithTopControls() {
        return mBrowserControlsStateProvider == null
                ? 0
                : mBrowserControlsStateProvider.getTopControlsHeight();
    }

    // View.OnAttachStateChangeListener
    @Override
    public void onViewAttachedToWindow(View view) {
        // Called when the user opens the RecentTabsPage or switches back to the RecentTabsPage from
        // another tab.
        mIsAttachedToWindow = true;

        // Work around a bug on Samsung devices where the recent tabs page does not appear after
        // toggling the Sync quick setting.  For some reason, the layout is being dropped on the
        // flow and we need to force a root level layout to get the UI to appear.
        ViewUtils.requestLayout(view.getRootView(), "RecentTabsPage.onViewAttachedToWindow");
    }

    @Override
    public void onViewDetachedFromWindow(View view) {
        // Called when the user navigates from the RecentTabsPage or switches to another tab.
        mIsAttachedToWindow = false;
    }

    // ExpandableListView.OnChildClickedListener
    @Override
    public boolean onChildClick(
            ExpandableListView parent, View v, int groupPosition, int childPosition, long id) {
        return mAdapter.getGroup(groupPosition).onChildClick(childPosition);
    }

    // ExpandableListView.OnGroupExpandedListener
    @Override
    public void onGroupExpand(int groupPosition) {
        mAdapter.getGroup(groupPosition).setCollapsed(false);
        mSnapshotContentChanged = true;
    }

    // ExpandableListView.OnGroupCollapsedListener
    @Override
    public void onGroupCollapse(int groupPosition) {
        mAdapter.getGroup(groupPosition).setCollapsed(true);
        mSnapshotContentChanged = true;
    }

    // RecentTabsManager.UpdatedCallback
    @Override
    public void onUpdated() {
        mAdapter.notifyDataSetChanged();
        for (int i = 0; i < mAdapter.getGroupCount(); i++) {
            if (mAdapter.getGroup(i).isCollapsed()) {
                mListView.collapseGroup(i);
            } else {
                mListView.expandGroup(i);
            }
        }
        mSnapshotContentChanged = true;
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenuInfo menuInfo) {
        // Would prefer to have this context menu view managed internal to RecentTabsGroupView
        // Unfortunately, setting either onCreateContextMenuListener or onLongClickListener
        // disables the native onClick (expand/collapse) behaviour of the group view.
        ExpandableListView.ExpandableListContextMenuInfo info =
                (ExpandableListView.ExpandableListContextMenuInfo) menuInfo;

        int type = ExpandableListView.getPackedPositionType(info.packedPosition);
        int groupPosition = ExpandableListView.getPackedPositionGroup(info.packedPosition);

        if (type == ExpandableListView.PACKED_POSITION_TYPE_GROUP) {
            mAdapter.getGroup(groupPosition).onCreateContextMenuForGroup(menu, mActivity);
        } else if (type == ExpandableListView.PACKED_POSITION_TYPE_CHILD) {
            int childPosition = ExpandableListView.getPackedPositionChild(info.packedPosition);
            mAdapter.getGroup(groupPosition)
                    .onCreateContextMenuForChild(childPosition, menu, mActivity);
        }
    }

    // InvalidationAwareThumbnailProvider

    @Override
    public boolean shouldCaptureThumbnail() {
        if (mView.getWidth() == 0 || mView.getHeight() == 0) return false;

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

    @Override
    public void onBottomControlsHeightChanged(
            int bottomControlsHeight, int bottomControlsMinHeight) {
        updateMargins();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateMargins();
    }

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean needsAnimate,
            boolean isVisibilityForced) {
        updateMargins();
    }

    private void updateMargins() {
        final View recentTabsRoot = mView.findViewById(R.id.recent_tabs_root);
        final int topControlsHeight = mBrowserControlsStateProvider.getTopControlsHeight();
        final int contentOffset = mBrowserControlsStateProvider.getContentOffset();
        ViewGroup.MarginLayoutParams layoutParams =
                (ViewGroup.MarginLayoutParams) recentTabsRoot.getLayoutParams();
        int topMargin = layoutParams.topMargin;

        // If the top controls are at the resting position or their height is decreasing, we want to
        // update the margin. We don't do this if the controls height is increasing because changing
        // the margin shrinks the view height to its final value, leaving a gap at the bottom until
        // the animation finishes.
        if (contentOffset >= topControlsHeight) {
            topMargin = topControlsHeight;
        }

        // If the content offset is different from the margin, we use translationY to position the
        // view in line with the content offset.
        recentTabsRoot.setTranslationY(contentOffset - topMargin);

        final int bottomMargin = mBrowserControlsStateProvider.getBottomControlsHeight();
        if (topMargin != layoutParams.topMargin || bottomMargin != layoutParams.bottomMargin) {
            layoutParams.topMargin = topMargin;
            layoutParams.bottomMargin = bottomMargin;
            recentTabsRoot.setLayoutParams(layoutParams);
        }
    }

    Callback<Integer> getTabStripHeightChangeCallbackForTesting() {
        return mTabStripHeightChangeCallback;
    }
}
