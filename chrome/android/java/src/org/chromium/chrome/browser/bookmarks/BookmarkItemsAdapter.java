// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * BaseAdapter for {@link RecyclerView}. It manages bookmarks to list there.
 */
public class BookmarkItemsAdapter extends DragReorderableListAdapter<BookmarkListEntry> {
    /** Abstraction around how to build type specific {@link View}s. */
    interface ViewFactory {
        /**
         * @param parent The parent to which the new {@link View} will be added as a child.
         * @param viewType The type of row being created.
         * @return A new View that can be added to the view hierarchy.
         */
        View buildView(@NonNull ViewGroup parent, @ViewType int viewType);
    }

    /** Abstraction around how to bind type specific {@link View}s. */
    interface ViewBinder {
        void bindView(View view, @ViewType int viewType, PropertyModel model);
    }

    /**
     * Temporary interface to provide functionality that needs dependencies until this adapter is
     * replaced. See https://crbug.com/1413463.
     */
    interface ViewDelegate {
        PropertyModel buildModel(ViewHolder holder, int position);
        void recycleView(View view, @ViewType int viewType);
        void setOrder(List<BookmarkListEntry> listEntries);
        boolean isReorderable(BookmarkListEntry entry);
    }

    private final ViewFactory mViewFactory;
    private final ViewBinder mViewBinder;
    private @Nullable BookmarkDelegate mBookmarkDelegate;
    private @Nullable ViewDelegate mViewDelegate;

    BookmarkItemsAdapter(Context context, ViewFactory viewFactory, ViewBinder viewBinder) {
        super(context);
        mViewFactory = viewFactory;
        mViewBinder = viewBinder;
    }

    /**
     * Sets the delegate to use to handle UI actions related to this adapter.
     * @param bookmarkDelegate A {@link ViewDelegate} instance to backend interactions.
     * @param viewDelegate A {@link ViewDelegate} instance to handle model interactions.
     */
    @SuppressWarnings("NotifyDataSetChanged")
    void onBookmarkDelegateInitialized(
            BookmarkDelegate bookmarkDelegate, ViewDelegate viewDelegate) {
        mElements = new ArrayList<>();
        mBookmarkDelegate = bookmarkDelegate;
        mViewDelegate = viewDelegate;
        setDragStateDelegate(bookmarkDelegate.getDragStateDelegate());
        notifyDataSetChanged();
    }

    List<BookmarkListEntry> getElements() {
        return mElements;
    }

    ItemTouchHelper getItemTouchHelper() {
        return mItemTouchHelper;
    }

    // DragReorderableListAdapter implementation.

    @Override
    public @ViewType int getItemViewType(int position) {
        BookmarkListEntry entry = getItemByPosition(position);
        return entry.getViewType();
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, @ViewType int viewType) {
        return new ViewHolder(mViewFactory.buildView(parent, viewType)) {};
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        PropertyModel propertyModel = mViewDelegate.buildModel(holder, position);
        mViewBinder.bindView(holder.itemView, holder.getItemViewType(), propertyModel);
    }

    @Override
    public void onViewRecycled(ViewHolder holder) {
        mViewDelegate.recycleView(holder.itemView, holder.getItemViewType());
    }

    @Override
    protected void setOrder(List<BookmarkListEntry> listEntries) {
        mViewDelegate.setOrder(listEntries);
    }

    // DragReorderableListAdapter implementation.

    @Override
    public boolean isActivelyDraggable(ViewHolder viewHolder) {
        return isPassivelyDraggable(viewHolder)
                && ((BookmarkRow) viewHolder.itemView).isItemSelected();
    }

    @Override
    public boolean isPassivelyDraggable(ViewHolder viewHolder) {
        return mViewDelegate.isReorderable(getItemByHolder(viewHolder));
    }
}
