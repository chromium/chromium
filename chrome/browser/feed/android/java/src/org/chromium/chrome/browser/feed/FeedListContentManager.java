// Copyright 2020 The Chromium Authors
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
import androidx.annotation.Px;

import org.chromium.chrome.browser.xsurface.ListContentManager;
import org.chromium.chrome.browser.xsurface.ListContentManagerObserver;
import org.chromium.chrome.browser.xsurface.LoggingParameters;
import org.chromium.ui.UiUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

/**
 * Implementation of ListContentManager that manages a list of feed contents that are supported by
 * either native view or external surface controlled view.
 */
public class FeedListContentManager implements ListContentManager {
    /** Encapsulates the content of an item stored and managed by ListContentManager. */
    public abstract static class FeedContent {
        private final String mKey;
        private final boolean mIsFullSpan;

        FeedContent(String key) {
            this(key, false);
        }

        FeedContent(String key, boolean isFullSpan) {
            assert key != null && !key.isEmpty();
            mKey = key;
            mIsFullSpan = isFullSpan;
        }

        /** Returns true if the content is supported by the native view. */
        public abstract boolean isNativeView();

        /** Returns the key which should uniquely identify the content in the list. */
        public String getKey() {
            return mKey;
        }

        public boolean isFullSpan() {
            return mIsFullSpan;
        }

        public @Nullable LoggingParameters getLoggingParameters() {
            return null;
        }
    }

    /** For the content that is supported by external surface controlled view. */
    public static class ExternalViewContent extends FeedContent {
        private final byte[] mData;
        private final LoggingParameters mLoggingParameters;

        public ExternalViewContent(String key, byte[] data, LoggingParameters loggingParameters) {
            super(key);
            mData = data;
            mLoggingParameters = loggingParameters;
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

        @Override
        public @Nullable LoggingParameters getLoggingParameters() {
            return mLoggingParameters;
        }
    }

    /** For the content that is supported by the native view. */
    public static class NativeViewContent extends FeedContent {
        private View mNativeView;
        private int mResId;
        // An unique ID for this NativeViewContent. This is initially 0, and assigned by
        // FeedListContentManager when needed.
        private int mViewType;
        @Px private int mLateralPaddingsPx;

        /** Holds an inflated native view. */
        public NativeViewContent(@Px int lateralPaddingsPx, String key, View nativeView) {
            super(key, true);
            assert nativeView != null;
            mNativeView = nativeView;
            mLateralPaddingsPx = lateralPaddingsPx;
        }

        /** Holds an inflated native view. */
        public NativeViewContent(
                @Px int lateralPaddingsPx, String key, View nativeView, boolean isFullSpan) {
            super(key, isFullSpan);
            assert nativeView != null;
            mNativeView = nativeView;
            mLateralPaddingsPx = lateralPaddingsPx;
        }

        /** Holds a resource ID used to inflate a native view. */
        public NativeViewContent(@Px int lateralPaddingsPx, String key, int resId) {
            super(key, true);
            mResId = resId;
            mLateralPaddingsPx = lateralPaddingsPx;
        }

        /** Returns the native view if the content is supported by it. Null otherwise. */
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
            FrameLayout.LayoutParams layoutParams =
                    new FrameLayout.LayoutParams(
                            new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
            enclosingLayout.setLayoutParams(layoutParams);

            // Set the left and right paddings.
            enclosingLayout.setPadding(
                    /* left= */ mLateralPaddingsPx, /* top= */ 0,
                    /* right= */ mLateralPaddingsPx, /* bottom= */ 0);

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

    /** Returns a list of all contents */
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

    /**
     * Replaces content in the range [index, index+count) with the content in {@code
     * newContentList}. For content that already exists in the range, it is moved rather than
     * removed and then inserted.
     *
     * @param rangeStart Index of first item to replace.
     * @param count Number of items to replace.
     * @param newContentList List of content to insert.
     * @return Whether content has changed. Returns false if the new content matches the replaced
     *     content.
     */
    public boolean replaceRange(int rangeStart, int count, List<FeedContent> newContentList) {
        boolean hasContentChange = false;
        // 1) Builds the hash set based on keys of new contents.
        HashSet<String> newContentKeySet = new HashSet<>();
        for (int i = 0; i < newContentList.size(); ++i) {
            newContentKeySet.add(newContentList.get(i).getKey());
        }

        // 2) Builds the hash map of existing content list for fast look up by key. Ignores headers.
        HashMap<String, FeedContent> existingContentMap = new HashMap<>();
        for (int i = rangeStart; i < rangeStart + count; ++i) {
            FeedContent content = getContent(i);
            existingContentMap.put(content.getKey(), content);
        }

        // 3) Removes those existing contents that do not appear in the new list.
        for (int i = rangeStart + count - 1; i >= rangeStart; ) {
            // Find out how many contiguous items need to be removed, and then remove them in one
            // call.
            int rmIndex = i;
            while (rmIndex >= rangeStart) {
                String key = getContent(rmIndex).getKey();
                if (newContentKeySet.contains(key)) {
                    break;
                }
                existingContentMap.remove(key);
                --rmIndex;
            }

            if (rmIndex != i) {
                hasContentChange = true;
                removeContents(rmIndex + 1, i - rmIndex);
                i = rmIndex;
            } else {
                --i;
            }
        }

        // 4) Iterates through the new list to add the new content or move the existing content
        //    if needed.
        for (int i = 0; i < newContentList.size(); ) {
            FeedContent content = newContentList.get(i);

            // If this is an existing content, moves it to new position, offset by header count.
            if (existingContentMap.containsKey(content.getKey())) {
                hasContentChange = true;
                int oldIndex = findContentPositionByKey(content.getKey());
                int newIndex = i + rangeStart;
                if (oldIndex != newIndex) {
                    hasContentChange = true;
                    moveContent(oldIndex, i + rangeStart);
                }

                ++i;
                continue;
            }

            // Otherwise, this is new content. Add it together with all adjacent new contents.
            int startIndex = i++;
            while (i < newContentList.size()
                    && !existingContentMap.containsKey(newContentList.get(i).getKey())) {
                ++i;
            }
            hasContentChange = true;
            // Account for headers when inserting contents.
            addContents(startIndex + rangeStart, newContentList.subList(startIndex, i));
        }

        return hasContentChange;
    }

    @Override
    public boolean isNativeView(int index) {
        return mFeedContentList.get(index).isNativeView();
    }

    @Override
    public boolean isFullSpan(int index) {
        return mFeedContentList.get(index).isFullSpan();
    }

    @Override
    public byte[] getExternalViewBytes(int index) {
        assert !mFeedContentList.get(index).isNativeView();
        ExternalViewContent externalViewContent = (ExternalViewContent) mFeedContentList.get(index);
        return externalViewContent.getBytes();
    }

    @Override
    public Map<String, Object> getContextValues(int index) {
        // We just return mHandlers for items unless they need logging parameters added.
        if (index >= 0 && index < mFeedContentList.size()) {
            LoggingParameters loggingParameters =
                    mFeedContentList.get(index).getLoggingParameters();
            if (loggingParameters != null) {
                // It might be a good idea to cache this value, but it adds complexity because
                // setHandlers() can be called after items are added.
                Map<String, Object> contextValues = new HashMap<>(mHandlers);
                contextValues.put(LoggingParameters.KEY, loggingParameters);
                return contextValues;
            }
        }
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

    private @Nullable NativeViewContent findNativeViewByType(int viewType) {
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
