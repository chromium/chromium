// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.xsurface.ListContentManager;
import org.chromium.chrome.browser.xsurface.ListContentManagerObserver;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Implementation of ListContentManager that manages a list of feed contents that are supported by
 * either native view or external surface controlled view.
 */
public class NtpListContentManager implements ListContentManager {
    /**
     * Encapsulates the content of an item stored and managed by ListContentManager.
     */
    public abstract static class FeedContent {
        private final String mKey;

        FeedContent(String key) {
            assert key != null && !key.isEmpty();
            mKey = key;
        }

        /**
         * Returns true if the content is supported by the native view.
         */
        public abstract boolean isNativeView();

        /**
         * Returns the key which should uniquely identify the content in the list.
         */
        public String getKey() {
            return mKey;
        }
    }

    /**
     * For the content that is supported by external surface controlled view.
     */
    public static class ExternalViewContent extends FeedContent {
        private final byte[] mData;

        public ExternalViewContent(String key, byte[] data) {
            super(key);
            mData = data;
        }

        /**
         * Returns the raw bytes that are passed to the external surface for rendering if the
         * content is supported by the external surface controlled view.
         */
        public byte[] getBytes() {
            return mData;
        }

        @Override
        public boolean isNativeView() {
            return false;
        }
    }

    /**
     * For the content that is supported by the native view.
     */
    public static class NativeViewContent extends FeedContent {
        private View mNativeView;
        private int mResId;
        // An unique ID for this NativeViewContent. This is initially 0, and assigned by
        // FeedListContentManager when needed.
        private int mViewType;

        /** Holds an inflated native view. */
        public NativeViewContent(String key, View nativeView) {
            super(key);
            assert nativeView != null;
            mNativeView = nativeView;
        }

        /** Holds a resource ID used to inflate a native view. */
        public NativeViewContent(String key, int resId) {
            super(key);
            mResId = resId;
        }

        /**
         * Returns the native view if the content is supported by it. Null otherwise.
         */
        public View getNativeView(ViewGroup parent) {
            Context context = parent.getContext();
            if (mNativeView == null) {
                mNativeView = LayoutInflater.from(context).inflate(mResId, parent, false);
            }

            // If there's already a parent, we have already enclosed this view previously.
            // This can happen if a native view is added, removed, and added again.
            // In this case, it is important to make a new view because the RecyclerView
            // may still have a reference to the old one. See crbug.com/1131975.
            UiUtils.removeViewFromParent(mNativeView);

            FrameLayout enclosingLayout = new FrameLayout(parent.getContext());
            FrameLayout.LayoutParams layoutParams = new FrameLayout.LayoutParams(
                    new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            enclosingLayout.setLayoutParams(layoutParams);

            // Set the left and right paddings.
            int horizontalPadding = context.getResources().getDimensionPixelSize(
                    R.dimen.ntp_header_lateral_margins_v2);
            enclosingLayout.setPadding(/* left */ horizontalPadding, /* top */ 0,
                    /* right */ horizontalPadding, /* bottom */ 0);
            // Do not clip children. This ensures that the negative margin use in the feed header
            // does not subsequently cause the IPH bubble to be clipped.
            enclosingLayout.setClipToPadding(false);
            enclosingLayout.setClipChildren(false);
            enclosingLayout.addView(mNativeView);
            return enclosingLayout;
        }

        int getViewType() {
            return mViewType;
        }

        void setViewType(int viewType) {
            mViewType = viewType;
        }

        @Override
        public boolean isNativeView() {
            return true;
        }
    }

    private final ArrayList<FeedContent> mFeedContentList = new ArrayList<>();
    private final ArrayList<ListContentManagerObserver> mObservers = new ArrayList<>();
    private final Map<String, Object> mHandlers = new HashMap<>();
    private int mPreviousViewType;

    /**
     * Clears existing handlers and sets current handlers to newHandlers.
     * @param newHandlers handlers to set.
     */
    public void setHandlers(Map<String, Object> newHandlers) {
        mHandlers.clear();
        mHandlers.putAll(newHandlers);
    }

    /**
     * Returns the content at the specified position.
     *
     * @param index The index at which to get the content.
     * @return The content.
     */
    public FeedContent getContent(int index) {
        return mFeedContentList.get(index);
    }

    /**
     * Returns a list of all contents
     */
    public List<FeedContent> getContentList() {
        return mFeedContentList;
    }

    /**
     * Finds the position of the content with the specified key in the list.
     *
     * @param key The key of the content to search for.
     * @return The position if found, -1 otherwise.
     */
    public int findContentPositionByKey(String key) {
        for (int i = 0; i < mFeedContentList.size(); ++i) {
            if (mFeedContentList.get(i).getKey().equals(key)) {
                return i;
            }
        }
        return -1;
    }

    /**
     * Adds a list of the contents, starting at the specified position.
     *
     * @param index The index at which to insert the first content from the specified collection.
     * @param contents The collection containing contents to be added.
     */
    public void addContents(int index, List<FeedContent> contents) {
        assert index >= 0 && index <= mFeedContentList.size();
        mFeedContentList.addAll(index, contents);
        for (ListContentManagerObserver observer : mObservers) {
            observer.onItemRangeInserted(index, contents.size());
        }
    }

    /**
     * Removes the specified count of contents starting from the speicified position.
     *
     * @param index The index of the first content to be removed.
     * @param count The number of contents to be removed.
     */
    public void removeContents(int index, int count) {
        assert index >= 0 && index < mFeedContentList.size();
        assert index + count <= mFeedContentList.size();
        mFeedContentList.subList(index, index + count).clear();
        for (ListContentManagerObserver observer : mObservers) {
            observer.onItemRangeRemoved(index, count);
        }
    }

    /**
     * Updates a list of the contents, starting at the specified position.
     *
     * @param index The index at which to update the first content from the specified collection.
     * @param contents The collection containing contents to be updated.
     */
    public void updateContents(int index, List<FeedContent> contents) {
        assert index >= 0 && index < mFeedContentList.size();
        assert index + contents.size() <= mFeedContentList.size();
        int pos = index;
        for (FeedContent content : contents) {
            mFeedContentList.set(pos++, content);
        }
        for (ListContentManagerObserver observer : mObservers) {
            observer.onItemRangeChanged(index, contents.size());
        }
    }

    /**
     * Moves the content to a different position.
     *
     * @param curIndex The index of the content to be moved.
     * @param newIndex The new index where the content is being moved to.
     */
    public void moveContent(int curIndex, int newIndex) {
        assert curIndex >= 0 && curIndex < mFeedContentList.size();
        assert newIndex >= 0 && newIndex < mFeedContentList.size();
        int lowIndex;
        int highIndex;
        int distance;
        if (curIndex < newIndex) {
            lowIndex = curIndex;
            highIndex = newIndex;
            distance = -1;
        } else if (curIndex > newIndex) {
            lowIndex = newIndex;
            highIndex = curIndex;
            distance = 1;
        } else {
            return;
        }
        Collections.rotate(mFeedContentList.subList(lowIndex, highIndex + 1), distance);
        for (ListContentManagerObserver observer : mObservers) {
            observer.onItemMoved(curIndex, newIndex);
        }
    }

    @Override
    public boolean isNativeView(int index) {
        return mFeedContentList.get(index).isNativeView();
    }

    @Override
    public byte[] getExternalViewBytes(int index) {
        assert !mFeedContentList.get(index).isNativeView();
        ExternalViewContent externalViewContent = (ExternalViewContent) mFeedContentList.get(index);
        return externalViewContent.getBytes();
    }

    @Override
    public Map<String, Object> getContextValues(int index) {
        return mHandlers;
    }

    @Override
    public int getViewType(int position) {
        assert mFeedContentList.get(position).isNativeView();
        NativeViewContent content = (NativeViewContent) mFeedContentList.get(position);
        if (content.getViewType() == 0) content.setViewType(++mPreviousViewType);
        return content.getViewType();
    }

    @Override
    public View getNativeView(int viewType, ViewGroup parent) {
        NativeViewContent viewContent = findNativeViewByType(viewType);
        assert viewContent != null;
        return viewContent.getNativeView(parent);
    }

    @Override
    public void bindNativeView(int index, View v) {
        // Nothing to do.
    }

    @Override
    public int getItemCount() {
        return mFeedContentList.size();
    }

    @Override
    public void addObserver(ListContentManagerObserver observer) {
        mObservers.add(observer);
    }

    @Override
    public void removeObserver(ListContentManagerObserver observer) {
        mObservers.remove(observer);
    }

    @Nullable
    private NativeViewContent findNativeViewByType(int viewType) {
        // Note: since there's relatively few native views, they're mostly at the front, a linear
        // search isn't terrible. This function is also called infrequently.
        for (int i = 0; i < mFeedContentList.size(); i++) {
            FeedContent item = mFeedContentList.get(i);
            if (!item.isNativeView()) continue;
            NativeViewContent nativeContent = (NativeViewContent) item;
            if (nativeContent.getViewType() == viewType) return nativeContent;
        }
        return null;
    }
}
