// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.text.TextUtils;
import android.view.accessibility.AccessibilityManager;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.AdapterDataObserver;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkItemsAdapter.ViewFactory;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksReader;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;

import java.util.List;
import java.util.Stack;

/** Respondible for BookmarkManager business logic. */
// TODO(crbug.com/1416611): Remove BookmarkDelegate if possible.
class BookmarkManagerMediator
        implements BookmarkDelegate, PartnerBookmarksReader.FaviconUpdateObserver {
    private static boolean sPreventLoadingForTesting;

    /**
     * Keeps track of whether drag is enabled / active for bookmark lists.
     */
    class BookmarkDragStateDelegate implements DragStateDelegate {
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
            return !mA11yEnabled
                    && mBookmarkDelegate.getCurrentState() == BookmarkUIState.STATE_FOLDER;
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
            mBookmarkItemsAdapter.refresh();
        }

        @Override
        @SuppressWarnings("NotifyDataSetChanged")
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            // If the folder is removed in folder mode, show the parent folder or falls back to all
            // bookmarks mode.
            if (getCurrentState() == BookmarkUIState.STATE_FOLDER
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
            if (getCurrentState() == BookmarkUIState.STATE_FOLDER && !mStateStack.isEmpty()
                    && node.getId().equals(mStateStack.peek().mFolder)) {
                notifyUI(mStateStack.peek());
                return;
            }
            super.bookmarkNodeChanged(node);
        }

        @Override
        public void bookmarkModelChanged() {
            // If the folder no longer exists in folder mode, we need to fall back. Relying on the
            // default behavior by setting the folder mode again.
            if (getCurrentState() == BookmarkUIState.STATE_FOLDER) {
                setState(mStateStack.peek());
            }
        }
    };

    private final Stack<BookmarkUIState> mStateStack = new Stack<>() {
        @Override
        public BookmarkUIState push(BookmarkUIState item) {
            // The back press state depends on the size of stack. So push/pop item first in order
            // to keep the size update-to-date.
            var state = super.push(item);
            onBackPressStateChanged();
            return state;
        }

        @Override
        public synchronized BookmarkUIState pop() {
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

    private final ObserverList<BookmarkUIObserver> mUIObservers = new ObserverList<>();
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
    // TODO(crbug.com/1416611): Remove reference to BookmarkActionBar.
    // Owned by BookmarkManager(Coordinator).
    private final BookmarkActionBar mBookmarkActionBar;
    private final LargeIconBridge mLargeIconBridge;
    /** Whether we're showing in a dialog UI which is only true for phones. */
    private final boolean mIsDialogUi;
    private final boolean mIsIncognito;
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier;
    private final ViewFactory mViewFactory;

    /** Whether this instance has been destroyed. */
    private boolean mIsDestroyed;
    private String mInitialUrl;
    private boolean mFaviconsNeedRefresh;
    private BasicNativePage mNativePage;

    BookmarkManagerMediator(Context context, BookmarkModel bookmarkModel,
            BookmarkOpener bookmarkOpener, SelectableListLayout<BookmarkId> selectableListLayout,
            SelectionDelegate<BookmarkId> selectionDelegate, RecyclerView recyclerView,
            BookmarkItemsAdapter bookmarkItemsAdapter, BookmarkActionBar bookmarkActionBar,
            LargeIconBridge largeIconBridge, boolean isDialogUi, boolean isIncognito,
            ObservableSupplierImpl<Boolean> backPressStateSupplier, ViewFactory viewFactory) {
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
        mBookmarkActionBar = bookmarkActionBar;
        mLargeIconBridge = largeIconBridge;
        mIsDialogUi = isDialogUi;
        mIsIncognito = isIncognito;
        mBackPressStateSupplier = backPressStateSupplier;
        mViewFactory = viewFactory;

        // Previously we were waiting for BookmarkModel to be loaded, but it's not necessary.
        PartnerBookmarksReader.addFaviconUpdateObserver(this);

        initializeToLoadingState();
        if (!sPreventLoadingForTesting) {
            mBookmarkModel.finishLoadingBookmarkModel(this::onBookmarkModelLoaded);
        }
    }

    void onBookmarkModelLoaded() {
        mDragStateDelegate.onBookmarkDelegateInitialized(this);
        mBookmarkItemsAdapter.onBookmarkDelegateInitialized(this, mViewFactory);
        mBookmarkActionBar.onBookmarkDelegateInitialized(this);
        mBookmarkItemsAdapter.addDragListener(mBookmarkActionBar);

        if (!TextUtils.isEmpty(mInitialUrl)) {
            setState(BookmarkUIState.createStateFromUrl(mInitialUrl, mBookmarkModel));
        }
    }

    void onDestroy() {
        mIsDestroyed = true;
        mBookmarkItemsAdapter.unregisterAdapterDataObserver(mBookmarkItemsAdapterDataObserver);
        mBookmarkModel.removeObserver(mBookmarkModelObserver);

        mLargeIconBridge.destroy();
        PartnerBookmarksReader.removeFaviconUpdateObserver(this);

        for (BookmarkUIObserver observer : mUIObservers) {
            observer.onDestroy();
        }
        assert mUIObservers.size() == 0;
    }

    /** See BookmarkManager(Coordinator)#onBackPressed. */
    boolean onBackPressed() {
        if (mIsDestroyed) return false;

        // TODO(twellington): replicate this behavior for other list UIs during unification.
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

    /** See BookmarkManager(Coordinator)#setBasicNativePage. */
    void setBasicNativePage(BasicNativePage nativePage) {
        mNativePage = nativePage;
    }

    /** See BookmarkManager(Coordinator)#updateForUrl */
    void updateForUrl(String url) {
        // Bookmark model is null if the manager has been destroyed.
        if (mBookmarkModel == null) return;

        if (mBookmarkModel.isBookmarkModelLoaded()) {
            BookmarkUIState searchState = null;
            if (!mStateStack.isEmpty()
                    && mStateStack.peek().mState == BookmarkUIState.STATE_SEARCHING) {
                searchState = mStateStack.pop();
            }

            setState(BookmarkUIState.createStateFromUrl(url, mBookmarkModel));

            if (searchState != null) setState(searchState);
        } else {
            mInitialUrl = url;
        }
    }

    /** See BookmarkManager(Coordinator)#getCurrentUrl. */
    String getCurrentUrl() {
        if (mStateStack.isEmpty()) return null;
        return mStateStack.peek().mUrl;
    }

    /**
     * Puts all UI elements to loading state. This state might be overridden synchronously by
     * {@link #updateForUrl(String)}, if the bookmark model is already loaded.
     */
    private void initializeToLoadingState() {
        mBookmarkActionBar.showLoadingUi();
        assert mStateStack.isEmpty();
        setState(BookmarkUIState.createLoadingState());
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
    private void setState(BookmarkUIState state) {
        if (!state.isValid(mBookmarkModel)) {
            state = BookmarkUIState.createFolderState(
                    mBookmarkModel.getDefaultFolderViewLocation(), mBookmarkModel);
        }

        if (!mStateStack.isEmpty() && mStateStack.peek().equals(state)) return;

        // The loading state is not persisted in history stack and once we have a valid state it
        // shall be removed.
        if (!mStateStack.isEmpty() && mStateStack.peek().mState == BookmarkUIState.STATE_LOADING) {
            mStateStack.pop();
        }
        mStateStack.push(state);
        notifyUI(state);
    }

    private void notifyUI(BookmarkUIState state) {
        if (state.mState == BookmarkUIState.STATE_FOLDER) {
            // Loading and searching states may be pushed to the stack but should never be stored in
            // preferences.
            BookmarkUtils.setLastUsedUrl(mContext, state.mUrl);
            // If a loading state is replaced by another loading state, do not notify this change.
            if (mNativePage != null) {
                mNativePage.onStateChange(state.mUrl, false);
            }
        }

        for (BookmarkUIObserver observer : mUIObservers) {
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
            if (mSelectionDelegate.isItemSelected(node)
                    && mBookmarkItemsAdapter.getPositionForBookmark(node) == -1) {
                mSelectionDelegate.toggleSelectionForItem(node);
            }
        }
    }

    // BookmarkDelegate implementation

    @Override
    public void moveDownOne(BookmarkId bookmarkId) {
        mBookmarkItemsAdapter.moveDownOne(bookmarkId);
    }

    @Override
    public void moveUpOne(BookmarkId bookmarkId) {
        mBookmarkItemsAdapter.moveUpOne(bookmarkId);
    }

    @Override
    public void onBookmarkItemMenuOpened() {
        mBookmarkActionBar.hideKeyboard();
    }

    @Override
    public boolean isDialogUi() {
        return mIsDialogUi;
    }

    @Override
    public void openFolder(BookmarkId folder) {
        RecordUserAction.record("MobileBookmarkManagerOpenFolder");
        if (mBookmarkActionBar.isSearching()) mBookmarkActionBar.hideSearchView();
        setState(BookmarkUIState.createFolderState(folder, mBookmarkModel));
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
    public void notifyStateChange(BookmarkUIObserver observer) {
        int state = getCurrentState();
        switch (state) {
            case BookmarkUIState.STATE_FOLDER:
                observer.onFolderStateSet(mStateStack.peek().mFolder);
                break;
            case BookmarkUIState.STATE_LOADING:
                // In loading state, onBookmarkDelegateInitialized() is not called for all
                // UIObservers, which means that there will be no observers at the time. Do nothing.
                assert mUIObservers.isEmpty();
                break;
            case BookmarkUIState.STATE_SEARCHING:
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
    public void openSearchUI() {
        setState(BookmarkUIState.createSearchState());
        mSelectableListLayout.onStartSearch(R.string.bookmark_no_result);
        mBookmarkActionBar.showSearchView(true);
    }

    @Override
    public void closeSearchUI() {
        mBookmarkActionBar.hideSearchView();
    }

    @Override
    public void addUIObserver(BookmarkUIObserver observer) {
        mUIObservers.addObserver(observer);
    }

    @Override
    public void removeUIObserver(BookmarkUIObserver observer) {
        mUIObservers.removeObserver(observer);
    }

    @Override
    public BookmarkModel getModel() {
        return mBookmarkModel;
    }

    @Override
    public int getCurrentState() {
        if (mStateStack.isEmpty()) return BookmarkUIState.STATE_LOADING;
        return mStateStack.peek().mState;
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
        mBookmarkItemsAdapter.highlightBookmark(bookmarkId);
    }

    // SearchDelegate implementation.
    // Actual interface implemented in BookmarkManager(Coordinator).

    void onSearchTextChanged(String query) {
        mBookmarkItemsAdapter.search(query);
    }

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
            mBookmarkItemsAdapter.refresh();
            mFaviconsNeedRefresh = false;
        }
    }

    // Private methods.

    private void onBackPressStateChanged() {
        if (mIsDestroyed) {
            mBackPressStateSupplier.set(false);
            return;
        }
        mBackPressStateSupplier.set(
                Boolean.TRUE.equals(mSelectableListLayout.getHandleBackPressChangedSupplier().get())
                || mStateStack.size() > 1);
    }

    // Testing methods.

    /** Whether to prevent the bookmark model from fully loading for testing. */
    static void preventLoadingForTesting(boolean preventLoading) {
        sPreventLoadingForTesting = preventLoading;
    }

    void clearStateStackForTesting() {
        mStateStack.clear();
    }
}
