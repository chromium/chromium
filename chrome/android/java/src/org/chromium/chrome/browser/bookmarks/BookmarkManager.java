// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.AdapterDataObserver;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityManager;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegate;
import org.chromium.chrome.browser.native_page.BasicNativePage;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksReader;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.widget.dragreorder.DragStateDelegate;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar.SearchDelegate;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.Stack;

/**
 * The new bookmark manager that is planned to replace the existing bookmark manager. It holds all
 * views and shared logics between tablet and phone. For tablet/phone specific logics, see
 * {@link BookmarkActivity} (phone) and {@link BookmarkPage} (tablet).
 */
public class BookmarkManager
        implements BookmarkDelegate, SearchDelegate, PartnerBookmarksReader.FaviconUpdateObserver {
    private static final int FAVICON_MAX_CACHE_SIZE_BYTES =
            10 * ConversionUtils.BYTES_PER_MEGABYTE; // 10MB

    /**
     * This shared preference used to be used to save a list of recent searches. That feature
     * has been removed, so this string is now used solely to clear the shared preference.
     */
    private static final String PREF_SEARCH_HISTORY = "bookmark_search_history";

    private static boolean sPreventLoadingForTesting;

    private Activity mActivity;
    private ViewGroup mMainView;
    private BookmarkModel mBookmarkModel;
    private BookmarkUndoController mUndoController;
    private final ObserverList<BookmarkUIObserver> mUIObservers = new ObserverList<>();
    private BasicNativePage mNativePage;
    private SelectableListLayout<BookmarkId> mSelectableListLayout;
    private RecyclerView mRecyclerView;
    private TextView mEmptyView;
    private BookmarkActionBar mToolbar;
    private SelectionDelegate<BookmarkId> mSelectionDelegate;
    private final Stack<BookmarkUIState> mStateStack = new Stack<>();
    private LargeIconBridge mLargeIconBridge;
    private boolean mFaviconsNeedRefresh;
    private String mInitialUrl;
    private boolean mIsDialogUi;
    private boolean mIsDestroyed;

    // TODO(crbug.com/160194): Clean up after bookmark reordering launches.
    private ItemsAdapter mAdapter;
    private BookmarkDragStateDelegate mDragStateDelegate;
    private AdapterDataObserver mAdapterDataObserver;

    /**
     * An adapter responsible for managing bookmark items.
     */
    interface ItemsAdapter {
        void refresh();
        void notifyDataSetChanged();
        void onBookmarkDelegateInitialized(BookmarkDelegate bookmarkDelegate);
        void search(String query);
        void registerAdapterDataObserver(AdapterDataObserver observer);
        void unregisterAdapterDataObserver(AdapterDataObserver observer);

        void moveUpOne(BookmarkId bookmarkId);
        void moveDownOne(BookmarkId bookmarkId);

        void highlightBookmark(BookmarkId bookmarkId);
        int getPositionForBookmark(BookmarkId bookmarkId);
    }

    private final BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeChildrenReordered(BookmarkItem node) {
            mAdapter.refresh();
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            // If the folder is removed in folder mode, show the parent folder or falls back to all
            // bookmarks mode.
            if (getCurrentState() == BookmarkUIState.STATE_FOLDER
                    && node.getId().equals(mStateStack.peek().mFolder)) {
                if (mBookmarkModel.getTopLevelFolderIDs(true, true).contains(
                        node.getId())) {
                    openFolder(mBookmarkModel.getDefaultFolder());
                } else {
                    openFolder(parent.getId());
                }
            }

            // This is necessary as long as we rely on RecyclerView.ItemDecorations to apply padding
            // at the bottom of the bookmarks list to avoid the bottom navigation menu. This ensures
            // the item decorations are reapplied correctly when item indices change as the result
            // of an item being deleted.
            mAdapter.notifyDataSetChanged();
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

    /**
     * Creates an instance of {@link BookmarkManager}. It also initializes resources,
     * bookmark models and jni bridges.
     * @param activity The activity context to use.
     * @param isDialogUi Whether the main bookmarks UI will be shown in a dialog, not a NativePage.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     */
    public BookmarkManager(Activity activity, boolean isDialogUi, SnackbarManager snackbarManager) {
        mActivity = activity;
        mIsDialogUi = isDialogUi;
        boolean reorderBookmarksEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.REORDER_BOOKMARKS);

        PartnerBookmarksReader.addFaviconUpdateObserver(this);

        mSelectionDelegate = new SelectionDelegate<BookmarkId>() {
            @Override
            public boolean toggleSelectionForItem(BookmarkId bookmark) {
                if (mBookmarkModel.getBookmarkById(bookmark) != null
                        && !mBookmarkModel.getBookmarkById(bookmark).isEditable()) {
                    return false;
                }
                return super.toggleSelectionForItem(bookmark);
            }
        };

        if (reorderBookmarksEnabled) {
            mDragStateDelegate = new BookmarkDragStateDelegate();
        }

        mBookmarkModel = new BookmarkModel();
        mMainView = (ViewGroup) mActivity.getLayoutInflater().inflate(R.layout.bookmark_main, null);

        @SuppressWarnings("unchecked")
        SelectableListLayout<BookmarkId> selectableList =
                mMainView.findViewById(R.id.selectable_list);
        mSelectableListLayout = selectableList;
        mEmptyView = mSelectableListLayout.initializeEmptyView(
                R.string.bookmarks_folder_empty, R.string.bookmark_no_result);

        if (reorderBookmarksEnabled) {
            mAdapter = new ReorderBookmarkItemsAdapter(activity);
        } else {
            mAdapter = new BookmarkItemsAdapter(activity);
        }
        mAdapterDataObserver = new AdapterDataObserver() {
            @Override
            public void onItemRangeRemoved(int positionStart, int itemCount) {
                syncAdapterAndSelectionDelegate();
            }

            @Override
            public void onChanged() {
                syncAdapterAndSelectionDelegate();
            }
        };
        mAdapter.registerAdapterDataObserver(mAdapterDataObserver);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(
                (RecyclerView.Adapter<RecyclerView.ViewHolder>) mAdapter);

        mToolbar = (BookmarkActionBar) mSelectableListLayout.initializeToolbar(
                R.layout.bookmark_action_bar, mSelectionDelegate, 0, R.id.normal_menu_group,
                R.id.selection_mode_menu_group, null, true, isDialogUi);
        mToolbar.initializeSearchView(
                this, R.string.bookmark_action_bar_search, R.id.search_menu_id);

        mSelectableListLayout.configureWideDisplayStyle();

        mUndoController = new BookmarkUndoController(activity, mBookmarkModel, snackbarManager);
        mBookmarkModel.addObserver(mBookmarkModelObserver);
        initializeToLoadingState();
        if (!sPreventLoadingForTesting) {
            Runnable modelLoadedRunnable = () -> {
                if (reorderBookmarksEnabled) {
                    mDragStateDelegate.onBookmarkDelegateInitialized(BookmarkManager.this);
                }
                mAdapter.onBookmarkDelegateInitialized(BookmarkManager.this);
                mToolbar.onBookmarkDelegateInitialized(BookmarkManager.this);

                if (reorderBookmarksEnabled) {
                    ((ReorderBookmarkItemsAdapter) mAdapter).addDragListener(mToolbar);
                }

                if (!TextUtils.isEmpty(mInitialUrl)) {
                    setState(BookmarkUIState.createStateFromUrl(mInitialUrl, mBookmarkModel));
                }
            };
            mBookmarkModel.finishLoadingBookmarkModel(modelLoadedRunnable);
        }

        mLargeIconBridge = new LargeIconBridge(Profile.getLastUsedProfile().getOriginalProfile());
        ActivityManager activityManager = ((ActivityManager) ContextUtils
                .getApplicationContext().getSystemService(Context.ACTIVITY_SERVICE));
        int maxSize =
                Math.min(activityManager.getMemoryClass() / 4 * ConversionUtils.BYTES_PER_MEGABYTE,
                        FAVICON_MAX_CACHE_SIZE_BYTES);
        mLargeIconBridge.createCache(maxSize);

        RecordUserAction.record("MobileBookmarkManagerOpen");
        if (!isDialogUi) {
            RecordUserAction.record("MobileBookmarkManagerPageOpen");
        }

        // TODO(twellington): Remove this when Chrome version 59 is a distant memory and users
        // are unlikely to have the old PREF_SEARCH_HISTORY in shared preferences.
        ContextUtils.getAppSharedPreferences().edit().remove(PREF_SEARCH_HISTORY).apply();
    }

    @Override
    public void onUpdateFavicon(String url) {
        mLargeIconBridge.clearFavicon(url);
        mFaviconsNeedRefresh = true;
    }

    @Override
    public void onCompletedFaviconLoading() {
        if (mFaviconsNeedRefresh) {
            mAdapter.refresh();
            mFaviconsNeedRefresh = false;
        }
    }

    /**
     * Destroys and cleans up itself. This must be called after done using this class.
     */
    public void onDestroyed() {
        mAdapter.unregisterAdapterDataObserver(mAdapterDataObserver);
        mIsDestroyed = true;
        RecordUserAction.record("MobileBookmarkManagerClose");
        mSelectableListLayout.onDestroyed();

        for (BookmarkUIObserver observer : mUIObservers) {
            observer.onDestroy();
        }
        assert mUIObservers.size() == 0;

        if (mUndoController != null) {
            mUndoController.destroy();
            mUndoController = null;
        }
        mBookmarkModel.removeObserver(mBookmarkModelObserver);
        mBookmarkModel.destroy();
        mBookmarkModel = null;
        mLargeIconBridge.destroy();
        mLargeIconBridge = null;
        PartnerBookmarksReader.removeFaviconUpdateObserver(this);
    }

    /**
     * Called when the user presses the back key. This is only going to be called on Phone.
     * @return True if manager handles this event, false if it decides to ignore.
     */
    public boolean onBackPressed() {
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

    /**
     * @return The view that shows the main browsing history UI.
     */
    public View getView() {
        return mMainView;
    }

    /**
     * Sets the listener that reacts upon the change of the UI state of bookmark manager.
     */
    public void setBasicNativePage(BasicNativePage nativePage) {
        mNativePage = nativePage;
    }

    /**
     * Sets the delegate object needed for history navigation logic.
     * @param delegate {@link HistoryNavigationDelegate} object.
     */
    public void setHistoryNavigationDelegate(HistoryNavigationDelegate delegate) {
        mSelectableListLayout.setHistoryNavigationDelegate(delegate);
    }

    /**
     * @return Current URL representing the UI state of bookmark manager. If no state has been shown
     *         yet in this session, on phone return last used state stored in preference; on tablet
     *         return the url previously set by {@link #updateForUrl(String)}.
     */
    public String getCurrentUrl() {
        if (mStateStack.isEmpty()) return null;
        return mStateStack.peek().mUrl;
    }

    /**
     * Updates UI based on the new URL. If the bookmark model is not loaded yet, cache the url and
     * it will be picked up later when the model is loaded. This method is supposed to align with
     * {@link BookmarkPage#updateForUrl(String)}
     * <p>
     * @param url The url to navigate to.
     */
    public void updateForUrl(String url) {
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

    /**
     * Puts all UI elements to loading state. This state might be overridden synchronously by
     * {@link #updateForUrl(String)}, if the bookmark model is already loaded.
     */
    private void initializeToLoadingState() {
        mToolbar.showLoadingUi();
        assert mStateStack.isEmpty();
        setState(BookmarkUIState.createLoadingState());
    }

    /**
     * This is the ultimate internal method that updates UI and controls backstack. And it is the
     * only method that pushes states to {@link #mStateStack}.
     * <p>
     * If the given state is not valid, all_bookmark state will be shown. Afterwards, this method
     * checks the current state: if currently in loading state, it pops it out and adds the new
     * state to the back stack. It also notifies the {@link #mNativePage} (if any) that the
     * url has changed.
     * <p>
     * Also note that even if we store states to {@link #mStateStack}, on tablet the back navigation
     * and back button are not controlled by the manager: the tab handles back key and backstack
     * navigation.
     */
    private void setState(BookmarkUIState state) {
        if (!state.isValid(mBookmarkModel)) {
            state = BookmarkUIState.createFolderState(mBookmarkModel.getDefaultFolder(),
                    mBookmarkModel);
        }

        if (!mStateStack.isEmpty() && mStateStack.peek().equals(state)) return;

        // The loading state is not persisted in history stack and once we have a valid state it
        // shall be removed.
        if (!mStateStack.isEmpty()
                && mStateStack.peek().mState == BookmarkUIState.STATE_LOADING) {
            mStateStack.pop();
        }
        mStateStack.push(state);

        if (state.mState == BookmarkUIState.STATE_FOLDER) {
            // Loading and searching states may be pushed to the stack but should never be stored in
            // preferences.
            BookmarkUtils.setLastUsedUrl(mActivity, state.mUrl);
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
                    && mAdapter.getPositionForBookmark(node) == -1) {
                mSelectionDelegate.toggleSelectionForItem(node);
            }
        }
    }

    @Override
    public void moveDownOne(BookmarkId bookmarkId) {
        mAdapter.moveDownOne(bookmarkId);
    }

    @Override
    public void moveUpOne(BookmarkId bookmarkId) {
        mAdapter.moveUpOne(bookmarkId);
    }

    @Override
    public void onBookmarkItemMenuOpened() {
        mToolbar.hideKeyboard();
    }

    // BookmarkDelegate implementations.

    @Override
    public boolean isDialogUi() {
        return mIsDialogUi;
    }

    @Override
    public void openFolder(BookmarkId folder) {
        RecordUserAction.record("MobileBookmarkManagerOpenFolder");
        if (mToolbar.isSearching()) mToolbar.hideSearchView();
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
    public void openBookmark(BookmarkId bookmark, int launchLocation) {
        if (BookmarkUtils.openBookmark(
                    mBookmarkModel, mActivity, bookmark, launchLocation)) {
            BookmarkUtils.finishActivityOnPhone(mActivity);
        }
    }

    @Override
    public void openSearchUI() {
        setState(BookmarkUIState.createSearchState());
        mSelectableListLayout.onStartSearch();
        mToolbar.showSearchView();
    }

    @Override
    public void closeSearchUI() {
        mToolbar.hideSearchView();
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

    // SearchDelegate overrides

    @Override
    public void onSearchTextChanged(String query) {
        mAdapter.search(query);
    }

    @Override
    public void onEndSearch() {
        mSelectableListLayout.onEndSearch();

        // Pop the search state off the stack.
        mStateStack.pop();

        // Set the state back to the folder that was previously being viewed. Listeners, including
        // the BookmarkItemsAdapter, will be notified of the change and the list of bookmarks will
        // be updated.
        setState(mStateStack.pop());
    }

    @Override
    public void highlightBookmark(BookmarkId bookmarkId) {
        mAdapter.highlightBookmark(bookmarkId);
    }

    // Testing methods

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

    /**
     * @param preventLoading Whether to prevent the bookmark model from fully loading for testing.
     */
    @VisibleForTesting
    public static void preventLoadingForTesting(boolean preventLoading) {
        sPreventLoadingForTesting = preventLoading;
    }
}