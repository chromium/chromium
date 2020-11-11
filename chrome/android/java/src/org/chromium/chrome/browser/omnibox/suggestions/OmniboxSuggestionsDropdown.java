// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.KeyNavigationUtil;

/**
 * A widget for showing a list of omnibox suggestions.
 */
public class OmniboxSuggestionsDropdown extends RecyclerView {
    private final @NonNull OmniboxSuggestionsDropdownDelegate mDropdownDelegate;
    private final @NonNull SuggestionScrollListener mScrollListener;
    private @Nullable OmniboxSuggestionsDropdown.Observer mObserver;
    private @Nullable OmniboxSuggestionsDropdownAdapter mAdapter;

    private final int[] mTempMeasureSpecs = new int[2];

    /** Interface that will receive notifications and callbacks from OmniboxSuggestionList. */
    public interface Observer {
        /**
         * Invoked whenever the height of suggestion list changes.
         * The height may change as a result of eg. soft keyboard popping up.
         *
         * @param newHeightPx New height of the suggestion list in pixels.
         */
        void onSuggestionDropdownHeightChanged(@Px int newHeightPx);

        /**
         * Invoked whenever the User scrolls the list.
         */
        void onSuggestionDropdownScroll();

        /**
         * Invoked whenever the User scrolls the list to the top.
         */
        void onSuggestionDropdownOverscrolledToTop();

        /**
         * Notify that the user is interacting with an item on the Suggestions list.
         *
         * @param isGestureUp Whether user pressed (false) or depressed (true) the element on the
         *         list.
         * @param timestamp The timestamp associated with the event.
         */
        void onGesture(boolean isGestureUp, long timestamp);
    }

    /** Scroll listener that propagates scroll event notification to registered observers. */
    private class SuggestionScrollListener extends RecyclerView.OnScrollListener {
        private OmniboxSuggestionsDropdown.Observer mObserver;

        void setObserver(OmniboxSuggestionsDropdown.Observer observer) {
            mObserver = observer;
        }

        @Override
        public void onScrolled(RecyclerView view, int dx, int dy) {}

        @Override
        public void onScrollStateChanged(RecyclerView view, int scrollState) {
            if (scrollState == SCROLL_STATE_DRAGGING && mObserver != null) {
                mObserver.onSuggestionDropdownScroll();
            }
        }

        void onOverscrollToTop() {
            mObserver.onSuggestionDropdownOverscrolledToTop();
        }
    }

    /**
     * RecyclerView pool that records performance of the view recycling mechanism.
     * @see OmniboxSuggestionsListViewListAdapter#canReuseView(View, int)
     */
    private class HistogramRecordingRecycledViewPool extends RecycledViewPool {
        HistogramRecordingRecycledViewPool() {
            setMaxRecycledViews(OmniboxSuggestionUiType.CLIPBOARD_SUGGESTION, 1);
            setMaxRecycledViews(OmniboxSuggestionUiType.EDIT_URL_SUGGESTION, 1);
            setMaxRecycledViews(OmniboxSuggestionUiType.ANSWER_SUGGESTION, 1);
            setMaxRecycledViews(OmniboxSuggestionUiType.DEFAULT, 15);
            setMaxRecycledViews(OmniboxSuggestionUiType.ENTITY_SUGGESTION, 5);
            setMaxRecycledViews(OmniboxSuggestionUiType.TAIL_SUGGESTION, 10);
        }

        @Override
        public ViewHolder getRecycledView(int viewType) {
            ViewHolder result = super.getRecycledView(viewType);
            SuggestionsMetrics.recordSuggestionViewReused(result != null);
            return result;
        }
    }

    /**
     * Constructs a new list designed for containing omnibox suggestions.
     * @param context Context used for contained views.
     */
    public OmniboxSuggestionsDropdown(Context context) {
        super(context, null, android.R.attr.dropDownListViewStyle);
        setFocusable(true);
        setFocusableInTouchMode(true);
        setRecycledViewPool(new HistogramRecordingRecycledViewPool());

        // By default RecyclerViews come with item animators.
        setItemAnimator(null);

        mScrollListener = new SuggestionScrollListener();
        setOnScrollListener(mScrollListener);
        setLayoutManager(new LinearLayoutManager(context) {
            @Override
            public int scrollVerticallyBy(
                    int deltaY, RecyclerView.Recycler recycler, RecyclerView.State state) {
                int scrollY = super.scrollVerticallyBy(deltaY, recycler, state);
                if (scrollY == 0 && deltaY < 0) {
                    mScrollListener.onOverscrollToTop();
                }
                return scrollY;
            }
        });

        final Resources resources = context.getResources();
        int paddingBottom =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_bottom);
        ViewCompat.setPaddingRelative(this, 0, 0, 0, paddingBottom);

        mDropdownDelegate = new OmniboxSuggestionsDropdownDelegate(resources, this);
    }

    /** Get the Android View implementing suggestion list. */
    public ViewGroup getViewGroup() {
        return this;
    }

    /**
     * Sets the embedder for the list view.
     * @param embedder the embedder of this list.
     */
    public void setEmbedder(OmniboxSuggestionsDropdownEmbedder embedder) {
        mDropdownDelegate.setEmbedder(embedder);
    }

    /**
     * Sets the observer of suggestion list.
     * @param observer an observer of this list.
     */
    public void setObserver(OmniboxSuggestionsDropdown.Observer observer) {
        mObserver = observer;
        mScrollListener.setObserver(observer);
        mDropdownDelegate.setObserver(observer);
    }

    /** Resets selection typically in response to changes to the list. */
    public void resetSelection() {
        if (mAdapter == null) return;
        mAdapter.resetSelection();
    }

    /** @return The number of items in the list. */
    public int getDropdownItemViewCountForTest() {
        if (mAdapter == null) return 0;
        return mAdapter.getItemCount();
    }

    /** @return The Suggestion view at specific index. */
    public View getDropdownItemViewForTest(int index) {
        final LayoutManager manager = getLayoutManager();
        manager.scrollToPosition(index);
        return manager.findViewByPosition(index);
    }

    /** Show (and properly size) the suggestions list. */
    public void show() {
        if (getVisibility() == VISIBLE) return;

        setVisibility(VISIBLE);
        if (mAdapter != null && mAdapter.getSelectedViewIndex() != 0) {
            mAdapter.resetSelection();
        }
    }

    /** Hide the suggestions list and release any cached resources. */
    public void hide() {
        if (getVisibility() != VISIBLE) return;
        setVisibility(GONE);
        getRecycledViewPool().clear();
    }

    /** Update the suggestion popup background to reflect the current state. */
    public void refreshPopupBackground(boolean isIncognito) {
        setBackground(mDropdownDelegate.getPopupBackground(isIncognito));
    }

    @Override
    public void setAdapter(Adapter adapter) {
        mAdapter = (OmniboxSuggestionsDropdownAdapter) adapter;
        super.setAdapter(mAdapter);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        getRecycledViewPool().clear();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent tracing = TraceEvent.scoped("OmniboxSuggestionsList.Measure");
                SuggestionsMetrics.TimingMetric metric =
                        SuggestionsMetrics.recordSuggestionListMeasureTime()) {
            mDropdownDelegate.calculateOnMeasureAndTriggerUpdates(mTempMeasureSpecs);
            super.onMeasure(mTempMeasureSpecs[0], mTempMeasureSpecs[1]);
        }
    }

    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        try (TraceEvent tracing = TraceEvent.scoped("OmniboxSuggestionsList.Layout");
                SuggestionsMetrics.TimingMetric metric =
                        SuggestionsMetrics.recordSuggestionListLayoutTime()) {
            super.onLayout(changed, l, t, r, b);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!isShown()) return false;

        int selectedPosition = mAdapter.getSelectedViewIndex();
        if (KeyNavigationUtil.isGoDown(event)) {
            return mAdapter.setSelectedViewIndex(selectedPosition + 1);
        } else if (KeyNavigationUtil.isGoUp(event)) {
            return mAdapter.setSelectedViewIndex(selectedPosition - 1);
        } else if (KeyNavigationUtil.isGoRight(event) || KeyNavigationUtil.isGoLeft(event)) {
            View selectedView = mAdapter.getSelectedView();
            if (selectedView != null) return selectedView.onKeyDown(keyCode, event);
        } else if (KeyNavigationUtil.isEnter(event)) {
            View selectedView = mAdapter.getSelectedView();
            if (selectedView != null) return selectedView.performClick();
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        return mDropdownDelegate.shouldIgnoreGenericMotionEvent(event)
                || super.onGenericMotionEvent(event);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        final int eventType = ev.getActionMasked();
        if ((eventType == MotionEvent.ACTION_UP || eventType == MotionEvent.ACTION_DOWN)
                && mObserver != null) {
            mObserver.onGesture(eventType == MotionEvent.ACTION_UP, ev.getEventTime());
        }
        return super.dispatchTouchEvent(ev);
    }
}
