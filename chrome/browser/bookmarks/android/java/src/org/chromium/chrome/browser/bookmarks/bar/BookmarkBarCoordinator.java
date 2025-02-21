// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Coordinator for the bookmark bar which provides users with bookmark access from top chrome. */
public class BookmarkBarCoordinator {

    private final SimpleRecyclerViewAdapter mItemsAdapter;
    private final BookmarkBarMediator mMediator;
    private final BookmarkBar mView;

    /**
     * Constructs the bookmark bar coordinator.
     *
     * @param activity the activity which is hosting the bookmark bar.
     * @param browserControlsStateProvider the state provider for browser control
     *     positioning/visibility.
     * @param heightChangeCallback a callback to notify of bookmark bar height change events.
     * @param profileSupplier the supplier for the currently active profile.
     * @param viewStub the stub used to inflate the bookmark bar.
     * @param bookmarkOpener used to open bookmarks.
     */
    public BookmarkBarCoordinator(
            @NonNull Activity activity,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull Callback<Integer> heightChangeCallback,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull ViewStub viewStub,
            @NonNull BookmarkOpener bookmarkOpener,
            @NonNull ObservableSupplier<BookmarkManagerOpener> bookmarkManagerOpenerSupplier) {
        mView = (BookmarkBar) viewStub.inflate();

        // Bind view/model for 'All Bookmarks' button.
        final var allBookmarksButtonModel =
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                allBookmarksButtonModel,
                mView.findViewById(R.id.bookmark_bar_all_bookmarks_button),
                BookmarkBarButtonViewBinder::bind);

        // Bind adapter/model and initialize view for bookmark bar items.
        final var itemsModel = new ModelList();
        mItemsAdapter = new SimpleRecyclerViewAdapter(itemsModel);
        mItemsAdapter.registerType(
                BookmarkBarUtils.ViewType.ITEM,
                this::inflateBookmarkBarButton,
                BookmarkBarButtonViewBinder::bind);
        final RecyclerView itemsContainer = mView.findViewById(R.id.bookmark_bar_items_container);
        itemsContainer.setAdapter(mItemsAdapter);
        final var itemsLayoutManager = new BookmarkBarItemsLayoutManager(activity);
        itemsContainer.setLayoutManager(itemsLayoutManager);

        // NOTE: Scrolling isn't supported and items rarely change so item view caching is disabled.
        itemsContainer.getRecycledViewPool().setMaxRecycledViews(BookmarkBarUtils.ViewType.ITEM, 0);
        itemsContainer.setItemViewCacheSize(0);

        // Bind view/model for bookmark bar and instantiate mediator.
        final var model = new PropertyModel.Builder(BookmarkBarProperties.ALL_KEYS).build();
        mMediator =
                new BookmarkBarMediator(
                        activity,
                        allBookmarksButtonModel,
                        browserControlsStateProvider,
                        heightChangeCallback,
                        itemsModel,
                        itemsLayoutManager.getItemsOverflowSupplier(),
                        model,
                        profileSupplier,
                        bookmarkOpener,
                        bookmarkManagerOpenerSupplier);
        PropertyModelChangeProcessor.create(model, mView, BookmarkBarViewBinder::bind);
    }

    /** Destroys the bookmark bar coordinator. */
    public void destroy() {
        mItemsAdapter.destroy();
        mMediator.destroy();
        mView.destroy();
    }

    /**
     * @return the supplier which provides the current height of the bookmark bar.
     */
    public ObservableSupplier<Integer> getHeightSupplier() {
        return mMediator.getHeightSupplier();
    }

    private @NonNull BookmarkBarButton inflateBookmarkBarButton(@NonNull ViewGroup parent) {
        return (BookmarkBarButton)
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.bookmark_bar_button, parent, false);
    }
}
