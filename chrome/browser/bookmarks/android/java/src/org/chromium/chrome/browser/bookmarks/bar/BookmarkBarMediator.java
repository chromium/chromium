// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.res.Configuration;
import android.view.KeyEvent;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkImageFetcher;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.ConfigurationChangedObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.function.BiConsumer;

/** Mediator for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
class BookmarkBarMediator
        implements BookmarkBarItemsProvider.Observer, BrowserControlsStateProvider.Observer {

    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final PropertyModel mAllBookmarksButtonModel;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ConfigurationChangedObserver mConfigurationChangeObserver;
    private final Callback<Integer> mHeightChangeCallback;
    private final ObservableSupplierImpl<Integer> mHeightSupplier;
    private final ModelList mItemsModel;
    private final ObservableSupplier<Boolean> mItemsOverflowSupplier;
    private final Callback<Boolean> mItemsOverflowSupplierObserver;
    private final Callback<Integer> mItemMaxWidthChangeCallback;
    private final PropertyModel mModel;
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileSupplierObserver;
    private final BookmarkOpener mBookmarkOpener;
    private final ObservableSupplier<BookmarkManagerOpener> mBookmarkManagerOpenerSupplier;

    private @Nullable BookmarkImageFetcher mImageFetcher;
    private @Nullable BookmarkBarItemsProvider mItemsProvider;

    /**
     * Constructs the bookmark bar mediator.
     *
     * @param activity The activity which is hosting the bookmark bar.
     * @param activityLifecycleDispatcher The lifecycle dispatcher for the host activity.
     * @param allBookmarksButtonModel The model for the 'All Bookmarks' button.
     * @param browserControlsStateProvider The state provider for browser controls.
     * @param heightChangeCallback A callback to notify of bookmark bar height change events.
     * @param itemsModel The model for the items which are rendered within the bookmark bar.
     * @param itemsOverflowSupplier The supplier for the current state of items overflow.
     * @param itemMaxWidthChangeCallback A callback to notify of item max width change events.
     * @param model The model used to read/write bookmark bar properties.
     * @param profileSupplier The supplier for the currently active profile.
     */
    public BookmarkBarMediator(
            Activity activity,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            PropertyModel allBookmarksButtonModel,
            BrowserControlsStateProvider browserControlsStateProvider,
            Callback<Integer> heightChangeCallback,
            ModelList itemsModel,
            ObservableSupplier<Boolean> itemsOverflowSupplier,
            Callback<Integer> itemMaxWidthChangeCallback,
            PropertyModel model,
            ObservableSupplier<Profile> profileSupplier,
            BookmarkOpener bookmarkOpener,
            ObservableSupplier<BookmarkManagerOpener> bookmarkManagerOpenerSupplier) {
        mActivity = activity;
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;

        mAllBookmarksButtonModel = allBookmarksButtonModel;
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.CLICK_CALLBACK, this::onAllBookmarksButtonClick);
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.ICON_SUPPLIER,
                LazyOneshotSupplier.fromValue(
                        AppCompatResources.getDrawable(
                                mActivity, R.drawable.ic_folder_outline_24dp)));
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.ICON_TINT_LIST_ID,
                R.color.default_icon_color_tint_list);
        mAllBookmarksButtonModel.set(
                BookmarkBarButtonProperties.TITLE,
                mActivity.getString(R.string.bookmark_bar_all_bookmarks_button_title));

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);

        mConfigurationChangeObserver = this::onConfigurationChange;
        mActivityLifecycleDispatcher.register(mConfigurationChangeObserver);

        // NOTE: Height will be updated when binding the `HEIGHT_CHANGE_CALLBACK` property.
        mHeightSupplier = new ObservableSupplierImpl<Integer>(0);
        mHeightChangeCallback = heightChangeCallback;
        mHeightSupplier.addObserver(mHeightChangeCallback);

        mItemsModel = itemsModel;

        mItemsOverflowSupplier = itemsOverflowSupplier;
        mItemsOverflowSupplierObserver = this::onItemsOverflowChange;
        mItemsOverflowSupplier.addObserver(mItemsOverflowSupplierObserver);

        mItemMaxWidthChangeCallback = itemMaxWidthChangeCallback;

        mModel = model;
        mModel.set(BookmarkBarProperties.HEIGHT_CHANGE_CALLBACK, mHeightSupplier::set);
        mModel.set(
                BookmarkBarProperties.OVERFLOW_BUTTON_CLICK_CALLBACK, this::onOverflowButtonClick);
        mModel.set(BookmarkBarProperties.OVERFLOW_BUTTON_VISIBILITY, View.INVISIBLE);

        mProfileSupplier = profileSupplier;
        mProfileSupplierObserver = this::onProfileChange;
        mProfileSupplier.addObserver(mProfileSupplierObserver);

        mBookmarkOpener = bookmarkOpener;
        mBookmarkManagerOpenerSupplier = bookmarkManagerOpenerSupplier;

        updateItemMaxWidth();
        updateTopMargin();
        updateVisibility();
    }

    /** Destroys the bookmark bar mediator. */
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(mConfigurationChangeObserver);
        mAllBookmarksButtonModel.set(BookmarkBarButtonProperties.CLICK_CALLBACK, null);
        mBrowserControlsStateProvider.removeObserver(this);
        mHeightSupplier.removeObserver(mHeightChangeCallback);
        mItemsOverflowSupplier.removeObserver(mItemsOverflowSupplierObserver);

        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mItemsProvider != null) {
            mItemsProvider.destroy();
            mItemsProvider = null;
        }

        mModel.set(BookmarkBarProperties.HEIGHT_CHANGE_CALLBACK, null);
        mProfileSupplier.removeObserver(mProfileSupplierObserver);
    }

    /**
     * @return the supplier which provides the current height of the bookmark bar.
     */
    public ObservableSupplier<Integer> getHeightSupplier() {
        return mHeightSupplier;
    }

    // BookmarkBarItemsProvider.Observer implementation.

    @Override
    public void onBookmarkItemAdded(
            @BookmarkBarItemsProvider.ObservationId int observationId,
            BookmarkItem item,
            int index) {
        mItemsModel.add(
                index,
                BookmarkBarUtils.createListItemFor(
                        this::onBookmarkItemClick, mActivity, mImageFetcher, item));
    }

    @Override
    public void onBookmarkItemMoved(
            @BookmarkBarItemsProvider.ObservationId int observationId, int index, int oldIndex) {
        mItemsModel.move(oldIndex, index);
    }

    @Override
    public void onBookmarkItemRemoved(
            @BookmarkBarItemsProvider.ObservationId int observationId, int index) {
        mItemsModel.removeAt(index);
    }

    @Override
    public void onBookmarkItemUpdated(
            @BookmarkBarItemsProvider.ObservationId int observationId,
            BookmarkItem item,
            int index) {
        mItemsModel.update(
                index,
                BookmarkBarUtils.createListItemFor(
                        this::onBookmarkItemClick, mActivity, mImageFetcher, item));
    }

    @Override
    public void onBookmarkItemsAdded(
            @BookmarkBarItemsProvider.ObservationId int observationId,
            List<BookmarkItem> items,
            int index) {
        final List<ListItem> batch = new ArrayList<>();
        for (int i = 0; i < items.size(); i++) {
            batch.add(
                    BookmarkBarUtils.createListItemFor(
                            this::onBookmarkItemClick, mActivity, mImageFetcher, items.get(i)));
        }
        mItemsModel.addAll(batch, index);
    }

    @Override
    public void onBookmarkItemsRemoved(
            @BookmarkBarItemsProvider.ObservationId int observationId, int index, int count) {
        mItemsModel.removeRange(index, count);
    }

    // BrowserControlsStateProvider.Observer implementation.

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        updateVisibility();
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        updateTopMargin();
    }

    // Private methods.

    // TODO(crbug.com/394614779): Open in popup window instead of bookmark manager.
    private void onAllBookmarksButtonClick(int metaState) {
        // Open the manager iff the active profile and model are unchanged to prevent accidentally
        // opening the manager for the wrong profile/model.
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profileAfterLoading, modelAfterLoading) -> {
                    mBookmarkManagerOpenerSupplier
                            .get()
                            .showBookmarkManager(
                                    mActivity,
                                    profileAfterLoading,
                                    modelAfterLoading.getRootFolderId());
                });
    }

    // TODO(crbug.com/394614166): Handle shift-click to open in new window.
    private void onBookmarkItemClick(BookmarkItem item, int metaState) {
        final Profile profile = mProfileSupplier.get();

        // TODO(crbug.com/394614779): Open in popup window instead of bookmark manager.
        if (item.isFolder()) {
            mBookmarkManagerOpenerSupplier
                    .get()
                    .showBookmarkManager(mActivity, profile, item.getId());
            return;
        }

        final boolean isCtrlPressed = (metaState & KeyEvent.META_CTRL_ON) != 0;
        if (isCtrlPressed) {
            mBookmarkOpener.openBookmarksInNewTabs(
                    List.of(item.getId()),
                    profile.isOffTheRecord(),
                    Optional.of(TabLaunchType.FROM_BOOKMARK_BAR_BACKGROUND));
            return;
        }

        mBookmarkOpener.openBookmarkInCurrentTab(item.getId(), profile.isOffTheRecord());
    }

    private void onConfigurationChange(Configuration newConfig) {
        updateItemMaxWidth();
    }

    private void onItemsOverflowChange(boolean itemsOverflow) {
        mModel.set(
                BookmarkBarProperties.OVERFLOW_BUTTON_VISIBILITY,
                itemsOverflow ? View.VISIBLE : View.INVISIBLE);
    }

    private void onOverflowButtonClick() {
        // Open the manager iff the active profile and model are unchanged to prevent accidentally
        // opening the manager for the wrong profile/model.
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profileAfterLoading, modelAfterLoading) -> {
                    mBookmarkManagerOpenerSupplier
                            .get()
                            .showBookmarkManager(
                                    mActivity,
                                    profileAfterLoading,
                                    Optional.ofNullable(
                                                    modelAfterLoading.getAccountDesktopFolderId())
                                            .orElseGet(modelAfterLoading::getDesktopFolderId));
                });
    }

    private void onProfileChange(@Nullable Profile profile) {
        if (mImageFetcher != null) {
            mImageFetcher.destroy();
            mImageFetcher = null;
        }

        if (mItemsProvider != null) {
            mItemsProvider.destroy();
            mItemsProvider = null;
        }

        mItemsModel.clear();

        if (profile == null) {
            return;
        }

        // Instantiate dependencies iff the active profile and model are unchanged to prevent
        // accidentally instantiating dependencies for the wrong profile/model.
        runIfStillRelevantAfterFinishLoadingBookmarkModel(
                (profileAfterLoading, modelAfterLoading) -> {
                    mImageFetcher =
                            new BookmarkImageFetcher(
                                    profileAfterLoading,
                                    mActivity,
                                    modelAfterLoading,
                                    ImageFetcherFactory.createImageFetcher(
                                            ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                                            profileAfterLoading.getProfileKey(),
                                            GlobalDiscardableReferencePool.getReferencePool()),
                                    FaviconUtils.createCircularIconGenerator(mActivity));

                    mItemsProvider = new BookmarkBarItemsProvider(modelAfterLoading, this);
                });
    }

    private void runIfStillRelevantAfterFinishLoadingBookmarkModel(
            BiConsumer<Profile, BookmarkModel> callback) {
        final var profile = mProfileSupplier.get();
        if (profile == null) return;

        final var model = BookmarkModel.getForProfile(profile);
        model.finishLoadingBookmarkModel(
                () -> {
                    // Ensure the active profile hasn't changed while loading the model.
                    final var profileAfterLoading = mProfileSupplier.get();
                    if (!Objects.equals(profile, profileAfterLoading)) return;

                    // Ensure the active model hasn't changed while loading the model.
                    final var modelAfterLoading = BookmarkModel.getForProfile(profileAfterLoading);
                    if (!Objects.equals(model, modelAfterLoading)) return;

                    // Run the callback iff the active profile and model are unchanged to avoid
                    // running the callback for the wrong profile/model.
                    callback.accept(profileAfterLoading, modelAfterLoading);
                });
    }

    private void updateItemMaxWidth() {
        mItemMaxWidthChangeCallback.onResult(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bookmark_bar_item_max_width));
    }

    // TODO(crbug.com/339492600): Replace w/ positioning construct akin to `BottomControlsStacker`.
    private void updateTopMargin() {
        // NOTE: Top controls height is the sum of all top browser control heights which includes
        // that of the bookmark bar. Subtract the bookmark bar's height from the top controls height
        // when calculating top margin in order to bottom align the bookmark bar relative to other
        // top browser controls.
        mModel.set(
                BookmarkBarProperties.TOP_MARGIN,
                mBrowserControlsStateProvider.getTopControlsHeight()
                        - assumeNonNull(mHeightSupplier.get()));
    }

    private void updateVisibility() {
        mModel.set(
                BookmarkBarProperties.VISIBILITY,
                mBrowserControlsStateProvider.getTopControlOffset() == 0
                        ? View.VISIBLE
                        : View.GONE);
    }
}
