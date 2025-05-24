// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocus;
import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocusOnFirstFocusableDescendant;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
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
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
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
     * @param activity The activity which is hosting the bookmark bar.
     * @param activityLifecycleDispatcher The lifecycle dispatcher for the host activity.
     * @param browserControlsStateProvider The state provider for browser controls.
     * @param heightChangeCallback A callback to notify of bookmark bar height change events.
     * @param profileSupplier The supplier for the currently active profile.
     * @param viewStub The stub used to inflate the bookmark bar.
     * @param bookmarkOpener Used to open bookmarks.
     * @param bookmarkManagerOpenerSupplier Used to open the bookmark manager.
     */
    public BookmarkBarCoordinator(
            @NonNull Activity activity,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
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
                        activityLifecycleDispatcher,
                        allBookmarksButtonModel,
                        browserControlsStateProvider,
                        heightChangeCallback,
                        itemsModel,
                        itemsLayoutManager.getItemsOverflowSupplier(),
                        itemsLayoutManager::setItemMaxWidth,
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
     * @return The supplier which provides the current height of the bookmark bar.
     */
    public ObservableSupplier<Integer> getHeightSupplier() {
        return mMediator.getHeightSupplier();
    }

    /**
     * @return The view for the bookmark bar.
     */
    public @NonNull View getView() {
        return mView;
    }

    private @NonNull BookmarkBarButton inflateBookmarkBarButton(@NonNull ViewGroup parent) {
        return (BookmarkBarButton)
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.bookmark_bar_button, parent, false);
    }

    /** Requests focus within the bookmark bar. */
    public void requestFocus() {
        if (setFocusOnFirstFocusableDescendant(
                mView.findViewById(R.id.bookmark_bar_items_container))) {
            // If we set focus on a bookmark in the RecyclerView of user bookmarks, we are done.
            return;
        }
        // Otherwise (there were no user bookmarks), focus on the all bookmarks button at the end.
        setFocus(mView.findViewById(R.id.bookmark_bar_all_bookmarks_button));
    }

    /**
     * @return Whether keyboard focus is within this view.
     */
    public boolean hasKeyboardFocus() {
        return mView.getFocusedChild() != null;
    }
}
