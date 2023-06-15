// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.chromium.components.browser_ui.widget.listmenu.BasicListMenu.buildMenuListItem;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkRow.Location;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.Observer;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.StartImageVisibility;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksReader;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DragListener;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DraggabilityProvider;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Objects;
import java.util.Stack;
import java.util.function.Predicate;

/** Responsible for BookmarkManager business logic. */
// TODO(crbug.com/1416611): Remove BookmarkDelegate if possible.
class BookmarkManagerMediator
        implements BookmarkDelegate, TestingDelegate, PartnerBookmarksReader.FaviconUpdateObserver {
    private static final String EMPTY_QUERY = null;
    private static final int PROMO_MAX_INDEX = 1;
    private static final int SEARCH_BOX_MAX_INDEX = 0;

    private static boolean sPreventLoadingForTesting;

    /**
     * Keeps track of whether drag is enabled / active for bookmark lists.
     */
    private class BookmarkDragStateDelegate implements DragStateDelegate {
        private BookmarkDelegate mBookmarkDelegate;
        private SelectionDelegate<BookmarkId> mSelectionDelegate;

        void onBookmarkDelegateInitialized(BookmarkDelegate delegate) {
            mBookmarkDelegate = delegate;
            mSelectionDelegate = delegate.getSelectionDelegate();
        }

        // DragStateDelegate implementation
        @Override
        public boolean getDragEnabled() {
            return !AccessibilityState.isPerformGesturesEnabled()
                    && mBookmarkDelegate.getCurrentUiMode() == BookmarkUiMode.FOLDER;
        }

        @Override
        public boolean getDragActive() {
            return getDragEnabled() && mSelectionDelegate.isSelectionEnabled();
        }
    }

    private final BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeChildrenReordered(BookmarkItem node) {
            refresh();
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            clearHighlight();

            if (getCurrentUiMode() == BookmarkUiMode.FOLDER) {
                // If the folder is removed in folder mode, show the parent folder or falls back to
                // all bookmarks mode.
                if (Objects.equals(node.getId(), getCurrentFolderId())) {
                    if (mBookmarkModel.getTopLevelFolderIds(true, true).contains(node.getId())) {
                        openFolder(mBookmarkModel.getDefaultFolderViewLocation());
                    } else {
                        openFolder(parent.getId());
                    }
                } else {
                    // Needs to remove the current node, and update any transitive parents that may
                    // be showing child counts. Just refresh() for now.
                    refresh();
                }
            } else if (getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
                // We cannot rely on removing the specific list item that corresponds to the
                // removed node because the node might be a parent with children also shown
                // in the list.
                search(mSearchText);
            }
        }

        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            clearHighlight();

            if (getCurrentUiMode() == BookmarkUiMode.FOLDER
                    && Objects.equals(node.getId(), getCurrentFolderId())) {
                refresh();
            } else {
                super.bookmarkNodeChanged(node);
            }
        }

        @Override
        public void bookmarkModelChanged() {
            clearHighlight();

            if (getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
                if (!TextUtils.equals(mSearchText, EMPTY_QUERY)) {
                    search(mSearchText);
                } else {
                    onEndSearch();
                }
            } else {
                refresh();
            }
        }
    };

    private final Stack<BookmarkUiState> mStateStack = new Stack<>() {
        @Override
        public BookmarkUiState push(BookmarkUiState item) {
            // The back press state depends on the size of stack. So push/pop item first in order
            // to keep the size update-to-date.
            var state = super.push(item);
            onBackPressStateChanged();
            return state;
        }

        @Override
        public synchronized BookmarkUiState pop() {
            var state = super.pop();
            onBackPressStateChanged();
            return state;
        }
    };

    private final BookmarkUiObserver mBookmarkUiObserver = new BookmarkUiObserver() {
        @Override
        public void onDestroy() {
            removeUiObserver(mBookmarkUiObserver);
            getSelectionDelegate().removeObserver(mSelectionObserver);
            mPromoHeaderManager.destroy();
        }

        @Override
        public void onFolderStateSet(BookmarkId folder) {
            clearHighlight();

            mSearchText = EMPTY_QUERY;
            mDragReorderableRecyclerViewAdapter.enableDrag();

            setBookmarks(mBookmarkQueryHandler.buildBookmarkListForParent(getCurrentFolderId()));
            updateEmptyViewText();
        }

        private void updateEmptyViewText() {
            assert getCurrentFolderId() != null;
            if (BookmarkId.SHOPPING_FOLDER.equals(getCurrentFolderId())) {
                getSelectableListLayout().setEmptyViewText(
                        R.string.tracked_products_empty_list_title);
            } else if (getCurrentFolderId().getType() == BookmarkType.READING_LIST) {
                TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                        EventConstants.READ_LATER_BOOKMARK_FOLDER_OPENED);
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.EMPTY_STATES)) {
                    getSelectableListLayout().setEmptyStateImageRes(
                            R.drawable.reading_list_empty_state_illustration);
                    getSelectableListLayout().setEmptyStateViewText(
                            R.string.reading_list_manager_empty_state,
                            R.string.reading_list_manager_save_page_to_read_later);
                } else {
                    getSelectableListLayout().setEmptyViewText(
                            R.string.reading_list_empty_list_title);
                }
            } else {
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.EMPTY_STATES)) {
                    getSelectableListLayout().setEmptyStateImageRes(
                            R.drawable.bookmark_empty_state_illustration);
                    getSelectableListLayout().setEmptyStateViewText(
                            R.string.bookmark_manager_empty_state,
                            R.string.bookmark_manager_back_to_page_by_adding_bookmark);
                } else {
                    getSelectableListLayout().setEmptyViewText(R.string.bookmarks_folder_empty);
                }
            }
        }
    };

    private final SelectionObserver<BookmarkId> mSelectionObserver = new SelectionObserver<>() {
        @Override
        public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
            clearHighlight();
        }
    };

    private final DragListener mDragListener = new DragListener() {
        @Override
        public void onSwap() {
            setOrder();
        }
    };

    private final DraggabilityProvider mDraggabilityProvider = new DraggabilityProvider() {
        @Override
        public boolean isActivelyDraggable(PropertyModel propertyModel) {
            BookmarkId bookmarkId = propertyModel.get(BookmarkManagerProperties.BOOKMARK_ID);
            return mSelectionDelegate.isItemSelected(bookmarkId)
                    && isPassivelyDraggable(propertyModel);
        }

        @Override
        public boolean isPassivelyDraggable(PropertyModel propertyModel) {
            BookmarkListEntry bookmarkListEntry =
                    propertyModel.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
            BookmarkItem bookmarkItem = bookmarkListEntry.getBookmarkItem();
            return bookmarkItem.isReorderable();
        }
    };

    private final BookmarkUiPrefs.Observer mBookmarkUiPrefsObserver = new Observer() {
        @Override
        public void onBookmarkRowDisplayPrefChanged(@BookmarkRowDisplayPref int displayPref) {
            Resources res = mContext.getResources();
            mBookmarkImageFetcher.setupFetchProperties(
                    BookmarkUtils.getRoundedIconGenerator(mContext, displayPref),
                    BookmarkUtils.getImageIconSize(res, displayPref),
                    BookmarkUtils.getFaviconDisplaySize(res, displayPref));

            mModelList.clear();
            if (getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
                search(mSearchText);
            } else {
                setBookmarks(
                        mBookmarkQueryHandler.buildBookmarkListForParent(getCurrentFolderId()));
            }
        }

        @Override
        public void onBookmarkRowSortOrderChanged(@BookmarkRowSortOrder int sortOrder) {
            refresh();
        }
    };

    private final ObserverList<BookmarkUiObserver> mUiObservers = new ObserverList<>();
    private final BookmarkDragStateDelegate mDragStateDelegate = new BookmarkDragStateDelegate();
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final BookmarkOpener mBookmarkOpener;
    // TODO(crbug.com/1416611): Remove reference to SelectableListLayout.
    // Owned by BookmarkManager(Coordinator).
    private final SelectableListLayout<BookmarkId> mSelectableListLayout;
    private final SelectionDelegate<BookmarkId> mSelectionDelegate;
    // TODO(crbug.com/1416611): Remove reference to RecyclerView.
    // Owned by BookmarkManager(Coordinator).
    private final RecyclerView mRecyclerView;
    private final DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    private final LargeIconBridge mLargeIconBridge;
    // Whether we're showing in a dialog UI which is only true for phones.
    private final boolean mIsDialogUi;
    private final boolean mIsIncognito;
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier;
    private final Profile mProfile;
    private final BookmarkPromoHeader mPromoHeaderManager;
    private final BookmarkUndoController mBookmarkUndoController;
    private final BookmarkQueryHandler mBookmarkQueryHandler;
    private final ModelList mModelList;
    private final BookmarkUiPrefs mBookmarkUiPrefs;
    private final Runnable mHideKeyboardRunnable;
    private final BookmarkImageFetcher mBookmarkImageFetcher;
    private final ShoppingService mShoppingService;
    private final SnackbarManager mSnackbarManager;

    // Whether this instance has been destroyed.
    private boolean mIsDestroyed;
    private String mInitialUrl;
    private boolean mFaviconsNeedRefresh;
    private BasicNativePage mNativePage;
    private String mSearchText;
    // Keep track of the currently highlighted bookmark - used for "show in folder" action.
    private BookmarkId mHighlightedBookmark;

    BookmarkManagerMediator(Context context, BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener, SelectableListLayout<BookmarkId> selectableListLayout,
            SelectionDelegate<BookmarkId> selectionDelegate, RecyclerView recyclerView,
            DragReorderableRecyclerViewAdapter dragReorderableRecyclerViewAdapter,
            LargeIconBridge largeIconBridge, boolean isDialogUi, boolean isIncognito,
            ObservableSupplierImpl<Boolean> backPressStateSupplier, Profile profile,
            BookmarkUndoController bookmarkUndoController, ModelList modelList,
            BookmarkUiPrefs bookmarkUiPrefs, Runnable hideKeyboardRunnable,
            BookmarkImageFetcher bookmarkImageFetcher, ShoppingService shoppingService,
            SnackbarManager snackbarManager) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        mBookmarkOpener = bookmarkOpener;
        mSelectableListLayout = selectableListLayout;
        mSelectableListLayout.getHandleBackPressChangedSupplier().addObserver(
                (x) -> onBackPressStateChanged());
        mSelectionDelegate = selectionDelegate;
        mRecyclerView = recyclerView;
        mDragReorderableRecyclerViewAdapter = dragReorderableRecyclerViewAdapter;
        mDragReorderableRecyclerViewAdapter.addDragListener(mDragListener);
        mDragReorderableRecyclerViewAdapter.setLongPressDragDelegate(
                () -> mDragStateDelegate.getDragActive());
        mLargeIconBridge = largeIconBridge;
        mIsDialogUi = isDialogUi;
        mIsIncognito = isIncognito;
        mBackPressStateSupplier = backPressStateSupplier;
        mProfile = profile;

        mPromoHeaderManager = new BookmarkPromoHeader(mContext, mProfile, this::updateHeader);
        mBookmarkUndoController = bookmarkUndoController;
        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            mBookmarkQueryHandler =
                    new ImprovedBookmarkQueryHandler(mBookmarkModel, bookmarkUiPrefs);
        } else {
            mBookmarkQueryHandler = new LegacyBookmarkQueryHandler(
                    mBookmarkModel, bookmarkUiPrefs, SyncServiceFactory.getForProfile(mProfile));
        }

        mModelList = modelList;
        mBookmarkUiPrefs = bookmarkUiPrefs;
        mBookmarkUiPrefs.addObserver(mBookmarkUiPrefsObserver);
        mHideKeyboardRunnable = hideKeyboardRunnable;
        mBookmarkImageFetcher = bookmarkImageFetcher;
        mShoppingService = shoppingService;
        mSnackbarManager = snackbarManager;

        // Previously we were waiting for BookmarkModel to be loaded, but it's not necessary.
        PartnerBookmarksReader.addFaviconUpdateObserver(this);

        initializeToLoadingState();
        if (!sPreventLoadingForTesting) {
            finishLoadingBookmarkModel();
        }
    }

    void onBookmarkModelLoaded() {
        mDragStateDelegate.onBookmarkDelegateInitialized(this);

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
        mLargeIconBridge.destroy();
        PartnerBookmarksReader.removeFaviconUpdateObserver(this);

        mBookmarkUndoController.destroy();
        mBookmarkQueryHandler.destroy();

        mBookmarkUiPrefs.removeObserver(mBookmarkUiPrefsObserver);

        for (BookmarkUiObserver observer : mUiObservers) {
            observer.onDestroy();
        }
        assert mUiObservers.size() == 0;
    }

    void onAttachedToWindow() {
        mBookmarkUndoController.setEnabled(true);
    }

    void onDetachedFromWindow() {
        mBookmarkUndoController.setEnabled(false);
    }

    /**
     * See BookmarkManager(Coordinator)#onBackPressed.
     */
    boolean onBackPressed() {
        if (mIsDestroyed) return false;

        // TODO(twellington): Replicate this behavior for other list UIs during unification.
        if (mSelectableListLayout.onBackPressed()) {
            return true;
        }

        if (!mStateStack.empty()) {
            mStateStack.pop();
            if (!mStateStack.empty()) {
                setState(mStateStack.pop());
                return true;
            }
        }
        return false;
    }

    /**
     * See BookmarkManager(Coordinator)#setBasicNativePage.
     */
    void setBasicNativePage(BasicNativePage nativePage) {
        mNativePage = nativePage;
    }

    /**
     * See BookmarkManager(Coordinator)#updateForUrl
     */
    void updateForUrl(String url) {
        // Bookmark model is null if the manager has been destroyed.
        if (mBookmarkModel == null) return;

        if (mBookmarkModel.isBookmarkModelLoaded()) {
            BookmarkUiState searchState = null;
            if (!mStateStack.isEmpty() && mStateStack.peek().mUiMode == BookmarkUiMode.SEARCHING) {
                searchState = mStateStack.pop();
            }

            setState(BookmarkUiState.createStateFromUrl(url, mBookmarkModel));

            if (searchState != null) setState(searchState);
        } else {
            mInitialUrl = url;
        }
    }

    BookmarkPromoHeader getPromoHeaderManager() {
        return mPromoHeaderManager;
    }

    BookmarkId getIdByPosition(int position) {
        BookmarkListEntry entry = getItemByPosition(position);
        if (entry == null || entry.getBookmarkItem() == null) return null;
        return entry.getBookmarkItem().getId();
    }

    /**
     * Synchronously searches for the given query.
     * @param query The query text to search for.
     */
    void search(@Nullable String query) {
        mSearchText = query == null ? "" : query.trim();
        setBookmarks(mBookmarkQueryHandler.buildBookmarkListForSearch(mSearchText));
    }

    public void setOrder() {
        assert !topLevelFoldersShowing() : "Cannot reorder top-level folders!";
        assert getCurrentFolderId().getType()
                != BookmarkType.PARTNER : "Cannot reorder partner bookmarks!";
        assert getCurrentUiMode()
                == BookmarkUiMode.FOLDER : "Can only reorder items from folder mode!";

        int startIndex = getBookmarkItemStartIndex();
        int endIndex = getBookmarkItemEndIndex();

        // Get the new order for the IDs.
        long[] newOrder = new long[endIndex - startIndex + 1];
        for (int i = startIndex; i <= endIndex; i++) {
            BookmarkItem bookmarkItem = getItemByPosition(i).getBookmarkItem();
            assert bookmarkItem != null;
            newOrder[i - startIndex] = bookmarkItem.getId().getId();
        }
        mBookmarkModel.reorderBookmarks(getCurrentFolderId(), newOrder);
        if (mDragStateDelegate.getDragActive()) {
            RecordUserAction.record("MobileBookmarkManagerDragReorder");
        }

        updateAllLocations();
    }

    public boolean isReorderable(BookmarkListEntry entry) {
        return entry != null && entry.getBookmarkItem() != null
                && entry.getBookmarkItem().isReorderable();
    }

    DraggabilityProvider getDraggabilityProvider() {
        return mDraggabilityProvider;
    }

    // TestingDelegate implementation.

    @Override
    public BookmarkId getIdByPositionForTesting(int position) {
        return getIdByPosition(position);
    }

    @Override
    public void searchForTesting(@Nullable String query) {
        search(query);
    }

    @Override
    public void simulateSignInForTesting() {
        mBookmarkUiObserver.onFolderStateSet(getCurrentFolderId());
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
        int state = getCurrentUiMode();
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
        if (!mBookmarkOpener.openBookmarkInCurrentTab(bookmark, mIsIncognito)) return;

        // Close bookmark UI. Keep the reading list page open.
        if (bookmark != null && bookmark.getType() != BookmarkType.READING_LIST) {
            BookmarkUtils.finishActivityOnPhone(mContext);
        }
    }

    @Override
    public void openBookmarksInNewTabs(List<BookmarkId> bookmarks, boolean incognito) {
        if (mBookmarkOpener.openBookmarksInNewTabs(bookmarks, incognito)) {
            BookmarkUtils.finishActivityOnPhone(mContext);
        }
    }

    @Override
    public void openSearchUi() {
        setState(BookmarkUiState.createSearchState());
        mSelectableListLayout.onStartSearch(R.string.bookmark_no_result);
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
        if (mStateStack.isEmpty()) return BookmarkUiMode.LOADING;
        return mStateStack.peek().mUiMode;
    }

    @Override
    public LargeIconBridge getLargeIconBridge() {
        return mLargeIconBridge;
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
        mSelectableListLayout.onEndSearch();

        // Pop the search state off the stack.
        mStateStack.pop();

        // Set the state back to the folder that was previously being viewed. Listeners will be
        // notified of the change and the list of bookmarks will be updated.
        setState(mStateStack.pop());
    }

    // PartnerBookmarksReader.FaviconUpdateObserver implementation.

    @Override
    public void onUpdateFavicon(String url) {
        assert mBookmarkModel.isBookmarkModelLoaded();
        mLargeIconBridge.clearFavicon(new GURL(url));
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

    private @Nullable BookmarkId getCurrentFolderId() {
        return mStateStack.isEmpty() ? null : mStateStack.peek().mFolder;
    }

    /**
     * Puts all UI elements to loading state. This state might be overridden synchronously by
     * {@link #updateForUrl(String)}, if the bookmark model is already loaded.
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
     * state to the back stack. It also notifies the {@link #mNativePage} (if any) that the
     * url has changed.
     *
     * <p>Also note that even if we store states to {@link #mStateStack}, on tablet the back
     * navigation and back button are not controlled by the manager: the tab handles back key and
     * backstack navigation.
     */
    private void setState(BookmarkUiState state) {
        if (!state.isValid(mBookmarkModel)) {
            state = BookmarkUiState.createFolderState(
                    mBookmarkModel.getDefaultFolderViewLocation(), mBookmarkModel);
        }

        if (!mStateStack.isEmpty() && mStateStack.peek().equals(state)) return;

        // The loading state is not persisted in history stack and once we have a valid state it
        // shall be removed.
        if (!mStateStack.isEmpty() && mStateStack.peek().mUiMode == BookmarkUiMode.LOADING) {
            mStateStack.pop();
        }
        mStateStack.push(state);
        notifyUi(state);
    }

    private void notifyUi(BookmarkUiState state) {
        if (state.mUiMode == BookmarkUiMode.FOLDER) {
            // Loading and searching states may be pushed to the stack but should never be stored in
            // preferences.
            BookmarkUtils.setLastUsedUrl(mContext, state.mUrl);
            // If a loading state is replaced by another loading state, do not notify this change.
            if (mNativePage != null) {
                mNativePage.onStateChange(state.mUrl, false);
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
        for (BookmarkId node : mSelectionDelegate.getSelectedItemsAsList()) {
            if (mSelectionDelegate.isItemSelected(node) && getPositionForBookmark(node) == -1) {
                mSelectionDelegate.toggleSelectionForItem(node);
            }
        }
    }

    private void onBackPressStateChanged() {
        if (mIsDestroyed) {
            mBackPressStateSupplier.set(false);
            return;
        }
        mBackPressStateSupplier.set(
                Boolean.TRUE.equals(mSelectableListLayout.getHandleBackPressChangedSupplier().get())
                || mStateStack.size() > 1);
    }

    /** @return The position of the given bookmark in adapter. Will return -1 if not found. */
    private int getPositionForBookmark(BookmarkId bookmark) {
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

    @SuppressWarnings("NotifyDataSetChanged")
    private void setBookmarks(List<BookmarkListEntry> bookmarkListEntryList) {
        clearHighlight();

        // This method is called due to unknown model changes, and we're basically rebuilding every
        // row. However we need to avoid doing this in a way that'll cause flicker. So we replace
        // items in place so that the recycler view doesn't see everything being removed and added
        // back, but instead it sees items being changed.
        int index = 0;

        if (BookmarkFeatures.isAndroidImprovedBookmarksEnabled()) {
            // Don't replace if it already exists. The text box is stateful.
            if (getCurrentSearchBoxIndex() < 0) {
                updateOrAdd(index, buildSearchBoxRow());
            }
            index++;
        }

        // Restore the header, if it exists, then update it.
        final @ViewType int targetPromoHeaderType = calculatePromoHeaderType();
        if (targetPromoHeaderType != ViewType.INVALID) {
            updateOrAdd(index++, buildPersonalizedPromoListItem(targetPromoHeaderType));
        }

        for (BookmarkListEntry bookmarkListEntry : bookmarkListEntryList) {
            updateOrAdd(index++, buildBookmarkListItem(bookmarkListEntry));
        }

        if (mModelList.size() == 0 && index == 0) {
            // Bookmarks are loaded but we have no items. The SelectableListLayout should
            // hide the spinner, so force a notification.
            mDragReorderableRecyclerViewAdapter.notifyDataSetChanged();
        }

        if (mModelList.size() > index) {
            mModelList.removeRange(index, mModelList.size() - index);
        }

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

    private static boolean isMovable(PropertyModel propertyModel) {
        BookmarkListEntry bookmarkListEntry =
                propertyModel.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
        if (bookmarkListEntry == null) return false;
        BookmarkItem bookmarkItem = bookmarkListEntry.getBookmarkItem();
        if (bookmarkItem == null) return false;
        return BookmarkUtils.isMovable(bookmarkItem);
    }

    private int firstIndexWithLocation(int start, int stop, int delta) {
        for (int i = start; i != stop; i += delta) {
            ListItem listItem = mModelList.get(i);
            final @ViewType int viewType = listItem.type;
            if ((viewType == ViewType.BOOKMARK || viewType == ViewType.FOLDER
                        || viewType == ViewType.SHOPPING_POWER_BOOKMARK
                        || viewType == ViewType.IMPROVED_BOOKMARK_COMPACT
                        || viewType == ViewType.IMPROVED_BOOKMARK_VISUAL)
                    && isMovable(listItem.model)) {
                return i;
            }
        }
        return -1;
    }

    private void updateAllLocations() {
        int startIndex = firstIndexWithLocation(0, mModelList.size(), 1);
        int lastIndex = firstIndexWithLocation(mModelList.size() - 1, -1, -1);
        if (startIndex < 0 || lastIndex < 0) {
            return;
        }

        if (startIndex == lastIndex) {
            mModelList.get(startIndex).model.set(BookmarkManagerProperties.LOCATION, Location.SOLO);
        } else {
            mModelList.get(startIndex).model.set(BookmarkManagerProperties.LOCATION, Location.TOP);
            mModelList.get(lastIndex).model.set(
                    BookmarkManagerProperties.LOCATION, Location.BOTTOM);
        }

        for (int i = startIndex + 1; i < lastIndex; i++) {
            mModelList.get(i).model.set(BookmarkManagerProperties.LOCATION, Location.MIDDLE);
        }
    }

    /** Refresh the list of bookmarks within the currently visible folder. */
    private void refresh() {
        if (!mStateStack.isEmpty()) {
            notifyUi(mStateStack.peek());
        }
    }

    private @ViewType int calculatePromoHeaderType() {
        final @BookmarkUiMode int currentUiState = getCurrentUiMode();
        if (currentUiState != BookmarkUiMode.FOLDER) {
            return ViewType.INVALID;
        }

        final @SyncPromoState int promoState = mPromoHeaderManager.getPromoState();
        switch (promoState) {
            case SyncPromoState.NO_PROMO:
                return ViewType.INVALID;
            case SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE:
                return ViewType.PERSONALIZED_SIGNIN_PROMO;
            case SyncPromoState.PROMO_FOR_SIGNED_IN_STATE:
                return ViewType.PERSONALIZED_SYNC_PROMO;
            case SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE:
                return ViewType.SYNC_PROMO;
            default:
                assert false : "Unexpected value for promo state!";
                return ViewType.INVALID;
        }
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

    private int getCurrentPromoHeaderIndex() {
        return searchForFirstIndexOfType(/*endIndex=*/PROMO_MAX_INDEX, this::isPromoType);
    }

    private int getCurrentSearchBoxIndex() {
        return searchForFirstIndexOfType(
                /*endIndex=*/SEARCH_BOX_MAX_INDEX, (type) -> type == ViewType.SEARCH_BOX);
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

    private boolean isPromoType(@ViewType int viewType) {
        return viewType == ViewType.PERSONALIZED_SIGNIN_PROMO
                || viewType == ViewType.PERSONALIZED_SYNC_PROMO || viewType == ViewType.SYNC_PROMO;
    }

    /** Removes all promo and section headers from the current list. */
    private void removePromoAndSectionHeaders() {
        for (int i = mModelList.size() - 1; i >= 0; i--) {
            final @ViewType int viewType = mModelList.get(i).type;
            if (viewType == ViewType.SECTION_HEADER || isPromoType(viewType)) {
                mModelList.removeAt(i);
            }
        }
    }

    private int getBookmarkItemStartIndex() {
        return Math.max(0, firstIndexWithLocation(0, mModelList.size(), 1));
    }

    private int getBookmarkItemEndIndex() {
        int endIndex = mModelList.size() - 1;
        BookmarkItem bookmarkItem = getItemByPosition(endIndex).getBookmarkItem();
        if (bookmarkItem == null || !BookmarkUtils.isMovable(bookmarkItem)) {
            endIndex--;
        }
        return endIndex;
    }

    /**
     * Return true iff the currently-open folder is the root folder
     *         (which is true iff the top-level folders are showing)
     */
    private boolean topLevelFoldersShowing() {
        return Objects.equals(getCurrentFolderId(), mBookmarkModel.getRootFolderId());
    }

    /** Clears the highlighted bookmark, if there is one. */
    private void clearHighlight() {
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
        PropertyModel propertyModel = new PropertyModel(BookmarkManagerProperties.ALL_KEYS);
        propertyModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, bookmarkListEntry);
        propertyModel.set(BookmarkManagerProperties.BOOKMARK_PROMO_HEADER, mPromoHeaderManager);
        return new ListItem(bookmarkListEntry.getViewType(), propertyModel);
    }

    private ListItem buildSearchBoxRow() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(BookmarkSearchBoxRowProperties.ALL_KEYS)
                        .with(BookmarkSearchBoxRowProperties.QUERY_CALLBACK, this::onQueryCallback)
                        .build();
        return new ListItem(ViewType.SEARCH_BOX, propertyModel);
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
        propertyModel.set(BookmarkManagerProperties.IS_FROM_FILTER_VIEW,
                BookmarkId.SHOPPING_FOLDER.equals(getCurrentFolderId()));

        boolean isHighlighted = Objects.equals(bookmarkId, mHighlightedBookmark);
        propertyModel.set(BookmarkManagerProperties.IS_HIGHLIGHTED, isHighlighted);
        propertyModel.set(BookmarkManagerProperties.CLEAR_HIGHLIGHT, this::clearHighlight);

        if (bookmarkListEntry.getViewType() == ViewType.SHOPPING_FILTER) {
            propertyModel.set(BookmarkManagerProperties.OPEN_FOLDER, this::openFolder);
        }

        return new ListItem(bookmarkListEntry.getViewType(), propertyModel);
    }

    private void finishLoadingBookmarkModel() {
        mBookmarkModel.finishLoadingBookmarkModel(this::onBookmarkModelLoaded);
    }

    @VisibleForTesting
    ListItem buildImprovedBookmarkRow(BookmarkListEntry bookmarkListEntry) {
        PropertyModel propertyModel = new PropertyModel(ImprovedBookmarkRowProperties.ALL_KEYS);
        BookmarkItem item = bookmarkListEntry.getBookmarkItem();
        BookmarkId id = item.getId();
        PowerBookmarkMeta meta = bookmarkListEntry.getPowerBookmarkMeta();

        propertyModel.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, bookmarkListEntry);
        propertyModel.set(BookmarkManagerProperties.BOOKMARK_ID, id);
        propertyModel.set(ImprovedBookmarkRowProperties.TITLE, item.getTitle());
        propertyModel.set(ImprovedBookmarkRowProperties.DESCRIPTION,
                item.isFolder() ? BookmarkUtils.getFolderDescriptionText(
                        id, mBookmarkModel, mContext.getResources())
                                : item.getUrlForDisplay());
        resolveIconForBookmark(item, propertyModel);
        propertyModel.set(
                ImprovedBookmarkRowProperties.POPUP_LISTENER, this::onBookmarkItemMenuOpened);
        propertyModel.set(ImprovedBookmarkRowProperties.SELECTED, false);
        propertyModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, false);
        propertyModel.set(ImprovedBookmarkRowProperties.DRAG_ENABLED, false);
        // TODO(crbug.com/1442044): Investigate caching ModelList for the menu.
        propertyModel.set(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE,
                () -> createListMenuForBookmark(propertyModel));
        propertyModel.set(ImprovedBookmarkRowProperties.EDITABLE, item.isEditable());
        propertyModel.set(
                ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK, () -> openBookmarkId(id));

        if (meta != null && meta.hasShoppingSpecifics()) {
            ShoppingAccessoryCoordinator shoppingAccessoryCoordinator =
                    new ShoppingAccessoryCoordinator(
                            mContext, meta.getShoppingSpecifics(), mShoppingService);
            propertyModel.set(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR,
                    shoppingAccessoryCoordinator);
            propertyModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW,
                    shoppingAccessoryCoordinator.getView());
        } else {
            propertyModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, null);
        }

        return new ListItem(bookmarkListEntry.getViewType(), propertyModel);
    }

    // ImprovedBookmarkRow methods.

    private void resolveIconForBookmark(BookmarkItem item, PropertyModel model) {
        final @BookmarkRowDisplayPref int displayPref =
                mBookmarkUiPrefs.getBookmarkRowDisplayPref();
        boolean useImages = displayPref == BookmarkRowDisplayPref.VISUAL;
        model.set(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY,
                item.isFolder() && useImages ? StartImageVisibility.FOLDER_DRAWABLE
                                             : StartImageVisibility.DRAWABLE);

        if (item.isFolder()) {
            final @BookmarkType int type = item.getId().getType();
            // TODO(https://crbug.com/1454593): Rework to not require another model call.
            boolean isSpecialFolder =
                    Objects.equals(item.getParentId(), mBookmarkModel.getRootFolderId());
            final Drawable folderDrawable;
            if (useImages) {
                model.set(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT,
                        BookmarkUtils.getChildCountForDisplay(item.getId(), mBookmarkModel));
                if (isSpecialFolder) {
                    folderDrawable = BookmarkUtils.getFolderIcon(mContext, type, displayPref);
                } else {
                    folderDrawable = ResourcesCompat.getDrawable(mContext.getResources(),
                            R.drawable.ic_folder_outline_24dp, mContext.getTheme());
                    mBookmarkImageFetcher.fetchFirstTwoImagesForFolder(item, imagePair -> {
                        model.set(ImprovedBookmarkRowProperties.START_IMAGE_FOLDER_DRAWABLES,
                                imagePair);
                    });
                }
            } else {
                folderDrawable = BookmarkUtils.getFolderIcon(mContext, type, displayPref);
            }

            if (isSpecialFolder) {
                model.set(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR,
                        SemanticColorUtils.getColorPrimaryContainer(mContext));
                model.set(ImprovedBookmarkRowProperties.START_ICON_TINT,
                        ColorStateList.valueOf(
                                SemanticColorUtils.getDefaultIconColorAccent1(mContext)));
            } else {
                model.set(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR,
                        ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_1));
                model.set(ImprovedBookmarkRowProperties.START_ICON_TINT,
                        AppCompatResources.getColorStateList(
                                mContext, R.color.default_icon_color_secondary_tint_list));
            }

            model.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, folderDrawable);
        } else {
            model.set(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR,
                    ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_1));
            model.set(ImprovedBookmarkRowProperties.START_ICON_TINT, null);
            if (useImages) {
                mBookmarkImageFetcher.fetchImageForBookmarkWithFaviconFallback(item, image -> {
                    model.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, image);
                });
            } else {
                mBookmarkImageFetcher.fetchFaviconForBookmark(item, image -> {
                    model.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, image);
                });
            }
        }
    }

    @VisibleForTesting
    ModelList createListMenuModelList(BookmarkListEntry entry, @Location int location) {
        BookmarkItem bookmarkItem = entry.getBookmarkItem();
        BookmarkId bookmarkId = bookmarkItem.getId();
        // Reading list items can sometimes be movable (for type swapping purposes), but for
        // UI purposes they shouldn't be movable.
        boolean canMove = bookmarkItem != null && BookmarkUtils.isMovable(bookmarkItem);
        ModelList listItems = new ModelList();
        if (bookmarkId.getType() == BookmarkType.READING_LIST) {
            if (bookmarkItem != null) {
                listItems.add(buildMenuListItem(bookmarkItem.isRead()
                                ? R.string.reading_list_mark_as_unread
                                : R.string.reading_list_mark_as_read,
                        0, 0));
            }
        }

        listItems.add(buildMenuListItem(R.string.bookmark_item_select, 0, 0));
        listItems.add(buildMenuListItem(R.string.bookmark_item_edit, 0, 0));
        listItems.add(buildMenuListItem(R.string.bookmark_item_move, 0, 0, canMove));
        listItems.add(buildMenuListItem(R.string.bookmark_item_delete, 0, 0));

        if (getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
            listItems.add(buildMenuListItem(R.string.bookmark_show_in_folder, 0, 0));
        }

        PowerBookmarkMeta meta = entry.getPowerBookmarkMeta();
        if (meta != null && meta.hasShoppingSpecifics()) {
            CommerceSubscription sub =
                    PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);
            boolean isSubscribed = mShoppingService.isSubscribedFromCache(sub);
            listItems.add(buildMenuListItem(isSubscribed ? R.string.disable_price_tracking_menu_item
                                                         : R.string.enable_price_tracking_menu_item,
                    0, 0));
        }

        // TODO(crbug.com/1448691): Add reordering to new bookmarks manager.
        return listItems;
    }

    @VisibleForTesting
    ListMenu createListMenuForBookmark(PropertyModel model) {
        BookmarkListEntry entry = model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
        BookmarkId bookmarkId = entry.getBookmarkItem().getId();
        ModelList listItems =
                createListMenuModelList(entry, model.get(BookmarkManagerProperties.LOCATION));
        ListMenu.Delegate delegate = item -> {
            int textId = item.get(ListMenuItemProperties.TITLE_ID);
            if (textId == R.string.bookmark_item_select) {
                mSelectionDelegate.toggleSelectionForItem(bookmarkId);
                RecordUserAction.record("Android.BookmarkPage.SelectFromMenu");
                if (bookmarkId.getType() == BookmarkType.READING_LIST) {
                    RecordUserAction.record("Android.BookmarkPage.ReadingList.SelectFromMenu");
                }
            } else if (textId == R.string.bookmark_item_edit) {
                BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
                BookmarkUtils.startEditActivity(mContext, bookmarkItem.getId());
            } else if (textId == R.string.reading_list_mark_as_read) {
                BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
                mBookmarkModel.setReadStatusForReadingList(bookmarkItem.getUrl(), /*read=*/true);
                RecordUserAction.record("Android.BookmarkPage.ReadingList.MarkAsRead");
            } else if (textId == R.string.reading_list_mark_as_unread) {
                BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
                mBookmarkModel.setReadStatusForReadingList(bookmarkItem.getUrl(), /*read=*/false);
                RecordUserAction.record("Android.BookmarkPage.ReadingList.MarkAsUnread");
            } else if (textId == R.string.bookmark_item_move) {
                BookmarkFolderSelectActivity.startFolderSelectActivity(mContext, bookmarkId);
                RecordUserAction.record("MobileBookmarkManagerMoveToFolder");
            } else if (textId == R.string.bookmark_item_delete) {
                if (mBookmarkModel != null) {
                    mBookmarkModel.deleteBookmarks(bookmarkId);
                    RecordUserAction.record("Android.BookmarkPage.RemoveItem");
                    if (bookmarkId.getType() == BookmarkType.READING_LIST) {
                        RecordUserAction.record("Android.BookmarkPage.ReadingList.RemoveItem");
                    }
                }
            } else if (textId == R.string.bookmark_show_in_folder) {
                BookmarkItem bookmarkItem = mBookmarkModel.getBookmarkById(bookmarkId);
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
        return new BasicListMenu(mContext, listItems, delegate);
    }

    void setPriceTrackingEnabled(PropertyModel model, boolean enabled) {
        BookmarkListEntry entry = model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);

        Callback<Boolean> callback = success -> {
            if (!success) return;
            ShoppingAccessoryCoordinator shoppingAccessoryCoordinator =
                    model.get(ImprovedBookmarkRowProperties.SHOPPING_ACCESSORY_COORDINATOR);
            shoppingAccessoryCoordinator.setPriceTrackingEnabled(enabled);
        };

        PowerBookmarkUtils.setPriceTrackingEnabledWithSnackbars(mBookmarkModel,
                entry.getBookmarkItem().getId(), enabled, mSnackbarManager, mContext.getResources(),
                mProfile, callback);
    }

    void openBookmarkId(BookmarkId id) {
        BookmarkItem item = mBookmarkModel.getBookmarkById(id);
        if (item.isFolder()) {
            openFolder(id);
        } else {
            openBookmark(id);
        }
    }

    private void onQueryCallback(String text) {
        final @BookmarkUiMode int currentUiMode = getCurrentUiMode();
        if (!TextUtils.isEmpty(text)) {
            // #setState will no-op if we're already in a search state.
            setState(BookmarkUiState.createSearchState());
            search(text);
        } else if (currentUiMode == BookmarkUiMode.SEARCHING) {
            onEndSearch();
        }
    }

    // Testing methods.

    /** Whether to prevent the bookmark model from fully loading for testing. */
    static void preventLoadingForTesting(boolean preventLoading) {
        sPreventLoadingForTesting = preventLoading;
    }

    void finishLoadingForTesting() {
        finishLoadingBookmarkModel();
    }

    void clearStateStackForTesting() {
        mStateStack.clear();
    }

    BookmarkUndoController getUndoControllerForTesting() {
        return mBookmarkUndoController;
    }
}
