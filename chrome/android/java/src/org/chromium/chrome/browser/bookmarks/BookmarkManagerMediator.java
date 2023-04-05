// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.AdapterDataObserver;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkRow.Location;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksReader;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncService.SyncStateChangedListener;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Stack;

/** Responsible for BookmarkManager business logic. */
// TODO(crbug.com/1416611): Remove BookmarkDelegate if possible.
class BookmarkManagerMediator implements BookmarkDelegate, TestingDelegate,
                                         PartnerBookmarksReader.FaviconUpdateObserver,
                                         BookmarkItemsAdapter.ViewDelegate {
    private static final int MAXIMUM_NUMBER_OF_SEARCH_RESULTS = 500;
    private static final String EMPTY_QUERY = null;

    private static boolean sPreventLoadingForTesting;

    /**
     * Keeps track of whether drag is enabled / active for bookmark lists.
     */
    private class BookmarkDragStateDelegate implements DragStateDelegate {
        private BookmarkDelegate mBookmarkDelegate;
        private SelectionDelegate<BookmarkId> mSelectionDelegate;
        private AccessibilityManager mA11yManager;
        private AccessibilityManager.AccessibilityStateChangeListener mA11yChangeListener;
        private boolean mA11yEnabled;

        void onBookmarkDelegateInitialized(BookmarkDelegate delegate) {
            mBookmarkDelegate = delegate;

            mSelectionDelegate = delegate.getSelectionDelegate();

            mA11yManager =
                    (AccessibilityManager) getSelectableListLayout().getContext().getSystemService(
                            Context.ACCESSIBILITY_SERVICE);
            mA11yEnabled = mA11yManager.isEnabled();
            mA11yChangeListener = enabled -> mA11yEnabled = enabled;
            mA11yManager.addAccessibilityStateChangeListener(mA11yChangeListener);
        }

        // DragStateDelegate implementation
        @Override
        public boolean getDragEnabled() {
            return !mA11yEnabled && mBookmarkDelegate.getCurrentUiMode() == BookmarkUiMode.FOLDER;
        }

        @Override
        public boolean getDragActive() {
            return getDragEnabled() && mSelectionDelegate.isSelectionEnabled();
        }

        @VisibleForTesting
        @Override
        public void setA11yStateForTesting(boolean a11yEnabled) {
            if (mA11yManager != null) {
                mA11yManager.removeAccessibilityStateChangeListener(mA11yChangeListener);
            }
            mA11yChangeListener = null;
            mA11yManager = null;
            mA11yEnabled = a11yEnabled;
        }
    }

    private final BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeChildrenReordered(BookmarkItem node) {
            refresh();
        }

        @Override
        @SuppressWarnings("NotifyDataSetChanged")
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            // If the folder is removed in folder mode, show the parent folder or falls back to all
            // bookmarks mode.
            if (getCurrentUiMode() == BookmarkUiMode.FOLDER
                    && node.getId().equals(mStateStack.peek().mFolder)) {
                if (mBookmarkModel.getTopLevelFolderIDs(true, true).contains(node.getId())) {
                    openFolder(mBookmarkModel.getDefaultFolderViewLocation());
                } else {
                    openFolder(parent.getId());
                }
            }

            // This is necessary as long as we rely on RecyclerView.ItemDecorations to apply padding
            // at the bottom of the bookmarks list to avoid the bottom navigation menu. This ensures
            // the item decorations are reapplied correctly when item indices change as the result
            // of an item being deleted.
            mBookmarkItemsAdapter.notifyDataSetChanged();
        }

        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            if (getCurrentUiMode() == BookmarkUiMode.FOLDER && !mStateStack.isEmpty()
                    && node.getId().equals(mStateStack.peek().mFolder)) {
                notifyUi(mStateStack.peek());
                return;
            }
            super.bookmarkNodeChanged(node);
        }

        @Override
        public void bookmarkModelChanged() {
            // If the folder no longer exists in folder mode, we need to fall back. Relying on the
            // default behavior by setting the folder mode again.
            if (getCurrentUiMode() == BookmarkUiMode.FOLDER) {
                setState(mStateStack.peek());
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
        // TODO(crbug.com/1419493): Investigate use of synchronized.
        public synchronized BookmarkUiState pop() {
            var state = super.pop();
            onBackPressStateChanged();
            return state;
        }
    };

    private final AdapterDataObserver mBookmarkItemsAdapterDataObserver =
            new AdapterDataObserver() {
                @Override
                public void onItemRangeRemoved(int positionStart, int itemCount) {
                    syncAdapterAndSelectionDelegate();
                }

                @Override
                public void onChanged() {
                    syncAdapterAndSelectionDelegate();
                }
            };

    // TODO(https://crbug.com/1413463): Combine with mBookmarkModelObserver.
    private BookmarkModelObserver mBookmarkModelObserver2 = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            clearHighlight();
            int position = getPositionForBookmark(node.getId());
            if (position >= 0) mBookmarkItemsAdapter.notifyItemChanged(position);
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            clearHighlight();

            if (getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
                // We cannot rely on removing the specific list item that corresponds to the
                // removed node because the node might be a parent with children also shown
                // in the list.
                search(mSearchText);
                return;
            }

            if (node.isFolder()) {
                notifyStateChange(mBookmarkUiObserver);
            } else {
                int deletedPosition = getPositionForBookmark(node.getId());
                if (deletedPosition >= 0) {
                    removeItem(deletedPosition);
                }
            }
        }

        @Override
        public void bookmarkModelChanged() {
            clearHighlight();
            notifyStateChange(mBookmarkUiObserver);

            if (getCurrentUiMode() == BookmarkUiMode.SEARCHING) {
                if (!TextUtils.equals(mSearchText, EMPTY_QUERY)) {
                    search(mSearchText);
                } else {
                    closeSearchUi();
                }
            }
        }
    };

    private final BookmarkUiObserver mBookmarkUiObserver = new BookmarkUiObserver() {
        @Override
        public void onDestroy() {
            removeUiObserver(mBookmarkUiObserver);
            mBookmarkModel.removeObserver(mBookmarkModelObserver2);
            getSelectionDelegate().removeObserver(mSelectionObserver);
            mPromoHeaderManager.destroy();
            mSyncService.removeSyncStateChangedListener(mSyncStateChangedListener);
        }

        @Override
        public void onFolderStateSet(BookmarkId folder) {
            clearHighlight();

            mSearchText = EMPTY_QUERY;
            mCurrentFolder = folder;
            mBookmarkItemsAdapter.enableDrag();

            if (topLevelFoldersShowing()) {
                setBookmarks(mTopLevelFolders);
            } else {
                setBookmarks(mBookmarkModel.getChildIDs(folder));
            }

            if (BookmarkId.SHOPPING_FOLDER.equals(folder)) {
                getSelectableListLayout().setEmptyViewText(
                        R.string.tracked_products_empty_list_title);
            } else if (folder.getType() == BookmarkType.READING_LIST) {
                TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                        EventConstants.READ_LATER_BOOKMARK_FOLDER_OPENED);
                getSelectableListLayout().setEmptyViewText(R.string.reading_list_empty_list_title);
            } else {
                getSelectableListLayout().setEmptyViewText(R.string.bookmarks_folder_empty);
            }
        }

        @SuppressWarnings("NotifyDataSetChanged")
        @Override
        public void onSearchStateSet() {
            clearHighlight();
            mBookmarkItemsAdapter.disableDrag();
            // Headers should not appear in Search mode
            // Don't need to notify because we need to redraw everything in the next step
            updateHeader(false);
            removeSectionHeaders();
            mBookmarkItemsAdapter.notifyDataSetChanged();
        }
    };

    private final SelectionObserver<BookmarkId> mSelectionObserver = new SelectionObserver<>() {
        @Override
        public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
            clearHighlight();
        }
    };

    private final SyncStateChangedListener mSyncStateChangedListener =
            new SyncStateChangedListener() {
                @Override
                public void syncStateChanged() {
                    // If the bookmark model isn't loaded, we will set the top level folders on
                    // load (see onBookmarkModelLoaded method below).
                    if (!mBookmarkModel.isBookmarkModelLoaded()) {
                        return;
                    }
                    mTopLevelFolders.clear();
                    populateTopLevelFoldersList();
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
    // TODO(crbug.com/1416611): Remove reference to BookmarkItemsAdapter.
    private final BookmarkItemsAdapter mBookmarkItemsAdapter;
    private final LargeIconBridge mLargeIconBridge;
    // Whether we're showing in a dialog UI which is only true for phones.
    private final boolean mIsDialogUi;
    private final boolean mIsIncognito;
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier;
    private final List<BookmarkId> mTopLevelFolders = new ArrayList<>();
    private final Profile mProfile;
    private final SyncService mSyncService;
    private final BookmarkPromoHeader mPromoHeaderManager;
    private final BookmarkUndoController mBookmarkUndoController;

    // Whether this instance has been destroyed.
    private boolean mIsDestroyed;
    private String mInitialUrl;
    private boolean mFaviconsNeedRefresh;
    private BasicNativePage mNativePage;
    // There can only be one promo header at a time. This takes on one of the values:
    // ViewType.PERSONALIZED_SIGNIN_PROMO, ViewType.SYNC_PROMO, or ViewType.INVALID.
    private @ViewType int mPromoHeaderType = ViewType.INVALID;
    private String mSearchText;
    private BookmarkId mCurrentFolder;
    // Keep track of the currently highlighted bookmark - used for "show in folder" action.
    private BookmarkId mHighlightedBookmark;

    BookmarkManagerMediator(Context context, BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener, SelectableListLayout<BookmarkId> selectableListLayout,
            SelectionDelegate<BookmarkId> selectionDelegate, RecyclerView recyclerView,
            BookmarkItemsAdapter bookmarkItemsAdapter, LargeIconBridge largeIconBridge,
            boolean isDialogUi, boolean isIncognito,
            ObservableSupplierImpl<Boolean> backPressStateSupplier, Profile profile,
            BookmarkUndoController bookmarkUndoController) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        mBookmarkOpener = bookmarkOpener;
        mSelectableListLayout = selectableListLayout;
        mSelectableListLayout.getHandleBackPressChangedSupplier().addObserver(
                (x) -> onBackPressStateChanged());
        mSelectionDelegate = selectionDelegate;
        mRecyclerView = recyclerView;
        mBookmarkItemsAdapter = bookmarkItemsAdapter;
        mBookmarkItemsAdapter.registerAdapterDataObserver(mBookmarkItemsAdapterDataObserver);
        mLargeIconBridge = largeIconBridge;
        mIsDialogUi = isDialogUi;
        mIsIncognito = isIncognito;
        mBackPressStateSupplier = backPressStateSupplier;
        mProfile = profile;
        mSyncService = SyncService.get();
        mSyncService.addSyncStateChangedListener(mSyncStateChangedListener);
        // Notify the view of changes to the elements list as the promo might be showing.
        Runnable promoHeaderChangeAction = () -> updateHeader(true);
        mPromoHeaderManager = new BookmarkPromoHeader(mContext, mProfile, promoHeaderChangeAction);
        mBookmarkUndoController = bookmarkUndoController;

        // Previously we were waiting for BookmarkModel to be loaded, but it's not necessary.
        PartnerBookmarksReader.addFaviconUpdateObserver(this);

        initializeToLoadingState();
        if (!sPreventLoadingForTesting) {
            mBookmarkModel.finishLoadingBookmarkModel(this::onBookmarkModelLoaded);
        }
    }

    void onBookmarkModelLoaded() {
        mDragStateDelegate.onBookmarkDelegateInitialized(this);

        // TODO(https://crbug.com/1413463): This logic is here to keep the same execution order
        // from when it was in the original adapter. It doesn't coceptaully make senes to be here,
        // and should happen earlier.
        addUiObserver(mBookmarkUiObserver);
        mBookmarkModel.addObserver(mBookmarkModelObserver2);
        mSelectionDelegate.addObserver(mSelectionObserver);
        populateTopLevelFoldersList();

        mBookmarkItemsAdapter.onBookmarkDelegateInitialized(this, this);

        if (!TextUtils.isEmpty(mInitialUrl)) {
            setState(BookmarkUiState.createStateFromUrl(mInitialUrl, mBookmarkModel));
        }
    }

    void onDestroy() {
        mIsDestroyed = true;
        mBookmarkItemsAdapter.unregisterAdapterDataObserver(mBookmarkItemsAdapterDataObserver);
        mBookmarkModel.removeObserver(mBookmarkModelObserver);

        mLargeIconBridge.destroy();
        PartnerBookmarksReader.removeFaviconUpdateObserver(this);

        mBookmarkUndoController.destroy();

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
        List<BookmarkId> bookmarks =
                mBookmarkModel.searchBookmarks(mSearchText, MAXIMUM_NUMBER_OF_SEARCH_RESULTS);
        setBookmarks(bookmarks);
    }

    // BookmarkItemsAdapter.ViewDelegate implementation.

    @Override
    public PropertyModel buildModel(ViewHolder holder, int position) {
        PropertyModel model = new PropertyModel(BookmarkManagerProperties.ALL_KEYS);
        final @ViewType int viewType = holder.getItemViewType();
        if (viewType == ViewType.PERSONALIZED_SIGNIN_PROMO
                || viewType == ViewType.PERSONALIZED_SYNC_PROMO) {
            model.set(BookmarkManagerProperties.BOOKMARK_PROMO_HEADER, mPromoHeaderManager);
        } else if (viewType == ViewType.SECTION_HEADER) {
            model.set(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY, getItemByPosition(position));
        } else if (BookmarkListEntry.isBookmarkEntry(viewType)) {
            BookmarkId id = getIdByPosition(position);
            model.set(BookmarkManagerProperties.BOOKMARK_ID, id);
            model.set(BookmarkManagerProperties.LOCATION, getLocationFromPosition(position));
            model.set(BookmarkManagerProperties.IS_FROM_FILTER_VIEW,
                    BookmarkId.SHOPPING_FOLDER.equals(mCurrentFolder));
            model.set(BookmarkManagerProperties.ITEM_TOUCH_HELPER,
                    mBookmarkItemsAdapter.getItemTouchHelper());
            model.set(BookmarkManagerProperties.VIEW_HOLDER, holder);
            model.set(BookmarkManagerProperties.IS_HIGHLIGHTED, id.equals(mHighlightedBookmark));
            model.set(BookmarkManagerProperties.CLEAR_HIGHLIGHT, this::clearHighlight);
        } else if (viewType == ViewType.SHOPPING_FILTER) {
            model.set(BookmarkManagerProperties.OPEN_FOLDER, this::openFolder);
        }
        return model;
    }

    @Override
    public void recycleView(View view, @ViewType int viewType) {
        switch (viewType) {
            case ViewType.PERSONALIZED_SIGNIN_PROMO:
                // fall through
            case ViewType.PERSONALIZED_SYNC_PROMO:
                mPromoHeaderManager.detachPersonalizePromoView();
                break;
            default:
                // Other view holders don't have special recycling code.
        }
    }

    @Override
    public void setOrder(List<BookmarkListEntry> listEntries) {
        assert !topLevelFoldersShowing() : "Cannot reorder top-level folders!";
        assert mCurrentFolder.getType()
                != BookmarkType.PARTNER : "Cannot reorder partner bookmarks!";
        assert getCurrentUiMode()
                == BookmarkUiMode.FOLDER : "Can only reorder items from folder mode!";

        int startIndex = getBookmarkItemStartIndex();
        int endIndex = getBookmarkItemEndIndex();

        // Get the new order for the IDs.
        long[] newOrder = new long[endIndex - startIndex + 1];
        for (int i = startIndex; i <= endIndex; i++) {
            BookmarkItem bookmarkItem = listEntries.get(i).getBookmarkItem();
            assert bookmarkItem != null;
            newOrder[i - startIndex] = bookmarkItem.getId().getId();
        }
        mBookmarkModel.reorderBookmarks(mCurrentFolder, newOrder);
        if (mDragStateDelegate.getDragActive()) {
            RecordUserAction.record("MobileBookmarkManagerDragReorder");
        }
    }

    @Override
    public boolean isReorderable(BookmarkListEntry entry) {
        return entry != null && entry.getBookmarkItem() != null
                && entry.getBookmarkItem().isReorderable();
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
        mSyncStateChangedListener.syncStateChanged();
        mBookmarkUiObserver.onFolderStateSet(mCurrentFolder);
    }

    // BookmarkDelegate implementation.

    @Override
    public void moveDownOne(BookmarkId bookmarkId) {
        int pos = getPositionForBookmark(bookmarkId);
        assert isReorderable(getItemByPosition(pos));
        getElements().remove(pos);
        getElements().add(pos + 1,
                BookmarkListEntry.createBookmarkEntry(mBookmarkModel.getBookmarkById(bookmarkId),
                        mBookmarkModel.getPowerBookmarkMeta(bookmarkId)));
        setOrder(getElements());
    }

    @Override
    public void moveUpOne(BookmarkId bookmarkId) {
        int pos = getPositionForBookmark(bookmarkId);
        assert isReorderable(getItemByPosition(pos));
        getElements().remove(pos);
        getElements().add(pos - 1,
                BookmarkListEntry.createBookmarkEntry(mBookmarkModel.getBookmarkById(bookmarkId),
                        mBookmarkModel.getPowerBookmarkMeta(bookmarkId)));
        setOrder(getElements());
    }

    @Override
    public void onBookmarkItemMenuOpened() {}

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
            case BookmarkUiMode.FOLDER:
                observer.onFolderStateSet(mStateStack.peek().mFolder);
                break;
            case BookmarkUiMode.LOADING:
                break;
            case BookmarkUiMode.SEARCHING:
                observer.onSearchStateSet();
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
    public void closeSearchUi() {
        setState(mStateStack.pop());
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

        mRecyclerView.scrollToPosition(getPositionForBookmark(bookmarkId));
        mHighlightedBookmark = bookmarkId;
    }

    // SearchDelegate implementation.
    // Actual interface implemented in BookmarkManager(Coordinator).

    void onEndSearch() {
        mSelectableListLayout.onEndSearch();

        // Pop the search state off the stack.
        mStateStack.pop();

        // Set the state back to the folder that was previously being viewed. Listeners, including
        // the BookmarkItemsAdapter, will be notified of the change and the list of bookmarks will
        // be updated.
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

    private void filterForPriceTrackingCategory(List<BookmarkId> bookmarks) {
        for (int i = bookmarks.size() - 1; i >= 0; i--) {
            org.chromium.components.power_bookmarks.PowerBookmarkMeta meta =
                    mBookmarkModel.getPowerBookmarkMeta(bookmarks.get(i));
            if (meta == null || !meta.hasShoppingSpecifics()
                    || !meta.getShoppingSpecifics().getIsPriceTracked()) {
                bookmarks.remove(i);
                continue;
            }
        }
    }

    @SuppressWarnings("NotifyDataSetChanged")
    private void setBookmarks(List<BookmarkId> bookmarks) {
        clearHighlight();
        getElements().clear();

        // Restore the header, if it exists, then update it.
        if (hasPromoHeader()) {
            getElements().add(BookmarkListEntry.createSyncPromoHeader(mPromoHeaderType));
        }

        updateHeader(false);
        if (BookmarkId.SHOPPING_FOLDER.equals(mCurrentFolder)) {
            filterForPriceTrackingCategory(bookmarks);
        }

        for (BookmarkId bookmarkId : bookmarks) {
            BookmarkItem item = mBookmarkModel.getBookmarkById(bookmarkId);

            getElements().add(BookmarkListEntry.createBookmarkEntry(
                    item, mBookmarkModel.getPowerBookmarkMeta(bookmarkId)));
        }

        if (mCurrentFolder.getType() == BookmarkType.READING_LIST
                && getCurrentUiMode() != BookmarkUiMode.SEARCHING) {
            ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(getElements(), mContext);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SHOPPING_LIST)
                && topLevelFoldersShowing()) {
            getElements().add(BookmarkListEntry.createDivider());
            getElements().add(BookmarkListEntry.createShoppingFilter());
        }

        mBookmarkItemsAdapter.notifyDataSetChanged();
    }

    private void removeItem(int position) {
        getElements().remove(position);
        mBookmarkItemsAdapter.notifyItemRemoved(position);
    }

    /** Refresh the list of bookmarks within the currently visible folder. */
    @SuppressWarnings("NotifyDataSetChanged")
    private void refresh() {
        // Tell the RecyclerView to update its elements.
        if (getElements() != null) mBookmarkItemsAdapter.notifyDataSetChanged();
    }

    /**
     * Updates mPromoHeaderType. Makes sure that the 0th index of getElements() is consistent with
     * the promo header. This 0th index is null iff there is a promo header.
     *
     * @param shouldNotify True iff we should notify the RecyclerView of changes to the promoheader.
     *                     (This should be false iff we are going to make further changes to the
     *                     list of elements, as we do in setBookmarks, and true iff we are only
     *                     changing the header, as we do in the promoHeaderChangeAction runnable).
     */
    private void updateHeader(boolean shouldNotify) {
        boolean wasShowingPromo = hasPromoHeader();

        int currentUiState = getCurrentUiMode();
        if (currentUiState == BookmarkUiMode.LOADING) {
            return;
        } else if (currentUiState == BookmarkUiMode.SEARCHING) {
            mPromoHeaderType = ViewType.INVALID;
        } else {
            switch (mPromoHeaderManager.getPromoState()) {
                case SyncPromoState.NO_PROMO:
                    mPromoHeaderType = ViewType.INVALID;
                    break;
                case SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE:
                    mPromoHeaderType = ViewType.PERSONALIZED_SIGNIN_PROMO;
                    break;
                case SyncPromoState.PROMO_FOR_SIGNED_IN_STATE:
                    mPromoHeaderType = ViewType.PERSONALIZED_SYNC_PROMO;
                    break;
                case SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE:
                    mPromoHeaderType = ViewType.SYNC_PROMO;
                    break;
                default:
                    assert false : "Unexpected value for promo state!";
            }
        }

        boolean willShowPromo = hasPromoHeader();
        if (!wasShowingPromo && willShowPromo) {
            // A null element at the 0th index represents a promo header.
            getElements().add(0, BookmarkListEntry.createSyncPromoHeader(mPromoHeaderType));
            if (shouldNotify) mBookmarkItemsAdapter.notifyItemInserted(0);
        } else if (wasShowingPromo && willShowPromo) {
            if (shouldNotify) mBookmarkItemsAdapter.notifyItemChanged(0);
        } else if (wasShowingPromo && !willShowPromo) {
            getElements().remove(0);
            if (shouldNotify) mBookmarkItemsAdapter.notifyItemRemoved(0);
        }
    }

    /** Removes all section headers from the current list. */
    private void removeSectionHeaders() {
        for (int i = getElements().size() - 1; i >= 0; i--) {
            if (getElements().get(i).getViewType() == ViewType.SECTION_HEADER) {
                getElements().remove(i);
            }
        }
    }

    private void populateTopLevelFoldersList() {
        mTopLevelFolders.addAll(BookmarkUtils.populateTopLevelFolders(mBookmarkModel));
    }

    private int getBookmarkItemStartIndex() {
        return hasPromoHeader() ? 1 : 0;
    }

    private int getBookmarkItemEndIndex() {
        int endIndex = getElements().size() - 1;
        BookmarkItem bookmarkItem = getElements().get(endIndex).getBookmarkItem();
        if (bookmarkItem == null || !BookmarkUtils.isMovable(bookmarkItem)) {
            endIndex--;
        }
        return endIndex;
    }

    private boolean hasPromoHeader() {
        return mPromoHeaderType != ViewType.INVALID;
    }

    private @Location int getLocationFromPosition(int position) {
        if (position == getBookmarkItemStartIndex() && position == getBookmarkItemEndIndex()) {
            return Location.SOLO;
        } else if (position == getBookmarkItemStartIndex()) {
            return Location.TOP;
        } else if (position == getBookmarkItemEndIndex()) {
            return Location.BOTTOM;
        } else {
            return Location.MIDDLE;
        }
    }

    /**
     * Return true iff the currently-open folder is the root folder
     *         (which is true iff the top-level folders are showing)
     */
    private boolean topLevelFoldersShowing() {
        return mCurrentFolder.equals(mBookmarkModel.getRootFolderId());
    }

    /** Clears the highlighted bookmark, if there is one. */
    private void clearHighlight() {
        mHighlightedBookmark = null;
    }

    private BookmarkListEntry getItemByPosition(int position) {
        return getElements().get(position);
    }

    private List<BookmarkListEntry> getElements() {
        return mBookmarkItemsAdapter.getElements();
    }

    private int getItemCount() {
        return getElements().size();
    }

    // Testing methods.
    /** Whether to prevent the bookmark model from fully loading for testing. */
    static void preventLoadingForTesting(boolean preventLoading) {
        sPreventLoadingForTesting = preventLoading;
    }

    void clearStateStackForTesting() {
        mStateStack.clear();
    }

    BookmarkUndoController getUndoControllerForTesting() {
        return mBookmarkUndoController;
    }

    SyncStateChangedListener getSyncStateChangedListenerForTesting() {
        return mSyncStateChangedListener;
    }
}
