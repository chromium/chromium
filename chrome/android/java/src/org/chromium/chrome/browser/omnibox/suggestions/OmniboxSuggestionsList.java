// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.content.res.Resources;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;

import org.chromium.base.TraceEvent;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.util.KeyNavigationUtil;

import java.util.ArrayList;

/**
 * A ListView based implementation of {@link OmniboxSuggestionsDropdown} for showing a list of
 * omnibox suggestions.
 */
@VisibleForTesting
public class OmniboxSuggestionsList
        extends ListView implements OmniboxSuggestionsDropdown, AbsListView.OnScrollListener {
    private final OmniboxSuggestionsDropdownDelegate mDropdownDelegate;
    private OmniboxSuggestionsDropdown.Observer mObserver;

    private final int[] mTempMeasureSpecs = new int[2];

    /**
     * Constructs a new list designed for containing omnibox suggestions.
     * @param context Context used for contained views.
     */
    public OmniboxSuggestionsList(Context context) {
        super(context, null, android.R.attr.dropDownListViewStyle);
        setDivider(null);
        setFocusable(true);
        setFocusableInTouchMode(true);

        final Resources resources = context.getResources();
        int paddingBottom =
                resources.getDimensionPixelOffset(R.dimen.omnibox_suggestion_list_padding_bottom);
        ViewCompat.setPaddingRelative(this, 0, 0, 0, paddingBottom);

        mDropdownDelegate = new OmniboxSuggestionsDropdownDelegate(resources, this);
    }

    @Override
    public ViewGroup getViewGroup() {
        return this;
    }

    @Override
    public void setEmbedder(OmniboxSuggestionsDropdown.Embedder embedder) {
        mDropdownDelegate.setEmbedder(embedder);
        setOnScrollListener(this);
    }

    @Override
    public void setObserver(OmniboxSuggestionsDropdown.Observer observer) {
        mObserver = observer;
        mDropdownDelegate.setObserver(observer);
    }

    @Override
    public void resetSelection() {
        setSelection(0);
    }

    @Override
    public int getItemCount() {
        return getCount();
    }

    @Override
    public void show() {
        if (getVisibility() == VISIBLE) return;

        setVisibility(VISIBLE);
        if (getSelectedItemPosition() != 0) setSelection(0);
    }

    @Override
    public void hide() {
        if (getVisibility() != VISIBLE) return;
        setVisibility(GONE);
    }

    @Override
    public void refreshPopupBackground(boolean isIncognito) {
        setBackground(mDropdownDelegate.getPopupBackground(isIncognito));
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
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!isShown()) return false;

        int selectedPosition = getSelectedItemPosition();
        int itemCount = getAdapter().getCount();
        if (KeyNavigationUtil.isGoDown(event)) {
            if (selectedPosition >= itemCount - 1) {
                // Do not pass down events when the last item is already selected as it will
                // dismiss the suggestion list.
                return true;
            }

            if (selectedPosition == ListView.INVALID_POSITION) {
                // When clearing the selection after a text change, state is not reset
                // correctly so hitting down again will cause it to start from the previous
                // selection point. We still have to send the key down event to let the list
                // view items take focus, but then we select the first item explicitly.
                boolean result = super.onKeyDown(keyCode, event);
                setSelection(0);
                return result;
            }
        } else if ((KeyNavigationUtil.isGoRight(event) || KeyNavigationUtil.isGoLeft(event))
                && selectedPosition != ListView.INVALID_POSITION) {
            View selectedView = getSelectedView();
            if (selectedView != null) return selectedView.onKeyDown(keyCode, event);
        } else if (KeyNavigationUtil.isEnter(event)
                && selectedPosition != ListView.INVALID_POSITION) {
            View selectedView = getSelectedView();
            if (selectedView != null) return selectedView.performClick();
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        // Ensure none of the views are reused when re-attaching as the TextViews in the suggestions
        // do not handle it in all cases.  https://crbug.com/851839
        reclaimViews(new ArrayList<>());
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
    public boolean onGenericMotionEvent(MotionEvent event) {
        return mDropdownDelegate.shouldIgnoreGenericMotionEvent(event)
                || super.onGenericMotionEvent(event);
    }

    // Implementation of AbsListView.OnScrollListener.
    @Override
    public void onScroll(
            AbsListView view, int firstVisibleItem, int visibleItemCount, int totalItemCount) {}

    @Override
    protected boolean overScrollBy(int deltaX, int deltaY, int scrollX, int scrollY, int rangeX,
            int rangeY, int maxX, int maxY, boolean isTouchEvent) {
        if (scrollY == 0 && deltaY < 0) {
            mObserver.onSuggestionDropdownOverscrolledToTop();
        }
        return super.overScrollBy(
                deltaX, deltaY, scrollX, scrollY, rangeX, rangeY, maxX, maxY, isTouchEvent);
    }

    @Override
    public void onScrollStateChanged(AbsListView view, int scrollState) {
        if (scrollState == SCROLL_STATE_TOUCH_SCROLL && mObserver != null) {
            mObserver.onSuggestionDropdownScroll();
        }
    }
}
