// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.components.browser_ui.widget.ListItemBuilder.buildSimpleMenuItem;

import android.app.Activity;
import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.LifecycleOwner;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkMetrics.BookmarkManagerFilter;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.Observer;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRow.Location;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksReader;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.ui.batch_upload_card.BatchUploadCardCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DragListener;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DraggabilityProvider;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionsObserver;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.DeviceInput;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Deque;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.function.BooleanSupplier;
import java.util.function.Consumer;
import java.util.function.Predicate;

/** Responsible for BookmarkManager business logic. */
// TODO(crbug.com/40256938): Remove BookmarkDelegate if possible.
@NullMarked
class BookmarkManagerMediator
        implements BookmarkDelegate, PartnerBookmarksReader.FaviconUpdateObserver {
    private static final int PROMO_MAX_INDEX = 1;
    private static final int SEARCH_BOX_MAX_INDEX = 0;

    private static boolean sPreventLoadingForTesting;

    /** Keeps track of whether drag is enabled / active for bookmark lists. */
    private class BookmarkDragStateDelegate implements DragStateDelegate {
        private BookmarkDelegate mBookmarkDelegate;
        private SelectionDelegate<BookmarkId> mSelectionDelegate;

        @Initializer
        void onBookmarkDelegateInitialized(BookmarkDelegate delegate) {
            mBookmarkDelegate = delegate;
            mSelectionDelegate = delegate.getSelectionDelegate();
        }

        // DragStateDelegate implementation
        @Override
        public boolean getDragEnabled() {
            boolean enabled =
                    !AccessibilityState.isPerformGesturesEnabled()
                            && mBookmarkDelegate.getCurrentUiMode() == BookmarkUiMode.FOLDER;
            return enabled
                    && mBookmarkUiPrefs.getBookmarkRowSortOrder() == BookmarkRowSortOrder.MANUAL
                    && mCurrentPowerFilter.isEmpty();
        }

        @Override
        public boolean getDragActive() {
            return getDragEnabled() && mSelectionDelegate.isSelectionEnabled();
        }
    }

    private final BookmarkModelObserver mBookmarkModelObserver =
            new BookmarkModelObserver() {
                @Override
                public void bookmarkNodeChildrenReordered(BookmarkItem node) {
                    if (!mIsBookmarkModelReorderingInProgress) {
                        mPendingRefresh.post();
                    }
                }

                @Override
                public void bookmarkNodeRemoved(
                        BookmarkItem parent,
                        int oldIndex,
                        BookmarkItem node,
                        boolean isDoingExtensiveChanges) {
                    clearHighlight();

                    BookmarkId id = node.getId();
                    boolean isTabletSearch =
                            DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                                    && !TextUtils.isEmpty(getCurrentSearchText());

                    if (getCurrentUiMode() == BookmarkUiMode.SEARCHING || isTabletSearch) {
                        // We cannot rely on removing the specific list item that corresponds to the
                        // removed node because the node might be a parent with children also shown
                        // in the list.
                        mPendingRefresh.post();
                    } else if (getCurrentUiMode() == BookmarkUiMode.FOLDER) {
                        // If the folder is removed in folder mode, show the parent folder or falls
                        // back to all bookmarks mode.
                        if (Objects.equals(id, getCurrentFolderId())) {
                            if (mBookmarkModel.getTopLevelFolderIds().contains(id)) {
                                BookmarkId defaultFolder =
                                        mBookmarkModel.getDefaultFolderViewLocation();
                                assumeNonNull(defaultFolder);
                                openFolder(defaultFolder);
                            } else {
                                openFolder(parent.getId());
                            }
                        } else {
                            int position = getPositionForBookmark(id);
                            // If the position couldn't be found, then do a full refresh. Otherwise
                            // be smart and remove only the index of the removed bookmark.
                            if (position == -1) {
                                mPendingRefresh.post();
                            } else {
                                mModelList.removeAt(position);
                                updateAllLocations();

                                // If the deleted node was selection, unselect it.
                                if (mSelectionDelegate.isItemSelected(id)) {
                                    mSelectionDelegate.toggleSelectionForItem(id);
                                }
                                // Update the batch upload card (in case of refresh() is not called)
                                // to reflect the right number of the
                                // local bookmarks.
                                if (mBatchUploadCardCoordinator != null) {
                                    mBatchUploadCardCoordinator
                                            .immediatelyHideBatchUploadCardAndUpdateItsVisibility();
                                }
                            }
                        }
                    }
                }

                @Override
                public void bookmarkNodeChanged(BookmarkItem item) {
                    clearHighlight();

                    BookmarkId id = item.getId();
                    if (getPositionForBookmark(id) == -1 && mSelectionDelegate.isItemSelected(id)) {
                        mSelectionDelegate.toggleSelectionForItem(id);
                    }

                    if (getCurrentUiMode() == BookmarkUiMode.FOLDER
                            && Objects.equals(id, getCurrentFolderId())) {
                        mPendingRefresh.post();
                    } else {
                        super.bookmarkNodeChanged(item);
                    }
                }

                @Override
                public void bookmarkModelChanged() {
                    clearHighlight();
                    mPendingRefresh.post();
                }
            };

    private final Deque<BookmarkUiState> mStateStack =
            new ArrayDeque<>() {
                @Override
                public void addLast(BookmarkUiState item) {
                    // The back press state depends on the size of stack. So push/pop item first in
                    // order to keep the size update-to-date.
                    super.addLast(item);
                    onBackPressStateChanged();
                }

                @Override
                public synchronized BookmarkUiState removeLast() {
                    var state = super.removeLast();
                    onBackPressStateChanged();
                    return state;
                }
            };

    private final BookmarkUiObserver mBookmarkUiObserver =
            new BookmarkUiObserver() {
                @Override
                public void onDestroy() {
                    removeUiObserver(mBookmarkUiObserver);
                    getSelectionDelegate().removeObserver(mSelectionObserver);
                    if (mPromoHeaderManager != null) {
                        mPromoHeaderManager.destroy();
                    }
                }

                @Override
                public void onFolderStateSet(@Nullable BookmarkId folder) {
                    clearHighlight();

                    mDragReorderableRecyclerViewAdapter.enableDrag();

                    BookmarkId currentId = assumeNonNull(getCurrentFolderId());
                    setBookmarks(
                            mBookmarkQueryHandler.buildBookmarkListForParent(
                                    currentId, mCurrentPowerFilter));
                    setSearchTextAndUpdateButtonVisibility("");
                    clearSearchBoxFocus();
                    if (!mIsExitingSearch) {
                        maybeAutoFocusSearchBox();
                    }
                }
            };

    private final SelectionObserver<BookmarkId> mSelectionObserver =
            new SelectionObserver<>() {
                @Override
                public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
                    clearHighlight();

                    if (mIsSelectionEnabled != mSelectionDelegate.isSelectionEnabled()) {
                        changeSelectionMode(mSelectionDelegate.isSelectionEnabled());
                    }
                }
            };

    private final DragListener mDragListener =
            new DragListener() {
                @Override
                public void onSwap() {
                    mIsBookmarkModelReorderingInProgress = true;
                    try {
                        setOrder();
                    } finally {
                        mIsBookmarkModelReorderingInProgress = false;
                    }
                }
            };

    private final DraggabilityProvider mDraggabilityProvider =
            new DraggabilityProvider() {
                @Override
                public boolean isActivelyDraggable(PropertyModel propertyModel) {
                    return isPassivelyDraggable(propertyModel);
                }

                @Override
                public boolean isPassivelyDraggable(PropertyModel propertyModel) {
                    BookmarkListEntry bookmarkListEntry =
                            propertyModel.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
                    BookmarkItem bookmarkItem = assumeNonNull(bookmarkListEntry.getBookmarkItem());
                    return bookmarkItem.isReorderable();
                }
            };

    private final BookmarkUiPrefs.Observer mBookmarkUiPrefsObserver =
            new Observer() {
                @Override
                public void onBookmarkRowDisplayPrefChanged(
                        @BookmarkRowDisplayPref int displayPref) {
                    refresh();
                }

                @Override
                public void onBookmarkRowSortOrderChanged(@BookmarkRowSortOrder int sortOrder) {
                    refresh();
                }
            };

    private final SubscriptionsObserver mSubscriptionsObserver =
            new SubscriptionsObserver() {
                @Override
                public void onSubscribe(CommerceSubscription subscription, boolean succeeded) {
                    // Bookmark updates are pushed prior to subscriptions being updated, so we can
                    // safely check the folder for product items before initiating a full refresh of
                    // the list. The same applies for the unsubscribe event below.
                    if (hasShoppingItems(mModelList)) {
                        mPendingRefresh.post();
                    }
                }

                @Override
                public void onUnsubscribe(CommerceSubscription subscription, boolean succeeded) {
                    if (hasShoppingItems(mModelList)) {
                        mPendingRefresh.post();
                    }
                }

                private static boolean hasShoppingItems(ModelList list) {
                    for (ListItem item : list) {
                        if (isShoppingItem(item)) {
                            return true;
                        }
                    }
                    return false;
                }

                private static boolean isShoppingItem(ListItem item) {
                    if (!item.model.containsKey(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY)
                            || item.model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY)
                                    == null) {
                        return false;
                    }
                    PowerBookmarkMeta meta =
                            item.model
                                    .get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY)
                                    .getPowerBookmarkMeta();
                    return meta != null && meta.hasShoppingSpecifics();
                }
            };

    private final ObserverList<BookmarkUiObserver> mUiObservers = new ObserverList<>();
    private final BookmarkDragStateDelegate mDragStateDelegate = new BookmarkDragStateDelegate();
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkOpener mBookmarkOpener;
    // TODO(crbug.com/40256938): Remove reference to SelectableListLayout.
    // Owned by BookmarkManager(Coordinator).
    private final SelectableListLayout<BookmarkId> mSelectableListLayout;
    private final SelectionDelegate<BookmarkId> mSelectionDelegate;
    // TODO(crbug.com/40256938): Remove reference to RecyclerView.
    // Owned by BookmarkManager(Coordinator).
    private final RecyclerView mRecyclerView;
    private final DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    // Whether we're showing in a dialog UI which is only true for phones.
    private final boolean mIsDialogUi;
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier;
    private final Profile mProfile;
    private final @Nullable BookmarkPromoHeader mPromoHeaderManager;
    private final BookmarkUndoController mBookmarkUndoController;
    private final BookmarkQueryHandler mBookmarkQueryHandler;
    private final ModelList mModelList;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final Runnable mHideKeyboardRunnable;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final ShoppingService mShoppingService;
    private final SnackbarManager mSnackbarManager;
    private final BooleanSupplier mCanShowSigninPromo;
    private final ImprovedBookmarkRowCoordinator mImprovedBookmarkRowCoordinator;
    private final Set<PowerBookmarkType> mCurrentPowerFilter = new HashSet<>();
    private final CallbackController mCallbackController = new CallbackController();
    private final PendingRunnable mPendingRefresh =
            new PendingRunnable(
                    TaskTraits.UI_DEFAULT, mCallbackController.makeCancelable(this::refresh));
    private final BookmarkManagerOpener mBookmarkManagerOpener;
    private final PriceDropNotificationManager mPriceDropNotificationManager;

    private @Nullable BatchUploadCardCoordinator mBatchUploadCardCoordinator;
    // Whether this instance has been destroyed.
    private boolean mIsDestroyed;
    private boolean mIsExitingSearch;
    private @Nullable String mInitialUrl;
    private boolean mFaviconsNeedRefresh;
    private @Nullable BasicNativePage mNativePage;
    // Keep track of the currently highlighted bookmark - used for "show in folder" action.
    private @Nullable BookmarkId mHighlightedBookmark;
    // If selection is currently enabled in the bookmarks manager.
    private boolean mIsSelectionEnabled;
    // Track if we're the source of bookmark model reordering so the event can be ignored.
    private boolean mIsBookmarkModelReorderingInProgress;
    // Whether the shopping feature is available and there are price-tracked bookmarks.
    private boolean mShoppingFilterAvailable;

    BookmarkManagerMediator(
            Activity activity,
            LifecycleOwner lifecycleOwner,
            ModalDialogManager modalDialogManager,
            BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener,
            SelectableListLayout<BookmarkId> selectableListLayout,
            SelectionDelegate<BookmarkId> selectionDelegate,
            RecyclerView recyclerView,
            DragReorderableRecyclerViewAdapter dragReorderableRecyclerViewAdapter,
            boolean isDialogUi,
            ObservableSupplierImpl<Boolean> backPressStateSupplier,
            Profile profile,
            BookmarkUndoController bookmarkUndoController,
            ModelList modelList,
            BookmarkUiPrefs bookmarkUiPrefs,
            Runnable hideKeyboardRunnable,
            BookmarkImageFetcher bookmarkImageFetcher,
            ShoppingService shoppingService,
            SnackbarManager snackbarManager,
            BooleanSupplier canShowSigninPromo,
            Consumer<OnScrollListener> onScrollListenerConsumer,
            BookmarkManagerOpener bookmarkManagerOpener,
            PriceDropNotificationManager priceDropNotificationManager) {
        mContext = activity;
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        mBookmarkOpener = bookmarkOpener;
        mSelectableListLayout = selectableListLayout;
        mSelectableListLayout
                .getHandleBackPressChangedSupplier()
                .addObserver((x) -> onBackPressStateChanged());
        mSelectionDelegate = selectionDelegate;
        mRecyclerView = recyclerView;
        mDragReorderableRecyclerViewAdapter = dragReorderableRecyclerViewAdapter;
        mDragReorderableRecyclerViewAdapter.addDragListener(mDragListener);
        mDragReorderableRecyclerViewAdapter.setLongPressDragDelegate(
                () -> mDragStateDelegate.getDragActive());
        mIsDialogUi = isDialogUi;
        mBackPressStateSupplier = backPressStateSupplier;
        mProfile = profile;
        mModelList = modelList;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mBookmarkUiPrefs.addObserver(mBookmarkUiPrefsObserver);
        mHideKeyboardRunnable = hideKeyboardRunnable;
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mShoppingService = shoppingService;
        mSnackbarManager = snackbarManager;
        mCanShowSigninPromo = canShowSigninPromo;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            OneshotSupplierImpl<SnackbarManager> snackbarManagerSupplierImpl =
                    new OneshotSupplierImpl<>();
            snackbarManagerSupplierImpl.set(mSnackbarManager);
            mBatchUploadCardCoordinator =
                    new BatchUploadCardCoordinator(
                            activity,
                            lifecycleOwner,
                            modalDialogManager,
                            mProfile.getOriginalProfile(),
                            snackbarManagerSupplierImpl,
                            this::updateBatchUploadCard,
                            BatchUploadCardCoordinator.EntryPoint.BOOKMARK_MANAGER);
            mPromoHeaderManager = null;
        } else {
            mPromoHeaderManager =
                    new BookmarkPromoHeader(
                            mContext, mProfile.getOriginalProfile(), this::updateHeader);
        }
        mBookmarkUndoController = bookmarkUndoController;
        mBookmarkManagerOpener = bookmarkManagerOpener;
        mPriceDropNotificationManager = priceDropNotificationManager;
        if (CommerceFeatureUtils.isShoppingListEligible(mShoppingService)) {
            mShoppingService.addSubscriptionsObserver(mSubscriptionsObserver);
        }

        mBookmarkQueryHandler =
                new ImprovedBookmarkQueryHandler(
                        mBookmarkModel,
                        bookmarkUiPrefs,
                        mShoppingService,
                        /* rootFolderForceVisibleMask= */ BookmarkBarUtils
                                        .isDeviceBookmarkBarCompatible(mContext)
                                ? BookmarkNodeMaskBit.ACCOUNT_AND_LOCAL_BOOKMARK_BAR
                                : BookmarkNodeMaskBit.NONE);

        onScrollListenerConsumer.accept(
                new OnScrollListener() {
                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        if (dy > 0) {
                            clearSearchBoxFocus();
                        }
                    }
                });

        // Previously we were waiting for BookmarkModel to be loaded, but it's not necessary.
        PartnerBookmarksReader.addFaviconUpdateObserver(this);

        mImprovedBookmarkRowCoordinator =
                new ImprovedBookmarkRowCoordinator(
                        mContext,
                        mBookmarkImageFetcher,
                        mBookmarkModel,
                        mBookmarkUiPrefs,
                        mShoppingService);

        initializeToLoadingState();
        if (!sPreventLoadingForTesting) {
            finishLoadingBookmarkModel();
        }
    }

    void onBookmarkModelLoaded() {
        mDragStateDelegate.onBookmarkDelegateInitialized(this);

        updateShoppingFilterVisible();

        // TODO(https://crbug.com/1413463): This logic is here to keep the same execution order
        // from when it was in the original adapter. It doesn't conceptually make sense to be here,
        // and should happen earlier.
        addUiObserver(mBookmarkUiObserver);
        mSelectionDelegate.addObserver(mSelectionObserver);

        if (!TextUtils.isEmpty(mInitialUrl)) {
            setState(BookmarkUiState.createStateFromUrl(mInitialUrl, mBookmarkModel));
        }
    }

    void onDestroy() {
        mIsDestroyed = true;
        mBookmarkModel.removeObserver(mBookmarkModelObserver);

        mBookmarkImageFetcher.destroy();
        PartnerBookmarksReader.removeFaviconUpdateObserver(this);

        mBookmarkUndoController.destroy();
        mBookmarkQueryHandler.destroy();
        mCallbackController.destroy();
        if (mBatchUploadCardCoordinator != null) {
            mBatchUploadCardCoordinator.destroy();
        }

        mBookmarkUiPrefs.removeObserver(mBookmarkUiPrefsObserver);

        if (mShoppingService != null
                && CommerceFeatureUtils.isShoppingListEligible(mShoppingService)) {
            mShoppingService.removeSubscriptionsObserver(mSubscriptionsObserver);
        }

        for (BookmarkUiObserver observer : mUiObservers) {
            observer.onDestroy();
        }
        assert mUiObservers.size() == 0;
    }

    void onAttachedToWindow() {
        mBookmarkUndoController.setEnabled(true);
        maybeAutoFocusSearchBox();
        // Immediately re-calculate and set the back press state
        // upon attachment to ensure the supplier is not stale.
        onBackPressStateChanged();
    }

    void onDetachedFromWindow() {
        mBookmarkUndoController.setEnabled(false);
        // Explicitly disable the back press handler when the view is detached.
        // This tells the BackPressManager to ignore this handler even if another
        // component triggers an observer update in the background.
        mBackPressStateSupplier.set(false);
    }

    /** See BookmarkManager(Coordinator)#onBackPressed. */
    boolean onBackPressed() {
        if (mIsDestroyed) return false;

        if (mSelectableListLayout.onBackPressed()) {
            return true;
        }

        // Selectable list layout is not handling back presses for this condition
        // !mToolbar.isLargeScreenWithKeyboard(). That causes back press events not to be consumed.
        // TODO(crbug.com/444674420): Unify back press logic under SelectableListLayout.
        if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
                && DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            String searchText = getCurrentSearchText();
            if (!TextUtils.isEmpty(searchText)) {
                onClearSearchTextRunnable();
                return true;
            }
        }

        if (!mStateStack.isEmpty()) {
            mStateStack.removeLast();
            if (!mStateStack.isEmpty()) {
                setState(mStateStack.removeLast());
                return true;
            }
        }
        return false;
    }

    /**
     * Handles the "Escape" key press. - On tablets: Clears the search bar if it contains text. Does
     * nothing otherwise. - On non-tablets: Behaves identically to a standard back press.
     *
     * @return True if the event was consumed, false otherwise.
     */
    boolean onEscapePressed() {
        assert ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)
                : "This path should only be reached when the feature flag is enabled.";

        if (mIsDestroyed) return false;

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            // Escape ONLY clears the search filter. It does not navigate back.
            String searchText = getCurrentSearchText();
            if (!TextUtils.isEmpty(searchText)) {
                onClearSearchTextRunnable();
                return true;
            }
            // If search is empty on a tablet, Escape does nothing.
            return false;
        } else {
            // Escape behaves exactly the same as the back button.
            return onBackPressed();
        }
    }

    void onPromoVisibilityChange() {
        updateHeader();
    }

    /** See BookmarkManager(Coordinator)#setBasicNativePage. */
    void setBasicNativePage(BasicNativePage nativePage) {
        mNativePage = nativePage;
    }

    /** See BookmarkManager(Coordinator)#updateForUrl */
    void updateForUrl(String url) {
        // Bookmark model is null if the manager has been destroyed.
        if (mBookmarkModel == null) return;

        if (mBookmarkModel.isBookmarkModelLoaded()) {
            BookmarkUiState searchState = null;
            if (getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
                searchState = mStateStack.removeLast();
            }

            setState(BookmarkUiState.createStateFromUrl(url, mBookmarkModel));

            if (searchState != null) setState(searchState);
        } else {
            mInitialUrl = url;
        }
    }

    @Nullable BookmarkPromoHeader getPromoHeaderManager() {
        return mPromoHeaderManager;
    }

    @Nullable BookmarkId getIdByPosition(int position) {
        BookmarkListEntry entry = getItemByPosition(position);
        if (entry == null || entry.getBookmarkItem() == null) return null;
        return entry.getBookmarkItem().getId();
    }

    /**
     * Synchronously searches for the given query.
     *
     * @param query The query text to search for.
     */
    void search(@Nullable String query) {
        onSearchTextChangeCallback(query);
    }

    public void setOrder() {
        assert !topLevelFoldersShowing() : "Cannot reorder top-level folders!";
        BookmarkId currentId = assumeNonNull(getCurrentFolderId());
        assert currentId.getType() != BookmarkType.READING_LIST : "Cannot reorder reading list!";
        assert currentId.getType() != BookmarkType.PARTNER : "Cannot reorder partner bookmarks!";
        assert getCurrentUiMode() == BookmarkUiMode.FOLDER
                : "Can only reorder items from folder mode!";

        int startIndex = getBookmarkItemStartIndex();
        int endIndex = getBookmarkItemEndIndex();

        // Get the new order for the IDs.
        List<Long> newOrder = new ArrayList<>(endIndex - startIndex + 1);
        for (int i = startIndex; i <= endIndex; i++) {
            BookmarkItem bookmarkItem = assumeNonNull(getItemByPosition(i).getBookmarkItem());
            // The partner bookmark folder is under "Mobile bookmarks", but can't be reordered.
            if (!bookmarkItem.isReorderable()) {
                assert i == endIndex
                        : "Partner bookmarks should always be at the end of the list when mobile"
                                + " bookmark children are re-ordered.";
                continue;
            }
            newOrder.add(bookmarkItem.getId().getId());
        }
        long[] newOrderArr = new long[newOrder.size()];
        for (int i = 0; i < newOrder.size(); i++) {
            newOrderArr[i] = newOrder.get(i);
        }
        mBookmarkModel.reorderBookmarks(getCurrentFolderId(), newOrderArr);
        if (mDragStateDelegate.getDragActive()) {
            RecordUserAction.record("MobileBookmarkManagerDragReorder");
        }

        updateAllLocations();
    }

    public boolean isReorderable(BookmarkListEntry entry) {
        if (!mCurrentPowerFilter.isEmpty()) {
            return false;
        }

        return entry != null
                && entry.getBookmarkItem() != null
                && entry.getBookmarkItem().isReorderable();
    }

    DraggabilityProvider getDraggabilityProvider() {
        return mDraggabilityProvider;
    }

    // BookmarkDelegate implementation.

    @Override
    public void moveDownOne(BookmarkId bookmarkId) {
        int pos = getPositionForBookmark(bookmarkId);
        assert isReorderable(getItemByPosition(pos));
        mModelList.move(pos, pos + 1);
        setOrder();
    }

    @Override
    public void moveUpOne(BookmarkId bookmarkId) {
        int pos = getPositionForBookmark(bookmarkId);
        assert isReorderable(getItemByPosition(pos));
        mModelList.move(pos, pos - 1);
        setOrder();
    }

    @Override
    public void onBookmarkItemMenuOpened() {
        mHideKeyboardRunnable.run();
    }

    @Override
    public boolean isDialogUi() {
        return mIsDialogUi;
    }

    @Override
    public void openFolder(BookmarkId folder) {
        RecordUserAction.record("MobileBookmarkManagerOpenFolder");
        setState(BookmarkUiState.createFolderState(folder, mBookmarkModel));
        mRecyclerView.scrollToPosition(0);
    }

    @Override
    public SelectionDelegate<BookmarkId> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    @Override
    public SelectableListLayout<BookmarkId> getSelectableListLayout() {
        return mSelectableListLayout;
    }

    @Override
    public void notifyStateChange(BookmarkUiObserver observer) {
        final @BookmarkUiMode int state = getCurrentUiMode();
        observer.onUiModeChanged(state);
        switch (state) {
            case BookmarkUiMode.LOADING:
                break;
            case BookmarkUiMode.FOLDER:
                observer.onFolderStateSet(getCurrentFolderId());
                break;
            case BookmarkUiMode.SEARCHING:
                clearHighlight();
                mDragReorderableRecyclerViewAdapter.disableDrag();
                // Promo and headers should not appear in search mode.
                removePromoAndSectionHeaders();
                break;
            default:
                assert false : "State not valid";
                break;
        }
    }

    @Override
    public void openBookmark(BookmarkId bookmark) {
        if (!mBookmarkOpener.openBookmarkInCurrentTab(bookmark, mProfile.isOffTheRecord())) return;

        // Close bookmark UI. Keep the reading list page open.
        if (bookmark != null && bookmark.getType() != BookmarkType.READING_LIST) {
            mBookmarkManagerOpener.finishActivityOnPhone(mContext);
        }
    }

    @Override
    public void openBookmarksInNewTabs(List<BookmarkId> bookmarks, boolean incognito) {
        if (mBookmarkOpener.openBookmarksInNewTabs(bookmarks, incognito)) {
            mBookmarkManagerOpener.finishActivityOnPhone(mContext);
        }
    }

    @Override
    public void openSearchUi() {
        onSearchTextChangeCallback("");
    }

    @Override
    public void addUiObserver(BookmarkUiObserver observer) {
        mUiObservers.addObserver(observer);
        notifyStateChange(observer);
    }

    @Override
    public void removeUiObserver(BookmarkUiObserver observer) {
        mUiObservers.removeObserver(observer);
    }

    @Override
    public BookmarkModel getModel() {
        return mBookmarkModel;
    }

    @Override
    public @BookmarkUiMode int getCurrentUiMode() {
        BookmarkUiState state = mStateStack.peekLast();
        return state == null ? BookmarkUiMode.LOADING : state.mUiMode;
    }

    @Override
    public DragStateDelegate getDragStateDelegate() {
        return mDragStateDelegate;
    }

    @Override
    public void highlightBookmark(BookmarkId bookmarkId) {
        assert mHighlightedBookmark == null : "There should not already be a highlighted bookmark!";

        int index = getPositionForBookmark(bookmarkId);
        mRecyclerView.scrollToPosition(index);
        mHighlightedBookmark = bookmarkId;
        mModelList.get(index).model.set(BookmarkManagerProperties.IS_HIGHLIGHTED, true);
    }

    // SearchDelegate implementation.
    // Actual interface implemented in BookmarkManager(Coordinator).

    void onEndSearch() {
        // Pop the search state off the stack.
        mStateStack.removeLast();

        // Set the state back to the folder that was previously being viewed. Listeners will be
        // notified of the change and the list of bookmarks will be updated.
        mIsExitingSearch = true;
        setState(mStateStack.removeLast());
        mIsExitingSearch = false;
    }

    // PartnerBookmarksReader.FaviconUpdateObserver implementation.

    @Override
    public void onUpdateFavicon(String url) {
        assert mBookmarkModel.isBookmarkModelLoaded();
        mFaviconsNeedRefresh = true;
    }

    @Override
    public void onCompletedFaviconLoading() {
        assert mBookmarkModel.isBookmarkModelLoaded();
        if (mFaviconsNeedRefresh) {
            refresh();
            mFaviconsNeedRefresh = false;
        }
    }

    // Private methods.

    /**
     * Puts all UI elements to loading state. This state might be overridden synchronously by {@link
     * #updateForUrl(String)}, if the bookmark model is already loaded.
     */
    private void initializeToLoadingState() {
        assert mStateStack.isEmpty();
        setState(BookmarkUiState.createLoadingState());
    }

    /**
     * This is the ultimate internal method that updates UI and controls backstack. And it is the
     * only method that pushes states to {@link #mStateStack}.
     *
     * <p>If the given state is not valid, all_bookmark state will be shown. Afterwards, this method
     * checks the current state: if currently in loading state, it pops it out and adds the new
     * state to the back stack. It also notifies the {@link #mNativePage} (if any) that the url has
     * changed.
     *
     * <p>Also note that even if we store states to {@link #mStateStack}, on tablet the back
     * navigation and back button are not controlled by the manager: the tab handles back key and
     * backstack navigation.
     */
    private void setState(BookmarkUiState state) {
        if (!state.isValid(mBookmarkModel)) {
            BookmarkId defaultFolder = assumeNonNull(mBookmarkModel.getDefaultFolderViewLocation());
            state = BookmarkUiState.createFolderState(defaultFolder, mBookmarkModel);
        }

        @BookmarkUiMode int currentUiMode = getCurrentUiMode();
        @Nullable BookmarkUiState currentState = getCurrentUiState();
        if (Objects.equals(currentState, state)) return;

        // The loading state is not persisted in history stack and once we have a valid state it
        // shall be removed.
        if (!mStateStack.isEmpty() && currentUiMode == BookmarkUiMode.LOADING) {
            mStateStack.removeLast();
        }

        // TODO(crbug.com/40276748): Delete this empty search mechanism.
        // In the old UI, when the search menu item is pressed, and the search box initially
        // appears, there is no query string yet. And the old folder bookmarks should still show
        // until text is typed into the search box. After this point, empty query strings should
        // be searching for all bookmarks, not the old folder bookmarks.
        boolean preserveFolderBookmarksOnEmptySearch = false;
        // Don't queue multiple consecutive search states. Instead replace the previous with the new
        // one.
        if (currentUiMode == BookmarkUiMode.SEARCHING
                && state.mUiMode == BookmarkUiMode.SEARCHING) {
            mStateStack.removeLast();
        } else if (currentUiMode != BookmarkUiMode.SEARCHING
                && state.mUiMode == BookmarkUiMode.SEARCHING) {
            // The initial state change to search should clear selection.
            mSelectionDelegate.clearSelection();
        }

        // Search states should only be the top most state. Back button should not restore them.
        if (currentUiMode == BookmarkUiMode.SEARCHING && state.mUiMode == BookmarkUiMode.FOLDER) {
            mStateStack.removeLast();
        }

        mStateStack.addLast(state);
        notifyUi(state, preserveFolderBookmarksOnEmptySearch);
    }

    private void notifyUi(BookmarkUiState state, boolean preserveFolderBookmarksOnEmptySearch) {
        if (state.mUiMode == BookmarkUiMode.FOLDER) {
            // Loading and searching states may be pushed to the stack but should never be stored in
            // preferences.
            BookmarkUtils.setLastUsedUrl(state.mUrl);
            // If a loading state is replaced by another loading state, do not notify this change.
            if (mNativePage != null) {
                boolean replaceLastUrl =
                        TextUtils.equals(mNativePage.getUrl(), UrlConstants.BOOKMARKS_URL)
                                || TextUtils.equals(
                                        mNativePage.getUrl(), UrlConstants.BOOKMARKS_NATIVE_URL);
                mNativePage.onStateChange(state.mUrl, replaceLastUrl);
            }
        } else if (state.mUiMode == BookmarkUiMode.SEARCHING) {
            String searchText = getCurrentSearchText();
            if (!preserveFolderBookmarksOnEmptySearch || !TextUtils.isEmpty(searchText)) {
                String trimmedText = searchText == null ? "" : searchText.trim();
                setBookmarks(
                        mBookmarkQueryHandler.buildBookmarkListForSearch(
                                trimmedText, mCurrentPowerFilter));
            }
        }

        for (BookmarkUiObserver observer : mUiObservers) {
            notifyStateChange(observer);
        }
    }

    // TODO(lazzzis): This method can be moved to adapter after bookmark reordering launches.
    /**
     * Some bookmarks may be moved to another folder or removed in another devices. However, it may
     * still be stored by {@link #mSelectionDelegate}, which causes incorrect selection counting.
     */
    private void syncAdapterAndSelectionDelegate() {
        List<BookmarkId> selectedItems = mSelectionDelegate.getSelectedItemsAsList();
        Set<BookmarkId> removedIds = new HashSet<>();
        for (BookmarkId node : selectedItems) {
            if (mSelectionDelegate.isItemSelected(node) && getPositionForBookmark(node) == -1) {
                removedIds.add(node);
            }
        }

        if (!removedIds.isEmpty()) {
            Set<BookmarkId> retainIds = new HashSet<>(selectedItems);
            retainIds.removeAll(removedIds);
            mSelectionDelegate.setSelectedItems(retainIds);
        }
    }

    private void onBackPressStateChanged() {
        if (mIsDestroyed) {
            mBackPressStateSupplier.set(false);
            return;
        }

        // Condition 1: Is selection mode active?
        boolean selectionActive =
                Boolean.TRUE.equals(
                        mSelectableListLayout.getHandleBackPressChangedSupplier().get());

        // Condition 2: Can we navigate back in the folder stack?
        boolean canNavigateFolders = mStateStack.size() > 1;

        // Condition 3: Are we on a tablet and actively searching?
        // TODO(crbug.com/444674420): Unify back press logic under SelectableListLayout.
        boolean isSearchingOnTablet = false;
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            String searchText = getCurrentSearchText();
            isSearchingOnTablet = !TextUtils.isEmpty(searchText);
        }

        // The handler is enabled if ANY of these conditions are true.
        boolean isEnabled = selectionActive || canNavigateFolders || isSearchingOnTablet;

        mBackPressStateSupplier.set(isEnabled);
    }

    /**
     * @return The position of the given bookmark in adapter. Will return -1 if not found.
     */
    @VisibleForTesting
    int getPositionForBookmark(BookmarkId bookmark) {
        assert bookmark != null;
        int position = -1;
        for (int i = 0; i < getItemCount(); i++) {
            if (bookmark.equals(getIdByPosition(i))) {
                position = i;
                break;
            }
        }
        return position;
    }

    private void clearSearchBoxFocus() {
        setSearchBoxFocusAndHideKeyboardIfNeeded(false);
    }

    private @Nullable PropertyModel getSearchBoxPropertyModel() {
        int index = getCurrentSearchBoxIndex();
        return index < 0 ? null : mModelList.get(index).model;
    }

    @SuppressWarnings("NotifyDataSetChanged")
    private void setBookmarks(List<BookmarkListEntry> bookmarkListEntryList) {
        clearHighlight();

        // This method is called due to unknown model changes, and we're basically rebuilding every
        // row. However we need to avoid doing this in a way that'll cause flicker. So we replace
        // items in place so that the recycler view doesn't see everything being removed and added
        // back, but instead it sees items being changed.
        int index = 0;

        // Don't replace if it already exists. The text box is stateful.
        if (getCurrentSearchBoxIndex() < 0) {
            updateOrAdd(index, buildSearchBoxRow());
        } else {
            // Update the filter visibility if the search box is already built.
            updateSearchBoxShoppingFilterVisibility(assumeNonNull(getSearchBoxPropertyModel()));
        }
        index++;

        // Restore the header, if it exists, then update it.
        final @ViewType int targetPromoHeaderType = calculatePromoHeaderType();
        if (targetPromoHeaderType != ViewType.INVALID) {
            updateOrAdd(index++, buildPersonalizedPromoListItem(targetPromoHeaderType));
        }

        for (BookmarkListEntry bookmarkListEntry : bookmarkListEntryList) {
            updateOrAdd(index++, buildBookmarkListItem(bookmarkListEntry));
        }

        // Only show the empty state if there's only a searchbox.
        boolean listIsEmpty = index == 1;
        if (listIsEmpty) {
            updateOrAdd(index++, buildEmptyStateListItem());
        }

        if (mModelList.size() == 0 && index == 0) {
            // Bookmarks are loaded but we have no items. The SelectableListLayout should
            // hide the spinner, so force a notification.
            mDragReorderableRecyclerViewAdapter.notifyDataSetChanged();
        }

        if (mModelList.size() > index) {
            mModelList.removeRange(index, mModelList.size() - index);
        }

        // Add the batch upload card if possible.
        updateBatchUploadCard();

        updateAllLocations();
        syncAdapterAndSelectionDelegate();
    }

    private void updateOrAdd(int index, ListItem listItem) {
        if (mModelList.size() > index) {
            mModelList.update(index, listItem);
        } else {
            mModelList.add(index, listItem);
        }
    }

    private static boolean isMovable(BookmarkModel bookmarkModel, PropertyModel propertyModel) {
        BookmarkListEntry bookmarkListEntry =
                propertyModel.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
        if (bookmarkListEntry == null) return false;
        BookmarkItem bookmarkItem = bookmarkListEntry.getBookmarkItem();
        if (bookmarkItem == null) return false;
        return BookmarkUtils.isMovable(bookmarkModel, bookmarkItem);
    }

    private boolean isBookmarkRowType(@ViewType int viewType) {
        return viewType == ViewType.IMPROVED_BOOKMARK_COMPACT
                || viewType == ViewType.IMPROVED_BOOKMARK_VISUAL;
    }

    private int firstIndexWithPredicate(
            int start, int stop, int delta, Predicate<ListItem> predicate) {
        for (int i = start; i != stop; i += delta) {
            ListItem listItem = mModelList.get(i);
            if (predicate.test(listItem)) return i;
        }
        return -1;
    }

    private void updateAllLocations() {
        Predicate<ListItem> locationPredicate =
                (listItem) -> {
                    return isBookmarkRowType(listItem.type)
                            && isMovable(mBookmarkModel, listItem.model);
                };
        int startIndex = firstIndexWithPredicate(0, mModelList.size(), 1, locationPredicate);
        int lastIndex = firstIndexWithPredicate(mModelList.size() - 1, -1, -1, locationPredicate);
        if (startIndex < 0 || lastIndex < 0) {
            return;
        }

        if (startIndex == lastIndex) {
            mModelList.get(startIndex).model.set(BookmarkManagerProperties.LOCATION, Location.SOLO);
        } else {
            mModelList.get(startIndex).model.set(BookmarkManagerProperties.LOCATION, Location.TOP);
            mModelList
                    .get(lastIndex)
                    .model
                    .set(BookmarkManagerProperties.LOCATION, Location.BOTTOM);
        }

        for (int i = startIndex + 1; i < lastIndex; i++) {
            mModelList.get(i).model.set(BookmarkManagerProperties.LOCATION, Location.MIDDLE);
        }
    }

    /** Refresh the list of bookmarks within the currently visible folder. */
    private void refresh() {
        assert !mIsDestroyed;
        if (mStateStack.isEmpty()) return;

        // On tablets, a refresh during a search should re-run the search.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            String searchText = getCurrentSearchText();
            if (!TextUtils.isEmpty(searchText)) {
                setBookmarks(
                        mBookmarkQueryHandler.buildBookmarkListForSearch(
                                searchText, mCurrentPowerFilter));
                return;
            }
        }

        notifyUi(mStateStack.peekLast(), /* preserveFolderBookmarksOnEmptySearch= */ false);
    }

    private @ViewType int calculatePromoHeaderType() {
        final @BookmarkUiMode int currentUiState = getCurrentUiMode();
        if (currentUiState != BookmarkUiMode.FOLDER) {
            return ViewType.INVALID;
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            return mCanShowSigninPromo.getAsBoolean() ? ViewType.SIGNIN_PROMO : ViewType.INVALID;
        }

        if (mPromoHeaderManager != null && mPromoHeaderManager.shouldShowPromo()) {
            return ViewType.SIGNIN_PROMO;
        } else {
            return ViewType.INVALID;
        }
    }

    private boolean shouldShowBatchUploadCard() {
        if (mBatchUploadCardCoordinator == null) {
            return false;
        }
        return mBatchUploadCardCoordinator.shouldShowBatchUploadCard();
    }

    /**
     * Removes, adds, or updates the promo row, depending on the previous state and desired state.
     * Note that this method effectively duplicates the logic in {@link this#setBookmarks} that
     * understands the order of the promo header and the search row.
     */
    private void updateHeader() {
        final @ViewType int targetPromoHeaderType = calculatePromoHeaderType();
        int currentPromoIndex = getCurrentPromoHeaderIndex();

        if (targetPromoHeaderType == ViewType.INVALID) {
            if (currentPromoIndex >= 0) {
                mModelList.removeAt(currentPromoIndex);
            }
        } else {
            ListItem promoListItem = buildPersonalizedPromoListItem(targetPromoHeaderType);
            if (currentPromoIndex >= 0) {
                mModelList.update(currentPromoIndex, promoListItem);
            } else {
                boolean hasSearchRow = getCurrentSearchBoxIndex() >= 0;
                int targetPromoIndex = hasSearchRow ? 1 : 0;
                mModelList.add(targetPromoIndex, promoListItem);
            }
        }
    }

    private void updateBatchUploadCard() {
        if (getCurrentUiMode() != BookmarkUiMode.FOLDER || !topLevelFoldersShowing()) {
            return;
        }
        int currentBatchUploadCardIndex =
                searchForFirstIndexOfType(
                        /* endIndex= */ mModelList.size() - 1,
                        viewType -> viewType == ViewType.BATCH_UPLOAD_CARD);
        if (shouldShowBatchUploadCard()) {
            // In the root folder there can only be exactly zero or two SECTION HEADERs. The
            // account header is the first of them, and the second header is the local one.
            if (currentBatchUploadCardIndex < 0) {
                ListItem batchUploadCardListItem = buildBatchUploadCardListItem();
                int localSectionHeaderIndex =
                        searchForLastIndexOfType(
                                0, viewType -> viewType == ViewType.SECTION_HEADER);
                if (localSectionHeaderIndex >= 0) {
                    // Inserting the batch upload card immediately after the local header.
                    mModelList.add(localSectionHeaderIndex + 1, batchUploadCardListItem);
                }
            }
        } else {
            if (currentBatchUploadCardIndex >= 0) {
                mModelList.removeAt(currentBatchUploadCardIndex);
            }
        }
    }

    private int getCurrentPromoHeaderIndex() {
        return searchForFirstIndexOfType(
                /* endIndex= */ PROMO_MAX_INDEX, (type) -> type == ViewType.SIGNIN_PROMO);
    }

    private int getCurrentSearchBoxIndex() {
        return searchForFirstIndexOfType(
                /* endIndex= */ SEARCH_BOX_MAX_INDEX, (type) -> type == ViewType.SEARCH_BOX);
    }

    /** Returns the first index that matches up until endIndex, or -1 if no match is found. */
    private int searchForFirstIndexOfType(int endIndex, Predicate<Integer> typePredicate) {
        endIndex = Math.min(endIndex, mModelList.size() - 1);
        for (int i = 0; i <= endIndex; ++i) {
            if (typePredicate.test(mModelList.get(i).type)) {
                return i;
            }
        }
        return -1;
    }

    /**
     * Returns the last index that matches up starting from startIndex, or -1 if no match is found.
     */
    private int searchForLastIndexOfType(int startIndex, Predicate<Integer> typePredicate) {
        startIndex = Math.min(startIndex, 0);
        for (int i = mModelList.size() - 1; i >= startIndex; --i) {
            if (typePredicate.test(mModelList.get(i).type)) {
                return i;
            }
        }
        return -1;
    }

    /** Removes all promo and section headers from the current list. */
    private void removePromoAndSectionHeaders() {
        for (int i = mModelList.size() - 1; i >= 0; i--) {
            final @ViewType int viewType = mModelList.get(i).type;
            if (viewType == ViewType.SECTION_HEADER || viewType == ViewType.SIGNIN_PROMO) {
                mModelList.removeAt(i);
            }
        }
    }

    int getBookmarkItemStartIndex() {
        return firstIndexWithPredicate(
                0,
                mModelList.size(),
                1,
                (listItem) -> {
                    return isBookmarkRowType(listItem.type);
                });
    }

    int getBookmarkItemEndIndex() {
        return firstIndexWithPredicate(
                mModelList.size() - 1,
                -1,
                -1,
                (listItem) -> {
                    return isBookmarkRowType(listItem.type);
                });
    }

    /**
     * Return true iff the currently-open folder is the root folder (which is true iff the top-level
     * folders are showing)
     */
    private boolean topLevelFoldersShowing() {
        return Objects.equals(getCurrentFolderId(), mBookmarkModel.getRootFolderId());
    }

    /** Clears the highlighted bookmark, if there is one. */
    private void clearHighlight() {
        if (mHighlightedBookmark == null) return;
        int index = getPositionForBookmark(mHighlightedBookmark);
        mModelList.get(index).model.set(BookmarkManagerProperties.IS_HIGHLIGHTED, false);
        mHighlightedBookmark = null;
    }

    private BookmarkListEntry getItemByPosition(int position) {
        ListItem listItem = mModelList.get(position);
        PropertyModel propertyModel = listItem.model;
        return propertyModel.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
    }

    private int getItemCount() {
        return mModelList.size();
    }

    private ListItem buildPersonalizedPromoListItem(@ViewType int promoHeaderType) {
        BookmarkListEntry bookmarkListEntry =
                BookmarkListEntry.createSyncPromoHeader(promoHeaderType);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(BookmarkManagerProperties.ALL_KEYS)
                        .with(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, bookmarkListEntry)
                        .with(BookmarkManagerProperties.BOOKMARK_PROMO_HEADER, mPromoHeaderManager);
        return new ListItem(bookmarkListEntry.getViewType(), builder.build());
    }

    private ListItem buildBatchUploadCardListItem() {
        BookmarkListEntry bookmarkListEntry = BookmarkListEntry.createBatchUploadCard();
        PropertyModel.Builder builder =
                new PropertyModel.Builder(BookmarkManagerProperties.ALL_KEYS)
                        .with(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, bookmarkListEntry)
                        .with(
                                BookmarkManagerProperties.BATCH_UPLOAD_CARD_COORDINATOR,
                                mBatchUploadCardCoordinator);
        return new ListItem(bookmarkListEntry.getViewType(), builder.build());
    }

    private ListItem buildSearchBoxRow() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                        .with(
                                BookmarkSearchBoxRowProperties.SEARCH_TEXT_CHANGE_CALLBACK,
                                this::onSearchTextChangeCallback)
                        .with(
                                BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_RUNNABLE,
                                this::onClearSearchTextRunnable)
                        .with(
                                BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK,
                                this::onSearchBoxFocusChange)
                        .with(
                                BookmarkSearchBoxRowProperties.SHOPPING_CHIP_START_ICON_RES,
                                R.drawable.notifications_active)
                        .with(
                                BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TEXT_RES,
                                R.string.price_tracking_bookmarks_filter_title)
                        .with(
                                BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK,
                                this::onShoppingFilterToggle)
                        .build();
        updateSearchBoxShoppingFilterVisibility(propertyModel);
        return new ListItem(ViewType.SEARCH_BOX, propertyModel);
    }

    private ListItem buildEmptyStateListItem() {
        BookmarkId currentParent = getCurrentFolderId();

        @StringRes int titleRes = R.string.bookmark_manager_empty_state;
        @StringRes int subtitleRes = R.string.bookmark_manager_back_to_page_by_adding_bookmark;
        @DrawableRes int imageRes = R.drawable.bookmark_empty_state_illustration;
        // The currentParent will be null when searching. In this case, fallback to the regular
        // bookmarks empty state.
        if (currentParent != null && currentParent.getType() == BookmarkType.READING_LIST) {
            titleRes = R.string.reading_list_manager_empty_state;
            subtitleRes = R.string.reading_list_manager_save_page_to_read_later;
            imageRes = R.drawable.reading_list_empty_state_illustration;
        }

        PropertyModel model =
                new PropertyModel.Builder(BookmarkManagerEmptyStateProperties.ALL_KEYS)
                        .with(BookmarkManagerEmptyStateProperties.EMPTY_STATE_TITLE_RES, titleRes)
                        .with(
                                BookmarkManagerEmptyStateProperties.EMPTY_STATE_DESCRIPTION_RES,
                                subtitleRes)
                        .with(BookmarkManagerEmptyStateProperties.EMPTY_STATE_IMAGE_RES, imageRes)
                        .build();
        return new ListItem(ViewType.EMPTY_STATE, model);
    }

    private ListItem buildBookmarkListItem(BookmarkListEntry bookmarkListEntry) {
        if (bookmarkListEntry.getViewType() == ViewType.IMPROVED_BOOKMARK_COMPACT
                || bookmarkListEntry.getViewType() == ViewType.IMPROVED_BOOKMARK_VISUAL) {
            return buildImprovedBookmarkRow(bookmarkListEntry);
        }

        BookmarkItem bookmarkItem = bookmarkListEntry.getBookmarkItem();
        BookmarkId bookmarkId = bookmarkItem == null ? null : bookmarkItem.getId();
        PropertyModel propertyModel = new PropertyModel(BookmarkManagerProperties.ALL_KEYS);
        propertyModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, bookmarkListEntry);
        propertyModel.set(BookmarkManagerProperties.BOOKMARK_ID, bookmarkId);

        boolean isHighlighted = Objects.equals(bookmarkId, mHighlightedBookmark);
        propertyModel.set(BookmarkManagerProperties.IS_HIGHLIGHTED, isHighlighted);

        return new ListItem(bookmarkListEntry.getViewType(), propertyModel);
    }

    private void finishLoadingBookmarkModel() {
        mBookmarkModel.finishLoadingBookmarkModel(this::onBookmarkModelLoaded);
    }

    /* package */ @Nullable BatchUploadCardCoordinator getBatchUploadCardCoordinatorForTesting() {
        return mBatchUploadCardCoordinator;
    }

    @VisibleForTesting
    /* package */ ListItem buildImprovedBookmarkRow(BookmarkListEntry bookmarkListEntry) {
        BookmarkItem bookmarkItem = assumeNonNull(bookmarkListEntry.getBookmarkItem());
        BookmarkId bookmarkId = bookmarkItem.getId();

        PropertyModel propertyModel =
                mImprovedBookmarkRowCoordinator.createBasePropertyModel(bookmarkId);
        propertyModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, bookmarkListEntry);

        // Menu
        propertyModel.set(
                ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY,
                bookmarkItem.isEditable() ? ImageVisibility.MENU : ImageVisibility.NONE);
        propertyModel.set(
                ImprovedBookmarkRowProperties.POPUP_LISTENER, this::onBookmarkItemMenuOpened);
        // TODO(crbug.com/40266762): Investigate caching ModelList for the menu.
        propertyModel.set(
                ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE,
                () -> createListMenuForBookmark(propertyModel));
        propertyModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, mIsSelectionEnabled);
        propertyModel.set(
                ImprovedBookmarkRowProperties.SELECTED,
                mSelectionDelegate.isItemSelected(bookmarkId));

        propertyModel.set(
                ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER,
                () -> bookmarkRowClicked(bookmarkId));
        propertyModel.set(
                ImprovedBookmarkRowProperties.ROW_LONG_CLICK_LISTENER,
                () -> bookmarkRowLongClicked(bookmarkId));

        return new ListItem(bookmarkListEntry.getViewType(), propertyModel);
    }

    // ImprovedBookmarkRow methods.

    @VisibleForTesting
    ModelList createListMenuModelList(BookmarkListEntry entry, @Location int location) {
        ModelList listItems = new ModelList();

        BookmarkItem bookmarkItem = entry.getBookmarkItem();
        if (bookmarkItem == null) return listItems;
        BookmarkId bookmarkId = bookmarkItem.getId();

        // Reading list items can sometimes be movable (for type swapping purposes), but for
        // UI purposes they shouldn't be movable.
        boolean canMove = BookmarkUtils.isMovable(mBookmarkModel, bookmarkItem);

        if (bookmarkId.getType() == BookmarkType.READING_LIST) {
            if (bookmarkItem != null) {
                listItems.add(
                        buildSimpleMenuItem(
                                bookmarkItem.isRead()
                                        ? R.string.reading_list_mark_as_unread
                                        : R.string.reading_list_mark_as_read));
            }
        }

        listItems.add(buildSimpleMenuItem(R.string.bookmark_item_select));
        listItems.add(buildSimpleMenuItem(R.string.bookmark_item_edit));
        listItems.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.bookmark_item_move)
                        .withEnabled(canMove)
                        .build());
        listItems.add(buildSimpleMenuItem(R.string.bookmark_item_delete));

        boolean canReorder = isReorderable(entry);
        if (getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
            listItems.add(buildSimpleMenuItem(R.string.bookmark_show_in_folder));
        } else if (getCurrentUiMode() == BookmarkUiMode.FOLDER
                && location != Location.SOLO
                && canReorder) {
            boolean manualSortActive =
                    mBookmarkUiPrefs.getBookmarkRowSortOrder() == BookmarkRowSortOrder.MANUAL;
            // Only add move up / move down buttons if there is more than 1 item.
            if (location != Location.TOP) {
                listItems.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.menu_item_move_up)
                                .withEnabled(manualSortActive)
                                .build());
            }
            if (location != Location.BOTTOM) {
                listItems.add(
                        new ListItemBuilder()
                                .withTitleRes(R.string.menu_item_move_down)
                                .withEnabled(manualSortActive)
                                .build());
            }
        }

        PowerBookmarkMeta meta = entry.getPowerBookmarkMeta();
        if (PowerBookmarkUtils.isShoppingListItem(mShoppingService, meta)) {
            CommerceSubscription sub =
                    PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);
            boolean isSubscribed = mShoppingService.isSubscribedFromCache(sub);
            listItems.add(
                    buildSimpleMenuItem(
                            isSubscribed
                                    ? R.string.disable_price_tracking_menu_item
                                    : R.string.enable_price_tracking_menu_item));
        }

        return listItems;
    }

    @VisibleForTesting
    ListMenu createListMenuForBookmark(PropertyModel model) {
        BookmarkListEntry entry = model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
        BookmarkId bookmarkId = assumeNonNull(entry.getBookmarkItem()).getId();
        ModelList listItems =
                createListMenuModelList(entry, model.get(BookmarkManagerProperties.LOCATION));
        ListMenu.Delegate delegate =
                (item, view) -> {
                    int textId = item.get(ListMenuItemProperties.TITLE_ID);
                    if (textId == R.string.bookmark_item_select) {
                        mSelectionDelegate.toggleSelectionForItem(bookmarkId);
                        RecordUserAction.record("Android.BookmarkPage.SelectFromMenu");
                        if (bookmarkId.getType() == BookmarkType.READING_LIST) {
                            RecordUserAction.record(
                                    "Android.BookmarkPage.ReadingList.SelectFromMenu");
                        }
                    } else if (textId == R.string.bookmark_item_edit) {
                        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
                        assumeNonNull(bookmarkItem);
                        mBookmarkManagerOpener.startEditActivity(
                                mContext, mProfile, bookmarkItem.getId());
                    } else if (textId == R.string.reading_list_mark_as_read) {
                        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
                        assumeNonNull(bookmarkItem);
                        mBookmarkModel.setReadStatusForReadingList(
                                bookmarkItem.getId(), /* read= */ true);
                        RecordUserAction.record("Android.BookmarkPage.ReadingList.MarkAsRead");
                    } else if (textId == R.string.reading_list_mark_as_unread) {
                        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
                        assumeNonNull(bookmarkItem);
                        mBookmarkModel.setReadStatusForReadingList(
                                bookmarkItem.getId(), /* read= */ false);
                        RecordUserAction.record("Android.BookmarkPage.ReadingList.MarkAsUnread");
                    } else if (textId == R.string.bookmark_item_move) {
                        mBookmarkManagerOpener.startFolderPickerActivity(
                                mContext, mProfile, bookmarkId);
                        RecordUserAction.record("MobileBookmarkManagerMoveToFolder");
                    } else if (textId == R.string.bookmark_item_delete) {
                        if (mBookmarkModel != null) {
                            mBookmarkModel.deleteBookmarks(bookmarkId);
                            RecordUserAction.record("Android.BookmarkPage.RemoveItem");
                            if (bookmarkId.getType() == BookmarkType.READING_LIST) {
                                RecordUserAction.record(
                                        "Android.BookmarkPage.ReadingList.RemoveItem");
                            }
                        }
                    } else if (textId == R.string.bookmark_show_in_folder) {
                        BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
                        assumeNonNull(bookmarkItem);
                        openFolder(bookmarkItem.getParentId());
                        highlightBookmark(bookmarkId);
                        RecordUserAction.record("MobileBookmarkManagerShowInFolder");
                    } else if (textId == R.string.menu_item_move_up) {
                        moveUpOne(bookmarkId);
                        RecordUserAction.record("MobileBookmarkManagerMoveUp");
                    } else if (textId == R.string.menu_item_move_down) {
                        moveDownOne(bookmarkId);
                        RecordUserAction.record("MobileBookmarkManagerMoveDown");
                    } else if (textId == R.string.disable_price_tracking_menu_item) {
                        setPriceTrackingEnabled(model, false);
                    } else if (textId == R.string.enable_price_tracking_menu_item) {
                        setPriceTrackingEnabled(model, true);
                    }
                };
        return BrowserUiListMenuUtils.getBasicListMenu(mContext, listItems, delegate);
    }

    void setPriceTrackingEnabled(PropertyModel model, boolean enabled) {
        BookmarkListEntry entry = model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);

        Callback<Boolean> callback =
                success -> {
                    if (!success) return;
                    ShoppingAccessoryCoordinator shoppingAccessoryCoordinator =
                            model.get(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR);
                    shoppingAccessoryCoordinator.setPriceTrackingEnabled(enabled);
                    updateShoppingFilterVisible();
                };

        PowerBookmarkUtils.setPriceTrackingEnabledWithSnackbars(
                mBookmarkModel,
                assumeNonNull(entry.getBookmarkItem()).getId(),
                enabled,
                mSnackbarManager,
                mContext.getResources(),
                mProfile,
                callback,
                mPriceDropNotificationManager);
    }

    void toggleSelectionForRow(BookmarkId id) {
        mSelectionDelegate.toggleSelectionForItem(id);
        int index = getPositionForBookmark(id);
        if (index < 0) {
            return;
        }
        PropertyModel model = mModelList.get(index).model;
        model.set(ImprovedBookmarkRowProperties.SELECTED, mSelectionDelegate.isItemSelected(id));
    }

    void bookmarkRowClicked(BookmarkId id) {
        if (mSelectionDelegate.isSelectionEnabled()) {
            toggleSelectionForRow(id);
        } else {
            openBookmarkId(id);
        }
    }

    void openBookmarkId(BookmarkId id) {
        @Nullable BookmarkItem item = mBookmarkModel.getBookmarkById(id);
        if (item == null) {
            return;
        }

        if (item.isFolder()) {
            openFolder(id);
        } else {
            openBookmark(id);
        }
    }

    boolean bookmarkRowLongClicked(BookmarkId id) {
        if (!mSelectionDelegate.isSelectionEnabled()) {
            toggleSelectionForRow(id);
        }

        // Always consume the event, so it doesn't go to bookmarkRowClicked.
        return true;
    }

    private void onSearchTextChangeCallback(@Nullable String searchText) {
        searchText = searchText == null ? "" : searchText;
        setSearchTextAndUpdateButtonVisibility(searchText);
        onSearchChange(searchText);
    }

    private void onClearSearchTextRunnable() {
        onSearchTextChangeCallback("");
    }

    private void setSearchTextAndUpdateButtonVisibility(String searchText) {
        PropertyModel searchModel = assumeNonNull(getSearchBoxPropertyModel());
        searchModel.set(BookmarkSearchBoxRowProperties.SEARCH_TEXT, searchText);
        boolean isVisible = !TextUtils.isEmpty(searchText);
        searchModel.set(
                BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY, isVisible);
    }

    private void onSearchBoxFocusChange(Boolean hasFocus) {
        assert hasFocus != null;
        setSearchBoxFocusAndHideKeyboardIfNeeded(hasFocus);
    }

    private void setSearchBoxFocusAndHideKeyboardIfNeeded(boolean hasFocus) {
        PropertyModel searchModel = assumeNonNull(getSearchBoxPropertyModel());
        searchModel.set(BookmarkSearchBoxRowProperties.HAS_FOCUS, hasFocus);
        if (hasFocus) {
            // On phones, tapping the search box switches to a dedicated search UI. On tablets, the
            // search box is part of the folder view and doesn't switch modes.
            if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                    && getCurrentUiMode() == BookmarkUiMode.FOLDER) {
                setState(BookmarkUiState.createSearchState(""));
            }
        } else {
            mHideKeyboardRunnable.run();
        }
    }

    private void onShoppingFilterToggle(boolean isFiltering) {
        if (isFiltering) {
            mCurrentPowerFilter.add(PowerBookmarkType.SHOPPING);
        } else {
            mCurrentPowerFilter.remove(PowerBookmarkType.SHOPPING);
        }

        BookmarkMetrics.reportBookmarkManagerFilterUsed(BookmarkManagerFilter.SHOPPING);
        PropertyModel searchModel = assumeNonNull(getSearchBoxPropertyModel());
        searchModel.set(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_SELECTED, isFiltering);
        refresh();
    }

    private void onSearchChange(@Nullable String searchText) {
        searchText = searchText == null ? "" : searchText;
        // On tablets, the search box is an in-place filter. When the search text is cleared,
        // the list should revert to the current folder's contents. On phones, search is a
        // distinct UI mode.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            if (TextUtils.isEmpty(searchText)) {
                mSelectionDelegate.clearSelection();
                refresh();
            } else {
                setBookmarks(
                        mBookmarkQueryHandler.buildBookmarkListForSearch(
                                searchText, mCurrentPowerFilter));
            }
            // After any search text change on a tablet, the back press state may have changed.
            // (e.g., from not-searching to searching, or vice-versa). We must explicitly
            // re-evaluate and notify the supplier.
            onBackPressStateChanged();
        } else {
            setState(BookmarkUiState.createSearchState(searchText));
        }
    }

    /** The search box only focused on LFF device with a hardware keyboard attached. */
    private void maybeAutoFocusSearchBox() {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)
                && DeviceInput.supportsKeyboard()) {
            mRecyclerView.post(
                    () -> {
                        // The search box might not be in the model list yet, so guard this call.
                        if (getCurrentSearchBoxIndex() < 0) return;
                        setSearchBoxFocusAndHideKeyboardIfNeeded(true);
                    });
        }
    }

    private @Nullable String getCurrentSearchText() {
        // On tablets, the search box is an in-place filter and the search text is stored in the
        // property model. On phones, search is a distinct UI mode and the search text is stored in
        // the state stack.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
            PropertyModel searchModel = getSearchBoxPropertyModel();
            return searchModel == null
                    ? ""
                    : searchModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT);
        }
        BookmarkUiState state = mStateStack.peekLast();
        return state == null ? "" : state.mSearchText;
    }

    private @Nullable BookmarkUiState getCurrentUiState() {
        return mStateStack.peekLast();
    }

    private @Nullable BookmarkId getCurrentFolderId() {
        BookmarkUiState state = mStateStack.peekLast();
        return state == null ? null : state.mFolder;
    }

    @VisibleForTesting
    void changeSelectionMode(boolean selectionEnabled) {
        mIsSelectionEnabled = selectionEnabled;

        int startIndex = getBookmarkItemStartIndex();
        int endIndex = getBookmarkItemEndIndex();
        if (startIndex < 0 || endIndex < 0) return;

        for (int i = startIndex; i <= endIndex; i++) {
            // Section headers may be embedded in the list for reading list.
            // TODO(crbug.com/40278854): Consider using RecyclerView decorations for section
            // headers.
            if (mModelList.get(i).type == ViewType.SECTION_HEADER) continue;
            PropertyModel model = mModelList.get(i).model;

            BookmarkId id = model.get(BookmarkManagerProperties.BOOKMARK_ID);
            model.set(
                    ImprovedBookmarkRowProperties.SELECTED, mSelectionDelegate.isItemSelected(id));
            model.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, mIsSelectionEnabled);
        }
    }

    // The shopping filter should only be visible if the shopping feature is enabled and
    // there's at least one price-tracked bookmark available.
    @VisibleForTesting
    void updateShoppingFilterVisible() {
        if (!CommerceFeatureUtils.isShoppingListEligible(mShoppingService)) {
            updateFilterAvailability(false);
            return;
        }

        mShoppingService.getAllPriceTrackedBookmarks(
                (bookmarks) -> {
                    updateFilterAvailability(!bookmarks.isEmpty());
                });
    }

    private void updateFilterAvailability(boolean shoppingFilterAvailable) {
        mShoppingFilterAvailable = shoppingFilterAvailable;
        PropertyModel searchBoxPropertyModel = getSearchBoxPropertyModel();
        // If the search box has already been built the it needs updating.
        if (searchBoxPropertyModel != null) {
            updateSearchBoxShoppingFilterVisibility(searchBoxPropertyModel);
        }
    }

    private void updateSearchBoxShoppingFilterVisibility(PropertyModel searchBoxPropertyModel) {
        // We purposefully hide the shopping filter in reading list even though search is
        // global to avoid confusing users.
        boolean filterVisible =
                mShoppingFilterAvailable
                        && !mBookmarkModel.isReadingListFolder(getCurrentFolderId());
        searchBoxPropertyModel.set(
                BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY, filterVisible);
        Set<PowerBookmarkType> powerFilter = mCurrentPowerFilter;
        if (!filterVisible && powerFilter.contains(PowerBookmarkType.SHOPPING)) {
            onShoppingFilterToggle(false);
        }

        if (filterVisible) {
            BookmarkMetrics.reportBookmarkManagerFilterShown(BookmarkManagerFilter.SHOPPING);
        }
    }

    // Testing methods.

    /** Whether to prevent the bookmark model from fully loading for testing. */
    /* package */ static void preventLoadingForTesting(boolean preventLoading) {
        sPreventLoadingForTesting = preventLoading;
    }

    /* package */ void finishLoadingForTesting() {
        finishLoadingBookmarkModel();
    }

    /* package */ void clearStateStackForTesting() {
        mStateStack.clear();
    }

    /* package */ BookmarkUndoController getUndoControllerForTesting() {
        return mBookmarkUndoController;
    }

    /* package */ DragStateDelegate getDragStateDelegateForTesting() {
        return mDragStateDelegate;
    }

    /* package */ @Nullable BookmarkId getIdByPositionForTesting(int position) {
        return getIdByPosition(getBookmarkItemStartIndex() + position);
    }

    /* package */ void simulateSignInForTesting() {
        mBookmarkUiObserver.onFolderStateSet(getCurrentFolderId());
    }

    /* package */ Deque<BookmarkUiState> getStateStackForTesting() {
        return mStateStack;
    }
}
