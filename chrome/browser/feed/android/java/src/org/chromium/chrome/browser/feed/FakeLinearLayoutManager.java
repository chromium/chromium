// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.xsurface.ListLayoutHelper;

import java.util.ArrayList;
import java.util.List;

/** A fake version of LinearLayoutManager. */
public class FakeLinearLayoutManager extends LinearLayoutManager implements ListLayoutHelper {
    private final List<View> mChildViews;
    private int mFirstVisiblePosition = RecyclerView.NO_POSITION;
    private int mLastVisiblePosition = RecyclerView.NO_POSITION;
    private int mItemCount = RecyclerView.NO_POSITION;

    public FakeLinearLayoutManager(Context context) {
        super(context);
        mChildViews = new ArrayList<>();
    }

    public void setFirstVisiblePosition(int firstVisiblePosition) {
        mFirstVisiblePosition = firstVisiblePosition;
    }

    public void setLastVisiblePosition(int lastVisiblePosition) {
        mLastVisiblePosition = lastVisiblePosition;
    }

    public void setItemCount(int itemCount) {
        mItemCount = itemCount;
    }

    public void addChildToPosition(int position, View child) {
        mChildViews.add(position, child);
    }

    @Override
    public int findFirstVisibleItemPosition() {
        return mFirstVisiblePosition;
    }

    @Override
    public int findLastVisibleItemPosition() {
        return mLastVisiblePosition;
    }

    @Override
    public int getItemCount() {
        return mItemCount;
    }

    @Override
    public View findViewByPosition(int i) {
        if (i < 0 || i >= mChildViews.size()) {
            return null;
        }
        return mChildViews.get(i);
    }
}
