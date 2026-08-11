// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.ScopedBookmarkModelObservation;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;

/**
 * A provider which observes and propagates events for the supplied bookmark model if and only if
 * they involve top-level bookmark bar items. Internally, it concatenates bookmark items from both
 * account and local sources.
 */
@NullMarked
class BookmarkBarItemsProvider extends BookmarkModelObserver
        implements ScopedBookmarkModelObservation.Observer {

    /** Enumeration of IDs for top-level bookmark bar item observations. */
    @IntDef({ObservationId.ACCOUNT, ObservationId.LOCAL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ObservationId {
        int ACCOUNT = 0;
        int LOCAL = 1;
    }

    /**
     * An observer to which events are propagated if and only if they involve top-level bookmark bar
     * items from the supplied bookmark model.
     */
    public interface Observer extends ScopedBookmarkModelObservation.Observer {
        /**
         * Invoked when top-level bookmark bar items are added to the supplied bookmark model.
         *
         * @param observationId the ID for the observation which propagated the event.
         * @param items the top-level bookmark bar items that were added.
         * @param index the index at which the top-level bookmark bar items were added.
         */
        void onBookmarkItemsAdded(
                @ObservationId int observationId, List<BookmarkItem> items, int index);

        /**
         * Invoked when top-level bookmark items are removed from the supplied bookmark model.
         *
         * @param observationId the ID for the observation which propagated the event.
         * @param index the index at which the top-level bookmark bar items were removed.
         * @param count the count of top-level bookmark bar items that were removed.
         */
        void onBookmarkItemsRemoved(@ObservationId int observationId, int index, int count);

        /**
         * NOTE: {@link #onBookmarkItemsChanged()} events are never propagated and so this method
         * should not be overridden. Instead, users should implement {@link #onBookmarkItemsAdded()}
         * and {@link #onBookmarkItemsRemoved()} respectively which are propagated from items
         * changed events.
         */
        @Override
        default void onBookmarkItemsChanged(
                @ObservationId int observationId, List<BookmarkItem> items) {}
    }

    private final ScopedBookmarkModelObservation mLocalFolderObservation;
    private final BookmarkModel mModel;
    private final Observer mObserver;

    private @Nullable ScopedBookmarkModelObservation mAccountFolderObservation;
    private List<BookmarkItem> mAccountFolderItems = Collections.emptyList();
    private List<BookmarkItem> mLocalFolderItems = Collections.emptyList();

    /**
     * Constructor.
     *
     * @param model the model to observe.
     * @param observer the observer to which events are propagated.
     */
    public BookmarkBarItemsProvider(BookmarkModel model, Observer observer) {
        assert model.isBookmarkModelLoaded();

        mObserver = observer;

        mModel = model;
        mModel.addObserver(this);
        bookmarkModelChanged();

        // NOTE: Local folder existence is guaranteed while account folder existence is dependent on
        // whether the user is signed in to the browser and may change dynamically during a session.
        mLocalFolderObservation =
                createObservation(
                        ObservationId.LOCAL,
                        assumeNonNull(mModel.getDesktopFolderId()),
                        mModel,
                        /* observer= */ this);
    }

    public void destroy() {
        if (mAccountFolderObservation != null) {
            mAccountFolderObservation.destroy();
            mAccountFolderObservation = null;
        }

        mLocalFolderObservation.destroy();
        mModel.removeObserver(this);
    }

    @Override
    public void bookmarkModelChanged() {
        // NOTE: Account folder existence is dependent on whether the user is signed in to the
        // browser and may change dynamically during a session.
        final @Nullable BookmarkId accountFolderId = mModel.getAccountDesktopFolderId();
        if (accountFolderId != null && mAccountFolderObservation == null) {
            mAccountFolderObservation =
                    createObservation(
                            ObservationId.ACCOUNT, accountFolderId, mModel, /* observer= */ this);
        } else if (accountFolderId == null && mAccountFolderObservation != null) {
            onBookmarkItemsChanged(ObservationId.ACCOUNT, Collections.emptyList());
            mAccountFolderObservation.destroy();
            mAccountFolderObservation = null;
        }
    }

    @Override
    public void onBookmarkItemAdded(int observationId, BookmarkItem item, int index) {
        List<BookmarkItem> items = new ArrayList<>(getItems(observationId));
        if (index >= 0 && index <= items.size()) {
            items.add(index, item);
            setItems(observationId, items);
        }
        mObserver.onBookmarkItemAdded(observationId, item, index + getStartIndex(observationId));
    }

    @Override
    public void onBookmarkItemMoved(int observationId, int index, int oldIndex) {
        List<BookmarkItem> items = new ArrayList<>(getItems(observationId));
        if (oldIndex >= 0 && oldIndex < items.size()) {
            BookmarkItem item = items.remove(oldIndex);
            if (index >= 0 && index <= items.size()) {
                items.add(index, item);
            }
            setItems(observationId, items);
        }
        final int startIndex = getStartIndex(observationId);
        index += startIndex;
        oldIndex += startIndex;
        mObserver.onBookmarkItemMoved(observationId, index, oldIndex);
    }

    @Override
    public void onBookmarkItemRemoved(int observationId, int index) {
        List<BookmarkItem> items = new ArrayList<>(getItems(observationId));
        if (index >= 0 && index < items.size()) {
            items.remove(index);
            setItems(observationId, items);
        }
        mObserver.onBookmarkItemRemoved(observationId, index + getStartIndex(observationId));
    }

    @Override
    public void onBookmarkItemUpdated(int observationId, BookmarkItem item, int index) {
        List<BookmarkItem> items = new ArrayList<>(getItems(observationId));
        if (index >= 0 && index < items.size()) {
            items.set(index, item);
            setItems(observationId, items);
        }
        mObserver.onBookmarkItemUpdated(observationId, item, index + getStartIndex(observationId));
    }

    @Override
    public void onBookmarkItemsChanged(int observationId, List<BookmarkItem> items) {
        final List<BookmarkItem> currentItems = getItems(observationId);
        if (Objects.equals(currentItems, items)) {
            return;
        }

        final int startIndex = getStartIndex(observationId);
        final int oldSize = currentItems.size();
        final int newSize = items.size();

        // Check for item reorderings and in-place updates (same set of item IDs).
        if (oldSize == newSize && hasSameItemIds(currentItems, items)) {
            List<BookmarkItem> workingList = new ArrayList<>(currentItems);
            for (int i = 0; i < newSize; i++) {
                BookmarkId targetId = items.get(i).getId();
                int currentIndex = -1;
                for (int j = i; j < newSize; j++) {
                    if (Objects.equals(workingList.get(j).getId(), targetId)) {
                        currentIndex = j;
                        break;
                    }
                }
                if (currentIndex != i && currentIndex != -1) {
                    BookmarkItem movedItem = workingList.remove(currentIndex);
                    workingList.add(i, movedItem);
                    mObserver.onBookmarkItemMoved(
                            observationId, i + startIndex, currentIndex + startIndex);
                }
            }

            for (int i = 0; i < newSize; i++) {
                BookmarkItem oldItem = workingList.get(i);
                BookmarkItem newItem = items.get(i);
                if (!Objects.equals(oldItem, newItem)) {
                    mObserver.onBookmarkItemUpdated(observationId, newItem, i + startIndex);
                }
            }
            setItems(observationId, new ArrayList<>(items));
            return;
        }

        // Check for a single item insertion or deletion.
        if (Math.abs(newSize - oldSize) == 1) {
            boolean isInsertion = newSize > oldSize;
            int changedIndex = findSingleChangeIndex(currentItems, items, isInsertion);
            if (changedIndex != -1) {
                setItems(observationId, new ArrayList<>(items));
                if (isInsertion) {
                    mObserver.onBookmarkItemAdded(
                            observationId, items.get(changedIndex), changedIndex + startIndex);
                } else {
                    mObserver.onBookmarkItemRemoved(observationId, changedIndex + startIndex);
                }
                return;
            }
        }

        setItems(observationId, new ArrayList<>(items));
        if (oldSize != 0) mObserver.onBookmarkItemsRemoved(observationId, startIndex, oldSize);
        if (!items.isEmpty()) mObserver.onBookmarkItemsAdded(observationId, items, startIndex);
    }

    /**
     * Invoked to create a scoped bookmark model observation. Note that this method is only
     * `protected` so as to be overridden in testing.
     *
     * @param observationId the ID for the observation.
     * @param folderId the ID for the folder to observe.
     * @param model the model to observe.
     * @param observer the observer to which events are propagated.
     * @return the created observation.
     */
    protected ScopedBookmarkModelObservation createObservation(
            @ObservationId int observationId,
            BookmarkId folderId,
            BookmarkModel model,
            ScopedBookmarkModelObservation.Observer observer) {
        return new ScopedBookmarkModelObservation(observationId, folderId, model, observer);
    }

    private int getStartIndex(@ObservationId int observationId) {
        switch (observationId) {
            case ObservationId.ACCOUNT:
                return 0;
            case ObservationId.LOCAL:
                // NOTE: Local folder items are appended to account folder items.
                return mAccountFolderItems.size();
        }
        throw new IllegalArgumentException("Unknown `observationId`.");
    }

    private List<BookmarkItem> getItems(@ObservationId int observationId) {
        switch (observationId) {
            case ObservationId.ACCOUNT:
                return mAccountFolderItems;
            case ObservationId.LOCAL:
                return mLocalFolderItems;
        }
        throw new IllegalArgumentException("Unknown `observationId`.");
    }

    private void setItems(@ObservationId int observationId, List<BookmarkItem> items) {
        switch (observationId) {
            case ObservationId.ACCOUNT:
                mAccountFolderItems = items;
                return;
            case ObservationId.LOCAL:
                mLocalFolderItems = items;
                return;
        }
        throw new IllegalArgumentException("Unknown `observationId`.");
    }

    private static boolean hasSameItemIds(List<BookmarkItem> list1, List<BookmarkItem> list2) {
        if (list1.size() != list2.size()) return false;
        Set<BookmarkId> ids1 = new HashSet<>();
        for (BookmarkItem item : list1) {
            if (item == null || item.getId() == null) return false;
            ids1.add(item.getId());
        }
        for (BookmarkItem item : list2) {
            if (item == null || item.getId() == null || !ids1.contains(item.getId())) {
                return false;
            }
        }
        return true;
    }

    private static int findSingleChangeIndex(
            List<BookmarkItem> currentItems, List<BookmarkItem> items, boolean isInsertion) {
        List<BookmarkItem> shorterList = isInsertion ? currentItems : items;
        List<BookmarkItem> longerList = isInsertion ? items : currentItems;
        int shorterSize = shorterList.size();
        int insertIndex = -1;
        int i = 0;
        int j = 0;
        while (i < shorterSize && j < longerList.size()) {
            if (Objects.equals(shorterList.get(i), longerList.get(j))) {
                i++;
                j++;
            } else if (insertIndex == -1) {
                insertIndex = j;
                j++;
            } else {
                return -1;
            }
        }
        if (insertIndex == -1 && j == shorterSize) {
            insertIndex = shorterSize;
        }
        return insertIndex;
    }
}
