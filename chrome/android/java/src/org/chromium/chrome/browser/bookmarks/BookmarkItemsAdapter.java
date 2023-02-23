// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.text.TextUtils;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkRow.Location;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableListAdapter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * BaseAdapter for {@link RecyclerView}. It manages bookmarks to list there.
 */
public class BookmarkItemsAdapter extends DragReorderableListAdapter<BookmarkListEntry>
        implements BookmarkUIObserver, SyncService.SyncStateChangedListener {
    private static final int MAXIMUM_NUMBER_OF_SEARCH_RESULTS = 500;
    private static final String EMPTY_QUERY = null;

    /** Abstraction around how to build type specific {@link View}s. */
    interface ViewFactory {
        /**
         * @param parent The parent to which the new {@link View} will be added as a child.
         * @param viewType The type of row being created.
         * @return A new View that can be added to the view hierarchy.
         */
        View buildView(@NonNull ViewGroup parent, @ViewType int viewType);
    }

    private final List<BookmarkId> mTopLevelFolders = new ArrayList<>();
    private final Profile mProfile;
    private final SyncService mSyncService;

    // There can only be one promo header at a time. This takes on one of the values:
    // ViewType.PERSONALIZED_SIGNIN_PROMO, ViewType.SYNC_PROMO, or ViewType.INVALID.
    @ViewType
    private int mPromoHeaderType = ViewType.INVALID;
    private BookmarkDelegate mDelegate;
    private BookmarkPromoHeader mPromoHeaderManager;
    private ViewFactory mViewFactory;
    private String mSearchText;
    private BookmarkId mCurrentFolder;

    // Keep track of the currently highlighted bookmark - used for "show in folder" action.
    private BookmarkId mHighlightedBookmark;

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

            if (mDelegate.getCurrentState() == BookmarkUIState.STATE_SEARCHING) {
                // We cannot rely on removing the specific list item that corresponds to the
                // removed node because the node might be a parent with children also shown
                // in the list.
                search(mSearchText);
                return;
            }

            if (node.isFolder()) {
                mDelegate.notifyStateChange(BookmarkItemsAdapter.this);
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
            mDelegate.notifyStateChange(BookmarkItemsAdapter.this);

            if (mDelegate.getCurrentState() == BookmarkUIState.STATE_SEARCHING) {
                if (!TextUtils.equals(mSearchText, EMPTY_QUERY)) {
                    search(mSearchText);
                } else {
                    mDelegate.closeSearchUI();
                }
            }
        }
    };

    BookmarkItemsAdapter(Context context, Profile profile) {
        super(context);
        mProfile = profile;
        mSyncService = SyncService.get();
        mSyncService.addSyncStateChangedListener(this);
    }

    /**
     * @return The position of the given bookmark in adapter. Will return -1 if not found.
     */
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

    private void filterForPriceTrackingCategory(List<BookmarkId> bookmarks) {
        for (int i = bookmarks.size() - 1; i >= 0; i--) {
            PowerBookmarkMeta meta = mDelegate.getModel().getPowerBookmarkMeta(bookmarks.get(i));
            if (meta == null || !meta.hasShoppingSpecifics()
                    || !meta.getShoppingSpecifics().getIsPriceTracked()) {
                bookmarks.remove(i);
                continue;
            }
        }
    }

    private void setBookmarks(List<BookmarkId> bookmarks) {
        clearHighlight();
        mElements.clear();

        // Restore the header, if it exists, then update it.
        if (hasPromoHeader()) {
            mElements.add(BookmarkListEntry.createSyncPromoHeader(mPromoHeaderType));
        }

        updateHeader(false);
        if (BookmarkId.SHOPPING_FOLDER.equals(mCurrentFolder)) {
            filterForPriceTrackingCategory(bookmarks);
        }

        for (BookmarkId bookmarkId : bookmarks) {
            BookmarkItem item = mDelegate.getModel().getBookmarkById(bookmarkId);

            mElements.add(BookmarkListEntry.createBookmarkEntry(
                    item, mDelegate.getModel().getPowerBookmarkMeta(bookmarkId)));
        }

        if (mCurrentFolder.getType() == BookmarkType.READING_LIST
                && mDelegate.getCurrentState() != BookmarkUIState.STATE_SEARCHING) {
            ReadingListSectionHeader.maybeSortAndInsertSectionHeaders(mElements, mContext);
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SHOPPING_LIST)
                && topLevelFoldersShowing()) {
            mElements.add(BookmarkListEntry.createDivider());
            mElements.add(BookmarkListEntry.createShoppingFilter());
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
        BookmarkListEntry entry = getItemByPosition(position);
        return entry.getViewType();
    }

    @Override
    public ViewHolder onCreateViewHolder(ViewGroup parent, @ViewType int viewType) {
        return new ViewHolder(mViewFactory.buildView(parent, viewType)) {};
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        if (holder.getItemViewType() == ViewType.PERSONALIZED_SIGNIN_PROMO
                || holder.getItemViewType() == ViewType.PERSONALIZED_SYNC_PROMO) {
            PersonalizedSigninPromoView view =
                    holder.itemView.findViewById(R.id.signin_promo_view_container);
            mPromoHeaderManager.setUpSyncPromoView(view);
        } else if (holder.getItemViewType() == ViewType.SECTION_HEADER) {
            bindSectionHeaderViewHolder(holder.itemView, getItemByPosition(position));
        } else if (BookmarkListEntry.isBookmarkEntry(holder.getItemViewType())) {
            BookmarkRow row = ((BookmarkRow) holder.itemView);
            BookmarkId id = getIdByPosition(position);
            row.setBookmarkId(id, getLocationFromPosition(position),
                    BookmarkId.SHOPPING_FOLDER.equals(mCurrentFolder));
            row.setDragHandleOnTouchListener((v, event) -> {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    mItemTouchHelper.startDrag(holder);
                }
                // this callback consumed the click action (don't activate menu)
                return true;
            });
            // Turn on the highlight for the currently highlighted bookmark.
            if (id.equals(mHighlightedBookmark)) {
                HighlightParams params = new HighlightParams(HighlightShape.RECTANGLE);
                params.setNumPulses(1);
                ViewHighlighter.turnOnHighlight(holder.itemView, params);
                clearHighlight();
            } else {
                // We need this in case we are change state during a pulse.
                ViewHighlighter.turnOffHighlight(holder.itemView);
            }
        } else if (holder.getItemViewType() == ViewType.SHOPPING_FILTER) {
            LinearLayout layout = ((LinearLayout) holder.itemView);
            layout.setClickable(true);
            layout.setOnClickListener((view) -> mDelegate.openFolder(BookmarkId.SHOPPING_FOLDER));
        }
    }

    private void bindSectionHeaderViewHolder(View view, BookmarkListEntry listItem) {
        TextView title = view.findViewById(R.id.title);
        title.setText(listItem.getHeaderTitle());
        if (listItem.getSectionHeaderData().topPadding > 0) {
            title.setPaddingRelative(title.getPaddingStart(),
                    listItem.getSectionHeaderData().topPadding, title.getPaddingEnd(),
                    title.getPaddingBottom());
        }
    }

    @Override
    public void onViewRecycled(ViewHolder holder) {
        switch (holder.getItemViewType()) {
            case ViewType.PERSONALIZED_SIGNIN_PROMO:
                // fall through
            case ViewType.PERSONALIZED_SYNC_PROMO:
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
     * @param viewFactory An object to create {@link View}s for each item/row.
     */
    void onBookmarkDelegateInitialized(BookmarkDelegate delegate, ViewFactory viewFactory) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);
        mDelegate.getModel().addObserver(mBookmarkModelObserver);
        mDelegate.getSelectionDelegate().addObserver(this);

        Runnable promoHeaderChangeAction = () -> {
            // Notify the view of changes to the elements list as the promo might be showing.
            updateHeader(true);
        };

        mPromoHeaderManager = new BookmarkPromoHeader(mContext, promoHeaderChangeAction);
        populateTopLevelFoldersList();

        mViewFactory = viewFactory;

        mElements = new ArrayList<>();
        setDragStateDelegate(delegate.getDragStateDelegate());
        notifyDataSetChanged();
    }

    // BookmarkUIObserver implementations.
    @Override
    public void onDestroy() {
        mDelegate.removeUIObserver(this);
        mDelegate.getModel().removeObserver(mBookmarkModelObserver);
        mDelegate.getSelectionDelegate().removeObserver(this);
        mDelegate = null;
        mPromoHeaderManager.destroy();
        mSyncService.removeSyncStateChangedListener(this);
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        assert mDelegate != null;
        clearHighlight();

        mSearchText = EMPTY_QUERY;
        mCurrentFolder = folder;
        enableDrag();

        if (topLevelFoldersShowing()) {
            setBookmarks(mTopLevelFolders);
        } else {
            setBookmarks(mDelegate.getModel().getChildIDs(folder));
        }

        if (BookmarkId.SHOPPING_FOLDER.equals(folder)) {
            mDelegate.getSelectableListLayout().setEmptyViewText(
                    R.string.tracked_products_empty_list_title);
        } else if (folder.getType() == BookmarkType.READING_LIST) {
            TrackerFactory.getTrackerForProfile(mProfile).notifyEvent(
                    EventConstants.READ_LATER_BOOKMARK_FOLDER_OPENED);
            mDelegate.getSelectableListLayout().setEmptyViewText(
                    R.string.reading_list_empty_list_title);
        } else {
            mDelegate.getSelectableListLayout().setEmptyViewText(R.string.bookmarks_folder_empty);
        }
    }

    @Override
    public void onSearchStateSet() {
        clearHighlight();
        disableDrag();
        // Headers should not appear in Search mode
        // Don't need to notify because we need to redraw everything in the next step
        updateHeader(false);
        removeSectionHeaders();
        notifyDataSetChanged();
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {
        clearHighlight();
    }

    /**
     * Refresh the list of bookmarks within the currently visible folder.
     */
    public void refresh() {
        // Tell the RecyclerView to update its elements.
        if (mElements != null) notifyDataSetChanged();
    }

    /**
     * Synchronously searches for the given query.
     *
     * @param query The query text to search for.
     */
    public void search(@Nullable String query) {
        mSearchText = query == null ? "" : query.trim();

        List<BookmarkId> bookmarks = TextUtils.isEmpty(query)
                ? Collections.emptyList()
                : mDelegate.getModel().searchBookmarks(
                        mSearchText, MAXIMUM_NUMBER_OF_SEARCH_RESULTS);
        setBookmarks(bookmarks);
    }

    /**
     * See {@link BookmarkDelegate#moveUpOne(BookmarkId)}.
     */
    void moveUpOne(BookmarkId bookmarkId) {
        int pos = getPositionForBookmark(bookmarkId);
        assert isReorderable(getItemByPosition(pos));
        mElements.remove(pos);
        mElements.add(pos - 1,
                BookmarkListEntry.createBookmarkEntry(
                        mDelegate.getModel().getBookmarkById(bookmarkId),
                        mDelegate.getModel().getPowerBookmarkMeta(bookmarkId)));
        setOrder(mElements);
    }

    /**
     * See {@link BookmarkDelegate#moveDownOne(BookmarkId)}.
     */
    void moveDownOne(BookmarkId bookmarkId) {
        int pos = getPositionForBookmark(bookmarkId);
        assert isReorderable(getItemByPosition(pos));
        mElements.remove(pos);
        mElements.add(pos + 1,
                BookmarkListEntry.createBookmarkEntry(
                        mDelegate.getModel().getBookmarkById(bookmarkId),
                        mDelegate.getModel().getPowerBookmarkMeta(bookmarkId)));
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
            mElements.add(0, BookmarkListEntry.createSyncPromoHeader(mPromoHeaderType));
            if (shouldNotify) notifyItemInserted(0);
        } else if (wasShowingPromo && willShowPromo) {
            if (shouldNotify) notifyItemChanged(0);
        } else if (wasShowingPromo && !willShowPromo) {
            mElements.remove(0);
            if (shouldNotify) notifyItemRemoved(0);
        }
    }

    /** Removes all section headers from the current list. */
    private void removeSectionHeaders() {
        for (int i = mElements.size() - 1; i >= 0; i--) {
            if (mElements.get(i).getViewType() == ViewType.SECTION_HEADER) {
                mElements.remove(i);
            }
        }
    }

    private void populateTopLevelFoldersList() {
        mTopLevelFolders.addAll(BookmarkUtils.populateTopLevelFolders(mDelegate.getModel()));
    }

    @VisibleForTesting
    public BookmarkDelegate getDelegateForTesting() {
        return mDelegate;
    }

    @Override
    protected void setOrder(List<BookmarkListEntry> listEntries) {
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
            BookmarkItem bookmarkItem = listEntries.get(i).getBookmarkItem();
            assert bookmarkItem != null;
            newOrder[i - startIndex] = bookmarkItem.getId().getId();
        }
        mDelegate.getModel().reorderBookmarks(mCurrentFolder, newOrder);
        if (mDragStateDelegate.getDragActive()) {
            RecordUserAction.record("MobileBookmarkManagerDragReorder");
        }
    }

    private int getBookmarkItemStartIndex() {
        return hasPromoHeader() ? 1 : 0;
    }

    private int getBookmarkItemEndIndex() {
        int endIndex = mElements.size() - 1;
        BookmarkItem bookmarkItem = mElements.get(endIndex).getBookmarkItem();
        if (bookmarkItem == null || !BookmarkUtils.isMovable(bookmarkItem)) {
            endIndex--;
        }
        return endIndex;
    }

    private boolean isReorderable(BookmarkListEntry entry) {
        return entry != null && entry.getBookmarkItem() != null
                && entry.getBookmarkItem().isReorderable();
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
        return isReorderable(getItemByHolder(viewHolder));
    }

    public BookmarkId getIdByPosition(int position) {
        BookmarkListEntry entry = getItemByPosition(position);
        if (entry == null || entry.getBookmarkItem() == null) return null;
        return entry.getBookmarkItem().getId();
    }

    public BookmarkPromoHeader getPromoHeaderManager() {
        return mPromoHeaderManager;
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
     * @return True iff the currently-open folder is the root folder
     *         (which is true iff the top-level folders are showing)
     */
    private boolean topLevelFoldersShowing() {
        return mCurrentFolder.equals(mDelegate.getModel().getRootFolderId());
    }

    @VisibleForTesting
    public void simulateSignInForTests() {
        syncStateChanged();
        onFolderStateSet(mCurrentFolder);
    }

    /**
     * Scroll the bookmarks list such that bookmarkId is shown in the view, and highlight it.
     *
     * @param bookmarkId The BookmarkId of the bookmark of interest
     */
    void highlightBookmark(BookmarkId bookmarkId) {
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
