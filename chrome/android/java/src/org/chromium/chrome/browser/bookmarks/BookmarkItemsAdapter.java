// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkItem;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkManager.ItemsAdapter;
import org.chromium.chrome.browser.signin.PersonalizedSigninPromoView;
import org.chromium.components.bookmarks.BookmarkId;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

// TODO(crbug.com/160194): This class will be deleted after bookmark reordering launches.
/**
 * BaseAdapter for {@link RecyclerView}. It manages bookmarks to list there.
 */
class BookmarkItemsAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder>
        implements BookmarkUIObserver, ItemsAdapter {
    /**
     * Specifies the view types that the bookmark delegate screen can contain.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ViewType.PERSONALIZED_SIGNIN_PROMO, ViewType.SYNC_PROMO, ViewType.FOLDER,
            ViewType.BOOKMARK})
    private @interface ViewType {
        int PERSONALIZED_SIGNIN_PROMO = 0;
        int SYNC_PROMO = 1;
        int FOLDER = 2;
        int BOOKMARK = 3;
    }

    private static final int MAXIMUM_NUMBER_OF_SEARCH_RESULTS = 500;
    private static final String EMPTY_QUERY = null;

    private final List<List<? extends Object>> mSections;

    // The promo header section will always contain 0 or 1 elements.
    private final List<Integer> mPromoHeaderSection = new ArrayList<>();
    private final List<BookmarkId> mFolderSection = new ArrayList<>();
    private final List<BookmarkId> mBookmarkSection = new ArrayList<>();

    private final List<BookmarkId> mTopLevelFolders = new ArrayList<>();

    private BookmarkDelegate mDelegate;
    private Context mContext;
    private BookmarkPromoHeader mPromoHeaderManager;
    private String mSearchText;
    private BookmarkId mCurrentFolder;

    private BookmarkModelObserver mBookmarkModelObserver = new BookmarkModelObserver() {
        @Override
        public void bookmarkNodeChanged(BookmarkItem node) {
            assert mDelegate != null;
            int position = getPositionForBookmark(node.getId());
            if (position >= 0) notifyItemChanged(position);
        }

        @Override
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            assert mDelegate != null;

            if (mDelegate.getCurrentState() == BookmarkUIState.STATE_SEARCHING
                    && TextUtils.equals(mSearchText, EMPTY_QUERY)) {
                mDelegate.closeSearchUI();
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
            mDelegate.notifyStateChange(BookmarkItemsAdapter.this);

            if (mDelegate.getCurrentState() == BookmarkUIState.STATE_SEARCHING
                    && !TextUtils.equals(mSearchText, EMPTY_QUERY)) {
                search(mSearchText);
            }
        }
    };

    BookmarkItemsAdapter(Context context) {
        mContext = context;

        mSections = new ArrayList<>();
        mSections.add(mPromoHeaderSection);
        mSections.add(mFolderSection);
        mSections.add(mBookmarkSection);
    }

    BookmarkId getItem(int position) {
        List<?> section = getSection(position);

        // The promo header section does contain bookmark ids.
        if (section == mPromoHeaderSection) {
            return null;
        }
        return (BookmarkId) section.get(toSectionPosition(position));
    }

    private int toSectionPosition(int globalPosition) {
        int sectionPosition = globalPosition;
        for (List<?> section : mSections) {
            if (sectionPosition < section.size()) break;
            sectionPosition -= section.size();
        }
        return sectionPosition;
    }

    private List<? extends Object> getSection(int position) {
        int i = position;
        for (List<? extends Object> section : mSections) {
            if (i < section.size()) {
                return section;
            }
            i -= section.size();
        }
        return null;
    }

    /**
     * @return The position of the given bookmark in adapter. Will return -1 if not found.
     */
    @Override
    public int getPositionForBookmark(BookmarkId bookmark) {
        assert bookmark != null;
        int position = -1;
        for (int i = 0; i < getItemCount(); i++) {
            if (bookmark.equals(getItem(i))) {
                position = i;
                break;
            }
        }
        return position;
    }

    /**
     * Set folders and bookmarks to show.
     *
     * @param folders This can be null if there is no folders to show.
     * @param bookmarks The list of bookmarks to show.
     */
    private void setBookmarks(List<BookmarkId> folders, List<BookmarkId> bookmarks) {
        if (folders == null) folders = new ArrayList<BookmarkId>();

        mFolderSection.clear();
        mFolderSection.addAll(folders);
        mBookmarkSection.clear();
        mBookmarkSection.addAll(bookmarks);

        updateHeaderAndNotify();
    }

    private void removeItem(int position) {
        List<?> section = getSection(position);
        assert section == mFolderSection || section == mBookmarkSection;
        section.remove(toSectionPosition(position));
        notifyItemRemoved(position);
    }

    // RecyclerView.Adapter implementation.

    @Override
    public int getItemCount() {
        int count = 0;
        for (List<?> section : mSections) {
            count += section.size();
        }
        return count;
    }

    @Override
    public @ViewType int getItemViewType(int position) {
        List<?> section = getSection(position);

        if (section == mPromoHeaderSection) {
            assert section.size() == 1 : "Only one element is supported in promo header section!";
            return mPromoHeaderSection.get(0);
        }

        if (section == mFolderSection) {
            return ViewType.FOLDER;
        } else if (section == mBookmarkSection) {
            return ViewType.BOOKMARK;
        }

        assert false : "Invalid position requested";
        return -1;
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
                BookmarkFolderRow folder = (BookmarkFolderRow) LayoutInflater.from(
                        parent.getContext()).inflate(R.layout.bookmark_folder_row, parent, false);
                folder.onDelegateInitialized(mDelegate);
                return new ItemViewHolder(folder);
            case ViewType.BOOKMARK:
                BookmarkItemRow item = (BookmarkItemRow) LayoutInflater.from(
                        parent.getContext()).inflate(R.layout.bookmark_item_row, parent, false);
                item.onDelegateInitialized(mDelegate);
                return new ItemViewHolder(item);
            default:
                assert false;
                return null;
        }
    }

    @Override
    public void onBindViewHolder(ViewHolder holder, int position) {
        switch (holder.getItemViewType()) {
            case ViewType.PERSONALIZED_SIGNIN_PROMO:
                PersonalizedSigninPromoView view = (PersonalizedSigninPromoView) holder.itemView;
                mPromoHeaderManager.setupPersonalizedSigninPromo(view);
                break;
            case ViewType.SYNC_PROMO:
                break;
            case ViewType.FOLDER:
                ((BookmarkRow) holder.itemView).setBookmarkId(getItem(position));
                break;
            case ViewType.BOOKMARK:
                ((BookmarkRow) holder.itemView).setBookmarkId(getItem(position));
                break;
            default:
                assert false : "View type not supported!";
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
     * @param delegate A {@link BookmarkDelegate} instance to handle all backend interaction.
     */
    @Override
    public void onBookmarkDelegateInitialized(BookmarkDelegate delegate) {
        mDelegate = delegate;
        mDelegate.addUIObserver(this);
        mDelegate.getModel().addObserver(mBookmarkModelObserver);

        Runnable promoHeaderChangeAction = () -> {
            assert mDelegate != null;
            if (mDelegate.getCurrentState() != BookmarkUIState.STATE_FOLDER) {
                return;
            }

            boolean wasShowingPromo = !mPromoHeaderSection.isEmpty();
            updateHeader();
            boolean willShowPromo = !mPromoHeaderSection.isEmpty();

            if (!wasShowingPromo && willShowPromo) {
                notifyItemInserted(0);
            } else if (wasShowingPromo && willShowPromo) {
                notifyItemChanged(0);
            } else if (wasShowingPromo && !willShowPromo) {
                notifyItemRemoved(0);
            }
        };

        mPromoHeaderManager = new BookmarkPromoHeader(mContext, promoHeaderChangeAction);
        populateTopLevelFoldersList();
        notifyDataSetChanged();
    }

    // BookmarkUIObserver implementations.
    @Override
    public void onDestroy() {
        mDelegate.removeUIObserver(this);
        mDelegate.getModel().removeObserver(mBookmarkModelObserver);
        mDelegate = null;
        mPromoHeaderManager.destroy();
    }

    @Override
    public void onFolderStateSet(BookmarkId folder) {
        assert mDelegate != null;

        mSearchText = EMPTY_QUERY;
        mCurrentFolder = folder;

        if (folder.equals(mDelegate.getModel().getRootFolderId())) {
            setBookmarks(mTopLevelFolders, new ArrayList<BookmarkId>());
        } else {
            // Get folders and bookmarks separately.
            setBookmarks(mDelegate.getModel().getChildIDs(folder, true, false),
                    mDelegate.getModel().getChildIDs(folder, false, true));
        }
    }

    @Override
    public void onSearchStateSet() {
        updateHeaderAndNotify();
    }

    @Override
    public void onSelectionStateChange(List<BookmarkId> selectedBookmarks) {}

    /**
     * Refresh the list of bookmarks within the currently visible folder.
     */
    @Override
    public void refresh() {
        if (mCurrentFolder == null) return;
        onFolderStateSet(mCurrentFolder);
    }

    /**
     * Synchronously searches for the given query.
     * @param query The query text to search for.
     */
    @Override
    public void search(String query) {
        mSearchText = query.toString().trim();
        List<BookmarkId> results =
                mDelegate.getModel().searchBookmarks(mSearchText, MAXIMUM_NUMBER_OF_SEARCH_RESULTS);
        setBookmarks(null, results);
    }

    @Override
    public void moveUpOne(BookmarkId bookmarkId) {
        throw new RuntimeException("Cannot reorder bookmarks when bookmark reordering flag is off");
    }

    @Override
    public void moveDownOne(BookmarkId bookmarkId) {
        throw new RuntimeException("Cannot reorder bookmarks when bookmark reordering flag is off");
    }

    private static class ItemViewHolder extends RecyclerView.ViewHolder {
        private ItemViewHolder(View view) {
            super(view);
        }
    }

    private void updateHeaderAndNotify() {
        updateHeader();
        notifyDataSetChanged();
    }

    private void updateHeader() {
        if (mDelegate == null) return;

        int currentUIState = mDelegate.getCurrentState();
        if (currentUIState == BookmarkUIState.STATE_LOADING) return;

        mPromoHeaderSection.clear();

        if (currentUIState == BookmarkUIState.STATE_SEARCHING) return;

        assert currentUIState == BookmarkUIState.STATE_FOLDER : "Unexpected UI state";

        switch (mPromoHeaderManager.getPromoState()) {
            case BookmarkPromoHeader.PromoState.PROMO_NONE:
                return;
            case BookmarkPromoHeader.PromoState.PROMO_SIGNIN_PERSONALIZED:
                mPromoHeaderSection.add(ViewType.PERSONALIZED_SIGNIN_PROMO);
                return;
            case BookmarkPromoHeader.PromoState.PROMO_SYNC:
                mPromoHeaderSection.add(ViewType.SYNC_PROMO);
                return;
            default:
                assert false : "Unexpected value for promo state!";
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

    /**
     * Scroll the bookmarks list such that bookmarkId is shown in the view, and highlight it.
     *
     * @param bookmarkId The BookmarkId of the bookmark of interest.
     */
    @Override
    public void highlightBookmark(BookmarkId bookmarkId) {
        // This method is currently implemented for the ReorderBookmarkItemsAdapter only.
    }
}
