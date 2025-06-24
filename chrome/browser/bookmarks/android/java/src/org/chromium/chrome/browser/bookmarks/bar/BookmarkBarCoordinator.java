// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static android.view.View.VISIBLE;

import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocus;
import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocusOnFirstFocusableDescendant;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarVisibilityProvider.BookmarkBarVisibilityObserver;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Coordinator for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
public class BookmarkBarCoordinator implements TopControlLayer, BookmarkBarVisibilityObserver {

    private final SimpleRecyclerViewAdapter mItemsAdapter;
    private final BookmarkBarItemsLayoutManager mBookmarkBarItemsLayoutManager;
    private final BookmarkBarMediator mMediator;
    private final BookmarkBar mView;
    private final TopControlsStacker mTopControlsStacker;
    private final Callback<@Nullable Void> mHeightChangeCallback;
    private final View.OnLayoutChangeListener mOnLayoutChangeListener;

    /**
     * Constructs the bookmark bar coordinator.
     *
     * @param activity The activity which is hosting the bookmark bar.
     * @param browserControlsStateProvider The state provider for browser controls.
     * @param heightChangeCallback A callback to notify owner of bookmark bar height changes.
     * @param profileSupplier The supplier for the currently active profile.
     * @param viewStub The stub used to inflate the bookmark bar.
     * @param currentTab The current tab if it exists.
     * @param bookmarkOpener Used to open bookmarks.
     * @param bookmarkManagerOpenerSupplier Used to open the bookmark manager.
     * @param topControlsStacker TopControlsStacker to manage the view's y-offset.
     */
    public BookmarkBarCoordinator(
            Activity activity,
            BrowserControlsStateProvider browserControlsStateProvider,
            Callback<@Nullable Void> heightChangeCallback,
            ObservableSupplier<Profile> profileSupplier,
            ViewStub viewStub,
            @Nullable Tab currentTab,
            BookmarkOpener bookmarkOpener,
            ObservableSupplier<BookmarkManagerOpener> bookmarkManagerOpenerSupplier,
            TopControlsStacker topControlsStacker) {
        mView = (BookmarkBar) viewStub.inflate();
        mHeightChangeCallback = heightChangeCallback;
        mOnLayoutChangeListener =
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    final int oldHeight = oldBottom - oldTop;
                    final int newHeight = bottom - top;
                    if (newHeight != oldHeight) {
                        mHeightChangeCallback.onResult(null);
                    }
                };
        mView.addOnLayoutChangeListener(mOnLayoutChangeListener);

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
        mBookmarkBarItemsLayoutManager = new BookmarkBarItemsLayoutManager(activity);
        mBookmarkBarItemsLayoutManager.setItemMaxWidth(
                activity.getResources().getDimensionPixelSize(R.dimen.bookmark_bar_item_max_width));
        itemsContainer.setLayoutManager(mBookmarkBarItemsLayoutManager);

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
                        this::getTopControlHeight,
                        itemsModel,
                        mBookmarkBarItemsLayoutManager.getItemsOverflowSupplier(),
                        model,
                        profileSupplier,
                        currentTab,
                        bookmarkOpener,
                        bookmarkManagerOpenerSupplier);
        PropertyModelChangeProcessor.create(model, mView, BookmarkBarViewBinder::bind);

        mTopControlsStacker = topControlsStacker;
        mTopControlsStacker.addControl(this);
    }

    /** Destroys the bookmark bar coordinator. */
    public void destroy() {
        mTopControlsStacker.removeControl(this);
        mItemsAdapter.destroy();
        mMediator.destroy();
        mView.removeOnLayoutChangeListener(mOnLayoutChangeListener);
    }

    /**
     * @return The view for the bookmark bar.
     */
    public View getView() {
        return mView;
    }

    private BookmarkBarButton inflateBookmarkBarButton(ViewGroup parent) {
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

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.BOOKMARK_BAR;
    }

    @Override
    public int getTopControlHeight() {
        return mView.getHeight();
    }

    @Override
    public int getTopControlVisibility() {
        // This could always return TopControlVisibility.VISIBLE assuming that on all visibility
        // changes {@link TabbedRootUiCoordinator#destroyBookmarkBarIfNecessary} is called,
        // but for correctness we will check view visibility just in case.
        // TODO(crbug.com/417238089): Possibly add way to notify stacker of visibility changes.
        return mView != null && mView.getVisibility() == VISIBLE
                ? TopControlVisibility.VISIBLE
                : TopControlVisibility.HIDDEN;
    }

    // BookmarkBarVisibilityObserver implementation:

    @Override
    public void onMaxWidthChanged(int maxWidth) {
        mBookmarkBarItemsLayoutManager.setItemMaxWidth(maxWidth);
    }
}
