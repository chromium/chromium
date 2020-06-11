// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.app.Activity;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.feed.shared.stream.Header;
import org.chromium.chrome.browser.feed.shared.stream.Stream;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.feed.R;

import java.util.ArrayList;
import java.util.List;

/**
 * A implementation of a Feed {@link Stream} that is just able to render a vertical stream of
 * cards for Feed v2.
 */
public class FeedStream implements Stream {
    private static final String TAG = "FeedStream";

    private final Activity mActivity;
    private final FeedStreamSurface mFeedStreamSurface;
    private final ObserverList<ScrollListener> mScrollListeners;

    private RecyclerView mRecyclerView;
    // setStreamContentVisibility() is always called once after onCreate(). So we can assume the
    // stream content is hidden initially and it can be made visible later when
    // setStreamContentVisibility() is called.
    private boolean mIsStreamContentVisible = false;

    public FeedStream(Activity activity, boolean isBackgroundDark, SnackbarManager snackbarManager,
            NativePageNavigationDelegate nativePageNavigationDelegate,
            BottomSheetController bottomSheetController) {
        // TODO(petewil): Use isBackgroundDark to turn on dark theme.
        this.mActivity = activity;
        this.mFeedStreamSurface = new FeedStreamSurface(
                activity, snackbarManager, nativePageNavigationDelegate, bottomSheetController);
        this.mScrollListeners = new ObserverList<ScrollListener>();
    }

    @Override
    public void onCreate(@Nullable String savedInstanceState) {
        setupRecyclerView();
        // TODO(jianli): Restore scroll state.
    }

    @Override
    public void onShow() {}

    @Override
    public void onHide() {}

    @Override
    public void onDestroy() {
        mFeedStreamSurface.destroy();
    }

    @Override
    public String getSavedInstanceStateString() {
        // TODO(jianli): Return saved scroll state as serializedf string;
        return "";
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
        if (visible == mIsStreamContentVisible) {
            return;
        }
        mIsStreamContentVisible = visible;

        if (visible) {
            mFeedStreamSurface.surfaceOpened();
        } else {
            mFeedStreamSurface.surfaceClosed();
        }
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
        // Not longer needed.
    }

    @Override
    public void removeOnContentChangedListener(ContentChangedListener listener) {
        // Not longer needed.
    }

    @Override
    public void triggerRefresh() {}

    private void setupRecyclerView() {
        assert (!mIsStreamContentVisible);

        mRecyclerView = (RecyclerView) mFeedStreamSurface.getView();
        mRecyclerView.setId(R.id.feed_stream_recycler_view);
        mRecyclerView.setClipToPadding(false);
        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView v, int x, int y) {
                for (ScrollListener listener : mScrollListeners) {
                    listener.onScrolled(x, y);
                }
            }
        });
    }
}
