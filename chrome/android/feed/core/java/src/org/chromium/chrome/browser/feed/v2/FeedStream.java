// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.app.Activity;
import android.util.TypedValue;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.shared.stream.Header;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ntp.ScrollListener;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;

/**
 * A implementation of a Feed {@link Stream} that is just able to render a vertical stream of
 * cards for Feed v2.
 */
public class FeedStream implements Stream {
    private static final String TAG = "FeedStream";

    // How far the user has to scroll down in DP before attempting to load more content.
    private final int mLoadMoreTriggerScrollDistanceDp;

    private final Activity mActivity;
    @VisibleForTesting
    final FeedStreamSurface mFeedStreamSurface;
    private final ObserverList<ScrollListener> mScrollListeners =
            new ObserverList<ScrollListener>();
    private RecyclerView mRecyclerView;
    // For loading more content.
    private int mAccumulatedDySinceLastLoadMore;

    private String mScrollStateToRestore;
    private RestoreScrollObserver mRestoreScrollObserver = new RestoreScrollObserver();

    public FeedStream(Activity activity, boolean isBackgroundDark, SnackbarManager snackbarManager,
            NativePageNavigationDelegate nativePageNavigationDelegate,
            BottomSheetController bottomSheetController, boolean isPlaceholderShown,
            WindowAndroid windowAndroid, Supplier<ShareDelegate> shareDelegateSupplier) {
        // TODO(petewil): Use isBackgroundDark to turn on dark theme.
        this.mActivity = activity;

        this.mFeedStreamSurface = new FeedStreamSurface(activity, isBackgroundDark, snackbarManager,
                nativePageNavigationDelegate, bottomSheetController,
                HelpAndFeedbackLauncherImpl.getInstance(), isPlaceholderShown,
                new FeedStreamSurface.ShareHelperWrapper(windowAndroid, shareDelegateSupplier),
                windowAndroid.getDisplay());

        this.mLoadMoreTriggerScrollDistanceDp =
                FeedServiceBridge.getLoadMoreTriggerScrollDistanceDp();
    }

    @Override
    public void onCreate(@Nullable String savedInstanceState) {
        mScrollStateToRestore = savedInstanceState;
        setupRecyclerView();
    }

    @Override
    public void onShow() {
        mFeedStreamSurface.setStreamVisibility(true);
    }

    @Override
    public void onHide() {
        mAccumulatedDySinceLastLoadMore = 0;
        mScrollStateToRestore = null;
        if (mFeedStreamSurface.isOpened()) {
            mScrollStateToRestore = getSavedInstanceStateString();
        }
        mFeedStreamSurface.setStreamVisibility(false);
    }

    @Override
    public void onDestroy() {
        mScrollStateToRestore = null;
        mFeedStreamSurface.destroy();
    }

    @Override
    public String getSavedInstanceStateString() {
        LinearLayoutManager layoutManager = (LinearLayoutManager) mRecyclerView.getLayoutManager();
        if (layoutManager == null) {
            return "";
        }
        ScrollState state = new ScrollState();
        state.position = layoutManager.findFirstVisibleItemPosition();
        state.lastPosition = layoutManager.findLastVisibleItemPosition();
        if (state.position == RecyclerView.NO_POSITION) {
            return "";
        }

        View firstVisibleView = layoutManager.findViewByPosition(state.position);
        if (firstVisibleView == null) {
            return "";
        }
        state.offset = firstVisibleView.getTop();
        return state.toJson();
    }

    @Override
    public View getView() {
        return mRecyclerView;
    }

    @Override
    public void setHeaderViews(List<Header> headers) {
        ArrayList<View> headerViews = new ArrayList<View>();
        for (Header header : headers) {
            headerViews.add(header.getView());
        }
        mFeedStreamSurface.setHeaderViews(headerViews);
    }

    @Override
    public void setStreamContentVisibility(boolean visible) {
        mFeedStreamSurface.setStreamContentVisibility(visible);
    }

    @Override
    public void toggledArticlesListVisible(boolean visible) {
        mFeedStreamSurface.toggledArticlesListVisible(visible);
    }

    @Override
    public void trim() {
        mRecyclerView.getRecycledViewPool().clear();
    }

    @Override
    public void smoothScrollBy(int dx, int dy) {
        mRecyclerView.smoothScrollBy(dx, dy);
    }

    @Override
    public int getChildTopAt(int position) {
        if (!isChildAtPositionVisible(position)) {
            return POSITION_NOT_KNOWN;
        }

        LinearLayoutManager layoutManager = (LinearLayoutManager) mRecyclerView.getLayoutManager();
        if (layoutManager == null) {
            return POSITION_NOT_KNOWN;
        }

        View view = layoutManager.findViewByPosition(position);
        if (view == null) {
            return POSITION_NOT_KNOWN;
        }

        return view.getTop();
    }

    @Override
    public boolean isChildAtPositionVisible(int position) {
        LinearLayoutManager layoutManager = (LinearLayoutManager) mRecyclerView.getLayoutManager();
        if (layoutManager == null) {
            return false;
        }

        int firstItemPosition = layoutManager.findFirstVisibleItemPosition();
        int lastItemPosition = layoutManager.findLastVisibleItemPosition();
        if (firstItemPosition == RecyclerView.NO_POSITION
                || lastItemPosition == RecyclerView.NO_POSITION) {
            return false;
        }

        return position >= firstItemPosition && position <= lastItemPosition;
    }

    @Override
    public void addScrollListener(ScrollListener listener) {
        mScrollListeners.addObserver(listener);
    }

    @Override
    public void removeScrollListener(ScrollListener listener) {
        mScrollListeners.removeObserver(listener);
    }

    @Override
    public void addOnContentChangedListener(ContentChangedListener listener) {
        mFeedStreamSurface.addContentChangedListener(listener);
    }

    @Override
    public void removeOnContentChangedListener(ContentChangedListener listener) {
        mFeedStreamSurface.removeContentChangedListener(listener);
    }

    @Override
    public void recordActionManageInterests() {
        mFeedStreamSurface.recordActionManageInterests();
    }
    @Override
    public void recordActionManageActivity() {
        mFeedStreamSurface.recordActionManageActivity();
    }
    @Override
    public void recordActionManageReactions() {
        mFeedStreamSurface.recordActionManageReactions();
    }
    @Override
    public void recordActionLearnMore() {
        mFeedStreamSurface.recordActionLearnMore();
    }

    @Override
    public void triggerRefresh() {}

    @Override
    public boolean isPlaceholderShown() {
        return mFeedStreamSurface.isPlaceholderShown();
    }

    @Override
    public void hidePlaceholder() {
        mFeedStreamSurface.hidePlaceholder();
    }

    private void setupRecyclerView() {
        mRecyclerView = (RecyclerView) mFeedStreamSurface.getView();
        mRecyclerView.setId(R.id.feed_stream_recycler_view);
        mRecyclerView.setClipToPadding(false);
        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView v, int dx, int dy) {
                super.onScrolled(v, dx, dy);
                checkScrollingForLoadMore(dy);
                mFeedStreamSurface.streamScrolled(dx, dy);
                for (ScrollListener listener : mScrollListeners) {
                    listener.onScrolled(dx, dy);
                }
            }
            @Override
            public void onScrollStateChanged(RecyclerView v, int newState) {
                for (ScrollListener listener : mScrollListeners) {
                    listener.onScrollStateChanged(newState);
                }
            }
        });
        mRecyclerView.getAdapter().registerAdapterDataObserver(mRestoreScrollObserver);
    }

    @VisibleForTesting
    void checkScrollingForLoadMore(int dy) {
        if (!mFeedStreamSurface.isOpened()) return;

        mAccumulatedDySinceLastLoadMore += dy;
        if (mAccumulatedDySinceLastLoadMore < 0) {
            mAccumulatedDySinceLastLoadMore = 0;
        }
        if (mAccumulatedDySinceLastLoadMore < TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                    mLoadMoreTriggerScrollDistanceDp,
                    mRecyclerView.getResources().getDisplayMetrics())) {
            return;
        }

        boolean canTrigger = mFeedStreamSurface.maybeLoadMore();
        if (canTrigger) {
            mAccumulatedDySinceLastLoadMore = 0;
        }
    }

    /**
     * Restores the scroll state serialized to |savedInstanceState|.
     * @return true if the scroll state was restored, or if the state could never be restored.
     * false if we need to wait until more items are added to the recycler view to make it
     * scrollable.
     */
    private boolean restoreScrollState(String savedInstanceState) {
        assert (mRecyclerView != null);
        ScrollState state = ScrollState.fromJson(savedInstanceState);
        if (state == null) return true;

        // If too few items exist, defer scrolling until later.
        if (mRecyclerView.getAdapter().getItemCount() <= state.lastPosition) return false;

        LinearLayoutManager layoutManager = (LinearLayoutManager) mRecyclerView.getLayoutManager();
        if (layoutManager != null) {
            layoutManager.scrollToPositionWithOffset(state.position, state.offset);
        }
        return true;
    }

    static class ScrollState {
        private static final String SCROLL_POSITION = "pos";
        private static final String SCROLL_LAST_POSITION = "lpos";
        private static final String SCROLL_OFFSET = "off";

        public int position;
        public int lastPosition;
        public int offset;

        String toJson() {
            JSONObject jsonSavedState = new JSONObject();
            try {
                jsonSavedState.put(SCROLL_POSITION, position);
                jsonSavedState.put(SCROLL_LAST_POSITION, lastPosition);
                jsonSavedState.put(SCROLL_OFFSET, offset);
                return jsonSavedState.toString();
            } catch (JSONException e) {
                Log.d(TAG, "Unable to write to a JSONObject.");
                return "";
            }
        }
        @Nullable
        static ScrollState fromJson(String json) {
            ScrollState result = new ScrollState();
            try {
                JSONObject jsonSavedState = new JSONObject(json);
                result.position = jsonSavedState.getInt(SCROLL_POSITION);
                result.lastPosition = jsonSavedState.getInt(SCROLL_LAST_POSITION);
                result.offset = jsonSavedState.getInt(SCROLL_OFFSET);
            } catch (JSONException e) {
                Log.d(TAG, "Unable to parse a JSONObject from a string.");
                return null;
            }
            return result;
        }
    }

    // Scroll state can't be restored until enough items are added to the recycler view adapter.
    // Attempts to restore scroll state every time new items are added to the adapter.
    class RestoreScrollObserver extends RecyclerView.AdapterDataObserver {
        @Override
        public void onItemRangeInserted(int positionStart, int itemCount) {
            if (mScrollStateToRestore != null) {
                if (restoreScrollState(mScrollStateToRestore)) {
                    mScrollStateToRestore = null;
                }
            }
        }
    };
}
