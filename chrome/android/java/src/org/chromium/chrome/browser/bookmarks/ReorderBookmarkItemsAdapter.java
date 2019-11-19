// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.LayoutRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkManager.ItemsAdapter;
import org.chromium.chrome.browser.bookmarks.BookmarkRow.Location;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.chrome.browser.ui.widget.highlight.ViewHighlighter;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * BaseAdapter for {@link RecyclerView}. It manages bookmarks to list there.
 */
class ReorderBookmarkItemsAdapter extends DragReorderableListAdapter<BookmarkItem>
        implements BookmarkUIObserver, ProfileSyncService.SyncStateChangedListener, ItemsAdapter {
    /**
     * Specifies the view types that the bookmark delegate screen can contain.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewType.PERSONALIZED_SIGNIN_PROMO, ViewType.SYNC_PROMO, ViewType.FOLDER,
            ViewType.BOOKMARK})
    private @interface ViewType {
        int INVALID_PROMO = -1;
        int PERSONALIZED_SIGNIN_PROMO = 0;
        int SYNC_PROMO = 1;
        int FOLDER = 2;
        int BOOKMARK = 3;
    }

    private static final int MAXIMUM_NUMBER_OF_SEARCH_RESULTS = 500;
    private static final String EMPTY_QUERY = null;

    private final List<BookmarkId> mTopLevelFolders = new ArrayList<>();

    // There can only be one promo header at a time. This takes on one of the values:
    // ViewType.PERSONALIZED_SIGNIN_PROMO, ViewType.SYNC_PROMO, or ViewType.INVALID_PROMO
    private int mPromoHeaderType = ViewType.INVALID_PROMO;
    private BookmarkDelegate mDelegate;
    private BookmarkPromoHeader mPromoHeaderManager;
    private String mSearchText;
    private BookmarkId mCurrentFolder;
    private ProfileSyncService mProfileSyncService;

    // Keep track of the currently highlighted bookmark - used for "show in folder" action.
    private BookmarkId mHighlightedBookmark;

    // For metrics
    private int mDragReorderCount;
    private int mMoveButtonCount;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            assert mDelegate != null;
            clearHighlight();
            int position = getPositionForBookmark(node.getId());
            if (position >= 0) notifyItemChanged(position);
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            assert mDelegate != null;
            clearHighlight();

            if (mDelegate.getCurrentState() == BookmarkUIState.STATE_SEARCHING
                    && TextUtils.equals(mSearchText, EMPTY_QUERY)) {
                mDelegate.closeSearchUI();
            }

            if (node.isFolder()) {
                mDelegate.notifyStateChange(ReorderBookmarkItemsAdapter.this);
            } else {
                int deletedPosition = getPositionForBookmark(node.getId());
                if (deletedPosition >= 0) {
                    removeItem(deletedPosition);
                }
            }
        }

        @Override
        public void bookmarkModelChanged() {
            assert mDelegate != null;
            clearHighlight();
            mDelegate.notifyStateChange(ReorderBookmarkItemsAdapter.this);

            if (mDelegate.getCurrentState() == BookmarkUIState.STATE_SEARCHING
                    && !TextUtils.equals(mSearchText, EMPTY_QUERY)) {
                search(mSearchText);
            }
        }
    };

    ReorderBookmarkItemsAdapter(Context context) {
        super(context);
        mProfileSyncService = ProfileSyncService.get();
        mProfileSyncService.addSyncStateChangedListener(this);
    }

    /**
     * @return The position of the given bookmark in adapter. Will return -1 if not found.
     */
    @Override
    public int getPositionForBookmark(BookmarkId bookmark) {
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

    private void setBookmarks(List<BookmarkId> bookmarks) {
        clearHighlight();
        mElements.clear();
        // Restore the header, if it exists, then update it.
        if (hasPromoHeader()) mElements.add(null);
        updateHeader(false);
        for (BookmarkId bId : bookmarks) {
            mElements.add(mDelegate.getModel().getBookmarkById(bId));
        }
        notifyDataSetChanged();
    }

    private void removeItem(int position) {
        mElements.remove(position);
        notifyItemRemoved(position);
    }

    // DragReorderableListAdapter implementation.
    @Override
    public @ViewType int getItemViewType(int position) {
        if (position == 0 && hasPromoHeader()) {
            return mPromoHeaderType;
        } else {
            BookmarkItem item = getItemByPosition(position);
            if (item.isFolder()) {
                return ViewType.FOLDER;
            } else {
                return ViewType.BOOKMARK;
            }
        }
    }

    private ViewHolder createViewHolderHelper(ViewGroup parent, @LayoutRes int layoutId) {
        // create the row associated with this adapter
        ViewGroup row = (ViewGroup) LayoutInflater.from(parent.getContext())
                                .inflate(layoutId, parent, false);

        // ViewHolder is abstract and it cannot be instantiated directly.
        ViewHolder holder = new ViewHolder(row) {};
        ((BookmarkRow) row).onDelegateInitialized(mDelegate);
        return holder;
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, @ViewType int viewType) {
        assert mDelegate != null;

        switch (viewType) {
            case ViewType.PERSONALIZED_SIGNIN_PROMO:
                return mPromoHeaderManager.createPersonalizedSigninPromoHolder(parent);
            case ViewType.SYNC_PROMO:
                return mPromoHeaderManager.createSyncPromoHolder(parent);
            case ViewType.FOLDER:
                return createViewHolderHelper(parent, R.layout.bookmark_folder_row);
            case ViewType.BOOKMARK:
                return createViewHolderHelper(parent, R.layout.bookmark_item_row);
            default:
                assert false;
                return null;
        }
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        if (holder.getItemViewType() == ViewType.PERSONALIZED_SIGNIN_PROMO) {
            PersonalizedSigninPromoView view = (PersonalizedSigninPromoView) holder.itemView;
            mPromoHeaderManager.setupPersonalizedSigninPromo(view);
        } else if (!(holder.getItemViewType() == ViewType.SYNC_PROMO)) {
            BookmarkRow row = ((BookmarkRow) holder.itemView);
            BookmarkId id = getIdByPosition(position);
            row.setBookmarkId(id, getLocationFromPosition(position));
            row.setDragHandleOnTouchListener((v, event) -> {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    mItemTouchHelper.startDrag(holder);
                }
                // this callback consumed the click action (don't activate menu)
                return true;
            });
            // Turn on the highlight for the currently highlighted bookmark.
            if (id.equals(mHighlightedBookmark)) {
                ViewHighlighter.pulseHighlight(holder.itemView, false, 1);
                clearHighlight();
            } else {
                // We need this in case we are change state during a pulse.
                ViewHighlighter.turnOffHighlight(holder.itemView);
            }
        }
    }

    @Override
    public void onViewRecycled(ViewHolder holder) {
        switch (holder.getItemViewType()) {
            case ViewType.PERSONALIZED_SIGNIN_PROMO:
                mPromoHeaderManager.detachPersonalizePromoView();
                break;
            default:
                // Other view holders don't have special recycling code.
        }
    }

    /**
     * Sets the delegate to use to handle UI actions related to this adapter.
     *
     * @param delegate A {@link BookmarkDelegate} instance to handle all backend interaction.
     */
    @Override
    public void onBookmarkDelegateInitialized(BookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);
        mDelegate.getModel().addObserver(mBookmarkModelObserver);
        mDelegate.getSelectionDelegate().addObserver(this);

        Runnable promoHeaderChangeAction = () -> {
            // If top level folders are not showing, update the header and notify.
            // Otherwise, update header without notifying; we are going to update the bookmarks
            // list, in case other top-level folders appeared because of the sync, and then
            // redraw.
            updateHeader(!topLevelFoldersShowing());
        };

        mPromoHeaderManager = new BookmarkPromoHeader(mContext, promoHeaderChangeAction);
        populateTopLevelFoldersList();

        mElements = new ArrayList<>();
        setDragStateDelegate(delegate.getDragStateDelegate());
        notifyDataSetChanged();
    }

    // BookmarkUIObserver implementations.
    @Override
    public void onDestroy() {
        recordSessionReorderInfo(); // For metrics
        mDelegate.removeUIObserver(this);
        mDelegate.getModel().removeObserver(mBookmarkModelObserver);
        mDelegate.getSelectionDelegate().removeObserver(this);
        mDelegate = null;
        mPromoHeaderManager.destroy();
        mProfileSyncService.removeSyncStateChangedListener(this);
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        assert mDelegate != null;
        clearHighlight();

        mSearchText = EMPTY_QUERY;
        mCurrentFolder = folder;

        if (!(folder.equals(mCurrentFolder))) {
            recordSessionReorderInfo();
            mCurrentFolder = folder;
        }
        enableDrag();

        if (topLevelFoldersShowing()) {
            setBookmarks(mTopLevelFolders);
        } else {
            setBookmarks(mDelegate.getModel().getChildIDs(folder, true, true));
        }
    }

    @Override
    public void onSearchStateSet() {
        recordSessionReorderInfo(); // For metrics
        clearHighlight();
        disableDrag();
        // Headers should not appear in Search mode
        // Don't need to notify because we need to redraw everything in the next step
        updateHeader(false);
        notifyDataSetChanged();
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        clearHighlight();
    }

    /**
     * Refresh the list of bookmarks within the currently visible folder.
     */
    @Override
    public void refresh() {
        // TODO(crbug.com/160194): Clean up after bookmark reordering launches.
        // Tell the RecyclerView to update its elements.
        notifyDataSetChanged();
    }

    /**
     * Synchronously searches for the given query.
     *
     * @param query The query text to search for.
     */
    @Override
    public void search(String query) {
        mSearchText = query.toString().trim();
        List<BookmarkId> result =
                mDelegate.getModel().searchBookmarks(mSearchText, MAXIMUM_NUMBER_OF_SEARCH_RESULTS);
        setBookmarks(result);
    }

    @Override
    public void moveUpOne(BookmarkId bookmarkId) {
        int pos = getPositionForBookmark(bookmarkId);
        mElements.remove(pos);
        mElements.add(pos - 1, mDelegate.getModel().getBookmarkById(bookmarkId));
        setOrder(mElements);
    }

    @Override
    public void moveDownOne(BookmarkId bookmarkId) {
        int pos = getPositionForBookmark(bookmarkId);
        mElements.remove(pos);
        mElements.add(pos + 1, mDelegate.getModel().getBookmarkById(bookmarkId));
        setOrder(mElements);
    }

    // SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        // If mDelegate is null, we will set the top level folders upon its initialization
        // (see onBookmarkDelegateInitialized method above).
        if (mDelegate == null) {
            return;
        }
        mTopLevelFolders.clear();
        populateTopLevelFoldersList();
    }

    private void recordSessionReorderInfo() {
        // Record metrics when we are exiting a folder (mCurrentFolder must not be null)
        // Cannot reorder top level folders or partner bookmarks
        if (mCurrentFolder != null && !topLevelFoldersShowing()
                && mCurrentFolder.getType() != BookmarkType.PARTNER) {
            RecordHistogram.recordCount1000Histogram(
                    "BookmarkManager.NumDraggedInSession", mDragReorderCount);
            RecordHistogram.recordCount1000Histogram(
                    "BookmarkManager.NumReorderButtonInSession", mMoveButtonCount);
            mDragReorderCount = 0;
            mMoveButtonCount = 0;
        }
    }

    /**
     * Updates mPromoHeaderType. Makes sure that the 0th index of mElements is consistent with the
     * promo header. This 0th index is null iff there is a promo header.
     *
     * @param shouldNotify True iff we should notify the RecyclerView of changes to the promoheader.
     *                     (This should be false iff we are going to make further changes to the
     *                     list of elements, as we do in setBookmarks, and true iff we are only
     *                     changing the header, as we do in the promoHeaderChangeAction runnable).
     */
    private void updateHeader(boolean shouldNotify) {
        if (mDelegate == null) return;

        boolean wasShowingPromo = hasPromoHeader();

        int currentUIState = mDelegate.getCurrentState();
        if (currentUIState == BookmarkUIState.STATE_LOADING) {
            return;
        } else if (currentUIState == BookmarkUIState.STATE_SEARCHING) {
            mPromoHeaderType = ViewType.INVALID_PROMO;
        } else {
            switch (mPromoHeaderManager.getPromoState()) {
                case BookmarkPromoHeader.PromoState.PROMO_NONE:
                    mPromoHeaderType = ViewType.INVALID_PROMO;
                    break;
                case BookmarkPromoHeader.PromoState.PROMO_SIGNIN_PERSONALIZED:
                    mPromoHeaderType = ViewType.PERSONALIZED_SIGNIN_PROMO;
                    break;
                case BookmarkPromoHeader.PromoState.PROMO_SYNC:
                    mPromoHeaderType = ViewType.SYNC_PROMO;
                    break;
                default:
                    assert false : "Unexpected value for promo state!";
            }
        }

        boolean willShowPromo = hasPromoHeader();

        if (!wasShowingPromo && willShowPromo) {
            // A null element at the 0th index represents a promo header.
            mElements.add(0, null);
            if (shouldNotify) notifyItemInserted(0);
        } else if (wasShowingPromo && willShowPromo) {
            if (shouldNotify) notifyItemChanged(0);
        } else if (wasShowingPromo && !willShowPromo) {
            mElements.remove(0);
            if (shouldNotify) notifyItemRemoved(0);
        }
    }

    private void populateTopLevelFoldersList() {
        BookmarkId desktopNodeId = mDelegate.getModel().getDesktopFolderId();
        BookmarkId mobileNodeId = mDelegate.getModel().getMobileFolderId();
        BookmarkId othersNodeId = mDelegate.getModel().getOtherFolderId();

        if (mDelegate.getModel().isFolderVisible(mobileNodeId)) {
            mTopLevelFolders.add(mobileNodeId);
        }
        if (mDelegate.getModel().isFolderVisible(desktopNodeId)) {
            mTopLevelFolders.add(desktopNodeId);
        }
        if (mDelegate.getModel().isFolderVisible(othersNodeId)) {
            mTopLevelFolders.add(othersNodeId);
        }

        // Add any top-level managed and partner bookmark folders that are children of the root
        // folder.
        List<BookmarkId> managedAndPartnerFolderIds =
                mDelegate.getModel().getTopLevelFolderIDs(true, false);
        BookmarkId rootFolder = mDelegate.getModel().getRootFolderId();
        for (BookmarkId bookmarkId : managedAndPartnerFolderIds) {
            BookmarkId parent = mDelegate.getModel().getBookmarkById(bookmarkId).getParentId();
            if (parent.equals(rootFolder)) mTopLevelFolders.add(bookmarkId);
        }
    }

    @VisibleForTesting
    public BookmarkDelegate getDelegateForTesting() {
        return mDelegate;
    }

    @Override
    protected void setOrder(List<BookmarkItem> bookmarkItems) {
        assert !topLevelFoldersShowing() : "Cannot reorder top-level folders!";
        assert mCurrentFolder.getType()
                != BookmarkType.PARTNER : "Cannot reorder partner bookmarks!";
        assert mDelegate.getCurrentState()
                == BookmarkUIState.STATE_FOLDER : "Can only reorder items from folder mode!";

        int startIndex = getBookmarkItemStartIndex();
        int endIndex = getBookmarkItemEndIndex();

        // Get the new order for the IDs.
        long[] newOrder = new long[endIndex - startIndex + 1];
        for (int i = startIndex; i <= endIndex; i++) {
            newOrder[i - startIndex] = bookmarkItems.get(i).getId().getId();
        }
        mDelegate.getModel().reorderBookmarks(mCurrentFolder, newOrder);
        if (mDragStateDelegate.getDragActive()) {
            RecordUserAction.record("MobileBookmarkManagerDragReorder");
            mDragReorderCount++;
        } else {
            mMoveButtonCount++;
        }
    }

    private int getBookmarkItemStartIndex() {
        return hasPromoHeader() ? 1 : 0;
    }

    private int getBookmarkItemEndIndex() {
        int endIndex = mElements.size() - 1;
        if (!mElements.get(endIndex).isMovable()) {
            endIndex--;
        }
        return endIndex;
    }

    private boolean isOrderable(BookmarkItem bItem) {
        return bItem != null && bItem.isMovable();
    }

    @Override
    @VisibleForTesting
    public boolean isActivelyDraggable(ViewHolder viewHolder) {
        return isPassivelyDraggable(viewHolder)
                && ((BookmarkRow) viewHolder.itemView).isItemSelected();
    }

    @Override
    @VisibleForTesting
    public boolean isPassivelyDraggable(ViewHolder viewHolder) {
        BookmarkItem bItem = getItemByHolder(viewHolder);
        return isOrderable(bItem);
    }

    @VisibleForTesting
    BookmarkId getIdByPosition(int position) {
        BookmarkItem bItem = getItemByPosition(position);
        if (bItem == null) return null;
        return bItem.getId();
    }

    private boolean hasPromoHeader() {
        return mPromoHeaderType != ViewType.INVALID_PROMO;
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
     * @return True iff the currently-open folder is the root folder
     *         (which is true iff the top-level folders are showing)
     */
    private boolean topLevelFoldersShowing() {
        return mCurrentFolder.equals(mDelegate.getModel().getRootFolderId());
    }

    @VisibleForTesting
    void simulateSignInForTests() {
        syncStateChanged();
        onFolderStateSet(mCurrentFolder);
    }

    /**
     * Scroll the bookmarks list such that bookmarkId is shown in the view, and highlight it.
     *
     * @param bookmarkId The BookmarkId of the bookmark of interest
     */
    @Override
    public void highlightBookmark(BookmarkId bookmarkId) {
        assert mHighlightedBookmark == null : "There should not already be a highlighted bookmark!";

        mRecyclerView.scrollToPosition(getPositionForBookmark(bookmarkId));
        mHighlightedBookmark = bookmarkId;
    }

    /**
     * Clears the highlighted bookmark, if there is one.
     */
    private void clearHighlight() {
        mHighlightedBookmark = null;
    }
}