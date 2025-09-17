// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * A scoped object which observes and propagates events for the supplied bookmark model if and only
 * if they involve the supplied bookmark folder's direct descendants.
 */
@NullMarked
public class ScopedBookmarkModelObservation extends BookmarkModelObserver {

    public static final int NO_ID = -1;

    /**
     * An observer to which events are propagated if and only if they involve the supplied bookmark
     * folder's direct descendants.
     */
    public interface Observer {
        /**
         * Invoked when a direct descendant of the supplied bookmark folder is added.
         *
         * @param observationId the ID for the observation that propagated the event.
         * @param item the direct descendant that was added.
         * @param index the index at which the direct descendant was added.
         */
        void onBookmarkItemAdded(int observationId, BookmarkItem item, int index);

        /**
         * Invoked when a direct descendant of the supplied bookmark folder is moved.
         *
         * @param observationId the ID for the observation that propagated the event.
         * @param index the index to which the direct descendant was moved.
         * @param oldIndex the index from which the direct descendant was moved.
         */
        void onBookmarkItemMoved(int observationId, int index, int oldIndex);

        /**
         * Invoked when a direct descendant of the supplied bookmark folder is removed.
         *
         * @param observationId the ID for the observation that propagated the event.
         * @param index the index at which the direct descendant was removed.
         */
        void onBookmarkItemRemoved(int observationId, int index);

        /**
         * Invoked when a direct descendant of the supplied bookmark folder is updated.
         *
         * @param observationId the ID for the observation that propagated the event.
         * @param item the direct descendant that was updated.
         * @param index the index at which the direct descendant was updated.
         */
        void onBookmarkItemUpdated(int observationId, BookmarkItem item, int index);

        /**
         * Invoked when the direct descendants of the supplied bookmark folder have changed. Note
         * that this event is only propagated when a more specific event cannot be, e.g. when the
         * bookmark model has undergone extensive batched changes.
         *
         * @param observationId the ID for the observation that propagated the event.
         * @param items the direct descendants that changed.
         */
        void onBookmarkItemsChanged(int observationId, List<BookmarkItem> items);
    }

    private final int mId;
    private final BookmarkId mFolderId;
    private final BookmarkModel mModel;
    private final Observer mObserver;

    /**
     * Constructor.
     *
     * @param folderId the ID for the folder to observe.
     * @param model the model to observe.
     * @param observer the observer to which events are propagated.
     */
    public ScopedBookmarkModelObservation(
            BookmarkId folderId, BookmarkModel model, Observer observer) {
        this(NO_ID, folderId, model, observer);
    }

    /**
     * Constructor.
     *
     * @param id the ID for this observation.
     * @param folderId the ID for the folder to observe.
     * @param model the model to observe.
     * @param observer the observer to which events are propagated.
     */
    public ScopedBookmarkModelObservation(
            int id, BookmarkId folderId, BookmarkModel model, Observer observer) {
        assert model.isBookmarkModelLoaded();

        mId = id;
        mFolderId = folderId;
        mModel = model;
        mObserver = observer;

        mModel.addObserver(this);
        bookmarkModelChanged();
    }

    /** Destroys the observation. */
    public void destroy() {
        mModel.removeObserver(this);
    }

    @Override
    public void bookmarkAllUserNodesRemoved() {
        mObserver.onBookmarkItemsChanged(mId, Collections.emptyList());
    }

    @Override
    public void bookmarkModelChanged() {
        final List<BookmarkItem> items = new ArrayList<>();
        for (final BookmarkId itemId : mModel.getChildIds(mFolderId)) {
            final BookmarkItem item = mModel.getBookmarkById(itemId);
            if (item != null) items.add(item);
        }
        mObserver.onBookmarkItemsChanged(mId, items);
    }

    @Override
    public void bookmarkNodeAdded(BookmarkItem parent, int index, boolean addedByUser) {
        if (Objects.equals(mFolderId, parent.getId())) {
            mObserver.onBookmarkItemAdded(mId, getBookmarkByIndex(index), index);
        }
    }

    @Override
    public void bookmarkNodeChanged(BookmarkItem node) {
        if (Objects.equals(mFolderId, node.getParentId())) {
            final int index = mModel.getChildIds(mFolderId).indexOf(node.getId());
            mObserver.onBookmarkItemUpdated(mId, node, index);
        }
    }

    @Override
    public void bookmarkNodeChildrenReordered(BookmarkItem node) {
        if (Objects.equals(mFolderId, node.getId())) {
            bookmarkModelChanged();
        }
    }

    @Override
    public void bookmarkNodeMoved(
            BookmarkItem oldParent, int oldIndex, BookmarkItem parent, int index) {
        final boolean isMoveFromFolder = Objects.equals(mFolderId, oldParent.getId());
        final boolean isMoveToFolder = Objects.equals(mFolderId, parent.getId());
        final boolean isMoveWithinFolder = isMoveFromFolder && isMoveToFolder;

        if (isMoveWithinFolder) {
            mObserver.onBookmarkItemMoved(mId, index, oldIndex);
        } else if (isMoveFromFolder) {
            mObserver.onBookmarkItemRemoved(mId, oldIndex);
        } else if (isMoveToFolder) {
            mObserver.onBookmarkItemAdded(mId, getBookmarkByIndex(index), index);
        }
    }

    @Override
    public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node) {
        if (Objects.equals(mFolderId, parent.getId())) {
            mObserver.onBookmarkItemRemoved(mId, oldIndex);
        }
    }

    private BookmarkItem getBookmarkByIndex(int index) {
        final BookmarkId id = mModel.getChildAt(mFolderId, index);
        assert id != null;

        final BookmarkItem item = mModel.getBookmarkById(id);
        assert item != null;
        return item;
    }
}
