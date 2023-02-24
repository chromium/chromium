// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;

/**
 * The new bookmark manager that is planned to replace the existing bookmark manager. It holds all
 * views and shared logics between tablet and phone. For tablet/phone specific logics, see
 * {@link BookmarkActivity} (phone) and {@link BookmarkPage} (tablet).
 */
public class BookmarkManager implements SearchDelegate, BackPressHandler {
    private static final int FAVICON_MAX_CACHE_SIZE_BYTES =
            10 * ConversionUtils.BYTES_PER_MEGABYTE; // 10MB

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private final ViewGroup mMainView;
    private final SelectableListLayout<BookmarkId> mSelectableListLayout;
    private final RecyclerView mRecyclerView;
    private final BookmarkActionBar mToolbar;
    private final BookmarkOpener mBookmarkOpener;
    private final BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private final BookmarkManagerMediator mMediator;
    private final BookmarkUndoController mUndoController;

    /**
     * Creates an instance of {@link BookmarkManager}. It also initializes resources,
     * bookmark models and jni bridges.
     * @param context The current {@link Context} used to obtain resources or inflate views.
     * @param openBookmarkComponentName The component to use when opening a bookmark.
     * @param isDialogUi Whether the main bookmarks UI will be shown in a dialog, not a NativePage.
     * @param isIncognito Whether the tab model loading the bookmark manager is for incognito mode.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     * @param profile The profile which the manager is running in.
     */
    public BookmarkManager(Context context, ComponentName openBookmarkComponentName,
            boolean isDialogUi, boolean isIncognito, SnackbarManager snackbarManager,
            Profile profile) {
        mMainView = (ViewGroup) LayoutInflater.from(context).inflate(R.layout.bookmark_main, null);
        BookmarkModel bookmarkModel = BookmarkModel.getForProfile(profile);
        if (ShoppingFeatures.isShoppingListEligible()) {
            ShoppingServiceFactory.getForProfile(profile).scheduleSavedProductUpdate();
        }

        SelectionDelegate selectionDelegate = new SelectionDelegate<BookmarkId>() {
            @Override
            public boolean toggleSelectionForItem(BookmarkId bookmark) {
                if (bookmarkModel.getBookmarkById(bookmark) != null
                        && !bookmarkModel.getBookmarkById(bookmark).isEditable()) {
                    return false;
                }
                return super.toggleSelectionForItem(bookmark);
            }
        };

        @SuppressWarnings("unchecked")
        SelectableListLayout<BookmarkId> selectableList =
                mMainView.findViewById(R.id.selectable_list);
        mSelectableListLayout = selectableList;
        mSelectableListLayout.initializeEmptyView(R.string.bookmarks_folder_empty);

        mBookmarkManagerCoordinator = new BookmarkManagerCoordinator(profile, snackbarManager);

        BookmarkItemsAdapter bookmarkItemsAdapter = new BookmarkItemsAdapter(context, profile);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(
                (RecyclerView.Adapter<RecyclerView.ViewHolder>) bookmarkItemsAdapter);

        mToolbar = (BookmarkActionBar) mSelectableListLayout.initializeToolbar(
                R.layout.bookmark_action_bar, selectionDelegate, 0, R.id.normal_menu_group,
                R.id.selection_mode_menu_group, null, isDialogUi);
        mToolbar.initializeSearchView(
                this, R.string.bookmark_action_bar_search, R.id.search_menu_id);
        mSelectableListLayout.configureWideDisplayStyle();

        LargeIconBridge largeIconBridge = new LargeIconBridge(profile);
        largeIconBridge.createCache(computeCacheMaxSize());

        mUndoController = new BookmarkUndoController(context, bookmarkModel, snackbarManager);
        mBookmarkOpener = new BookmarkOpener(bookmarkModel, context, openBookmarkComponentName);
        mMediator = new BookmarkManagerMediator(context, bookmarkModel, mBookmarkOpener,
                mSelectableListLayout, selectionDelegate, mRecyclerView, bookmarkItemsAdapter,
                mToolbar, largeIconBridge, isDialogUi, isIncognito, mBackPressStateSupplier,
                mBookmarkManagerCoordinator::createView);
        mBookmarkManagerCoordinator.initialize(
                mMediator, bookmarkItemsAdapter.getPromoHeaderManager());

        RecordUserAction.record("MobileBookmarkManagerOpen");
        if (!isDialogUi) {
            RecordUserAction.record("MobileBookmarkManagerPageOpen");
        }
    }

    // Public API implementation.

    /**
     * Destroys and cleans up itself. This must be called after done using this class.
     */
    public void onDestroyed() {
        RecordUserAction.record("MobileBookmarkManagerClose");
        mSelectableListLayout.onDestroyed();
        mMediator.onDestroy();
    }

    /**
     * @return The view that shows the main browsing history UI.
     */
    public View getView() {
        return mMainView;
    }

    /**
     * Sets the listener that reacts upon the change of the UI state of bookmark manager.
     */
    // TODO(crbug.com/1418859): Create abstraction between BookmarkManager & BasicNativePage.
    public void setBasicNativePage(BasicNativePage nativePage) {
        mMediator.setBasicNativePage(nativePage);
    }

    /**
     * Updates UI based on the new URL. If the bookmark model is not loaded yet, cache the url and
     * it will be picked up later when the model is loaded. This method is supposed to align with
     * {@link BookmarkPage#updateForUrl(String)}
     * <p>
     * @param url The url to navigate to.
     */
    public void updateForUrl(String url) {
        mMediator.updateForUrl(url);
    }

    /**
     * Called when the user presses the back key. This is only going to be called on Phone.
     * @return True if manager handles this event, false if it decides to ignore.
     */
    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    /** Opens the given BookmarkId. */
    public void openBookmark(BookmarkId bookmarkId) {
        mMediator.openBookmark(bookmarkId);
    }

    // BackPressHandler implementation.

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    // SearchDelegate implementation.

    @Override
    public void onSearchTextChanged(String query) {
        mMediator.onSearchTextChanged(query);
    }

    @Override
    public void onEndSearch() {
        mMediator.onEndSearch();
    }

    // Private methods.

    private int computeCacheMaxSize() {
        ActivityManager activityManager =
                ((ActivityManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.ACTIVITY_SERVICE));
        return Math.min(activityManager.getMemoryClass() / 4 * ConversionUtils.BYTES_PER_MEGABYTE,
                FAVICON_MAX_CACHE_SIZE_BYTES);
    }

    // Testing methods.

    @VisibleForTesting
    public BookmarkActionBar getToolbarForTests() {
        return mToolbar;
    }

    @VisibleForTesting
    public BookmarkUndoController getUndoControllerForTests() {
        return mUndoController;
    }

    @VisibleForTesting
    public RecyclerView getRecyclerViewForTests() {
        return mRecyclerView;
    }

    public static void preventLoadingForTesting(boolean preventLoading) {
        BookmarkManagerMediator.preventLoadingForTesting(preventLoading);
    }

    public BookmarkOpener getBookmarkOpenerForTesting() {
        return mBookmarkOpener;
    }

    public BookmarkDelegate getBookmarkDelegateForTesting() {
        return mMediator;
    }
}
