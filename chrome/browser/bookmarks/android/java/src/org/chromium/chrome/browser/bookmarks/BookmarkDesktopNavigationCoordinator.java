// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.View;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.navigation_pane.NavigationPaneAdapterFactory;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Coordinator for the bookmark desktop navigation pane. */
@NullMarked
public class BookmarkDesktopNavigationCoordinator {
    private final Context mContext;
    private final View mView;
    private final ModelList mModelList;
    private final SimpleRecyclerViewAdapter mAdapter;
    private final RecyclerView mRecyclerView;

    private @Nullable BookmarkDesktopNavigationMediator mMediator;
    private boolean mDestroyed;

    /**
     * @param context The current Activity context.
     * @param navigationPane The root view of the navigation pane.
     * @param bookmarkModel The bookmark model.
     * @param bookmarkDelegateSupplier Supplier for the bookmark delegate.
     */
    public BookmarkDesktopNavigationCoordinator(
            Context context,
            View navigationPane,
            BookmarkModel bookmarkModel,
            OneshotSupplier<BookmarkDelegate> bookmarkDelegateSupplier) {
        mContext = context;
        mView = navigationPane;
        mModelList = new ModelList();

        mAdapter = NavigationPaneAdapterFactory.createAdapter(mContext, mModelList);

        mRecyclerView = mView.findViewById(R.id.navigation_recycler_view);
        mRecyclerView.setLayoutManager(new LinearLayoutManager(mContext));
        mRecyclerView.setAdapter(mAdapter);

        bookmarkDelegateSupplier.onAvailable(
                bookmarkDelegate -> {
                    if (mDestroyed) return;
                    mMediator =
                            new BookmarkDesktopNavigationMediator(
                                    mContext, bookmarkModel, mModelList, bookmarkDelegate);
                });
    }

    /** Destroys the coordinator and its resources. */
    public void destroy() {
        mDestroyed = true;
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
    }
}
