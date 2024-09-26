// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import androidx.appcompat.widget.AppCompatTextView;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.xsurface.HybridListRenderer;
import org.chromium.chrome.browser.xsurface.ListContentManager;
import org.chromium.chrome.browser.xsurface.ListContentManagerObserver;
import org.chromium.chrome.browser.xsurface.ListLayoutHelper;
import org.chromium.ui.base.ViewUtils;

/** Implementation of {@link HybridListRenderer} for list consisting all native views. */
public class NativeViewListRenderer extends RecyclerView.Adapter<NativeViewListRenderer.ViewHolder>
        implements HybridListRenderer, ListContentManagerObserver {
    /** A ViewHolder for the underlying RecyclerView. */
    public static class ViewHolder extends RecyclerView.ViewHolder {
        ViewHolder(View v) {
            super(v);
        }
    }

    private final Context mContext;

    private ListContentManager mManager;
    private ListLayoutHelper mLayoutHelper;
    private RecyclerView mView;

    public NativeViewListRenderer(Context mContext) {
        this.mContext = mContext;
    }

    /* RecyclerView.Adapter methods */
    @Override
    public int getItemCount() {
        return mManager.getItemCount();
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        if (mManager.isNativeView(position)) {
            mManager.bindNativeView(position, holder.itemView);
        }
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
        View v;
        if (viewType >= 0) {
            v = mManager.getNativeView(viewType, parent);
        } else {
            v = createCannotRenderViewItem();
        }
        return new ViewHolder(v);
    }

    @Override
    public int getItemViewType(int position) {
        if (!mManager.isNativeView(position)) return -1;
        return mManager.getViewType(position);
    }

    /* HybridListRenderer methods */
    @Override
    public View bind(ListContentManager manager) {
        mManager = manager;
        mView = new RecyclerView(mContext);
        mView.setAdapter(this);
        LinearLayoutManager layoutManager = new LinearLayoutManager(mContext);
        mView.setLayoutManager(layoutManager);
        mManager.addObserver(this);
        onItemRangeInserted(0, mManager.getItemCount());
        mLayoutHelper = new NativeViewListLayoutHelper(layoutManager);
        return mView;
    }

    @Override
    public void unbind() {
        mManager.removeObserver(this);
        onItemRangeRemoved(0, mManager.getItemCount());
        mView.setAdapter(null);
        mView.setLayoutManager(null);
        mManager = null;
    }

    @Override
    public void update(byte[] data) {}

    /* ListContentManagerObserver methods */
    @Override
    public void onItemRangeInserted(int startIndex, int count) {
        notifyItemRangeInserted(startIndex, count);
    }

    @Override
    public void onItemRangeRemoved(int startIndex, int count) {
        notifyItemRangeRemoved(startIndex, count);
    }

    @Override
    public void onItemRangeChanged(int startIndex, int count) {
        notifyItemRangeChanged(startIndex, count);
    }

    @Override
    public void onItemMoved(int curIndex, int newIndex) {
        notifyItemMoved(curIndex, newIndex);
    }

    @Override
    public ListLayoutHelper getListLayoutHelper() {
        return mLayoutHelper;
    }

    RecyclerView getListViewForTest() {
        return mView;
    }

    private View createCannotRenderViewItem() {
        TextView viewItem = new AppCompatTextView(mContext);
        String message = "Unable to render external view";
        viewItem.setText(message);

        MarginLayoutParams layoutParams =
                new MarginLayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        layoutParams.bottomMargin = ViewUtils.dpToPx(mContext, 25F);
        layoutParams.topMargin = ViewUtils.dpToPx(mContext, 25F);
        viewItem.setLayoutParams(layoutParams);

        return viewItem;
    }

    static class NativeViewListLayoutHelper implements ListLayoutHelper {
        private LinearLayoutManager mLayoutManager;

        public NativeViewListLayoutHelper(LinearLayoutManager layoutManager) {
            mLayoutManager = layoutManager;
        }

        @Override
        public int findFirstVisibleItemPosition() {
            return mLayoutManager.findFirstVisibleItemPosition();
        }

        @Override
        public int findLastVisibleItemPosition() {
            return mLayoutManager.findLastVisibleItemPosition();
        }

        @Override
        public void scrollToPositionWithOffset(int position, int offset) {
            mLayoutManager.scrollToPositionWithOffset(position, offset);
        }

        @Override
        public boolean setColumnCount(int columnCount) {
            // no-op operation for LinearLayout. Consider as success.
            return true;
        }
    }
}
