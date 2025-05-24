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
import java.util.Collections;
import java.util.List;

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
    public static @interface ObservationId {
        int ACCOUNT = 0;
        int LOCAL = 1;
    }

    /**
     * An observer to which events are propagated if and only if they involve top-level bookmark bar
     * items from the supplied bookmark model.
     */
    public static interface Observer extends ScopedBookmarkModelObservation.Observer {
        /**
         * Invoked when top-level bookmark bar items are added to the supplied bookmark model.
         *
         * @param observationId the ID for the observation which propagated the event.
         * @param items the top-level bookmark bar items that were added.
         * @param index the index at which the top-level bookmark bar items were added.
         */
        public void onBookmarkItemsAdded(
                @ObservationId int observationId, List<BookmarkItem> items, int index);

        /**
         * Invoked when top-level bookmark items are removed from the supplied bookmark model.
         *
         * @param observationId the ID for the observation which propagated the event.
         * @param index the index at which the top-level bookmark bar items were removed.
         * @param count the count of top-level bookmark bar items that were removed.
         */
        public void onBookmarkItemsRemoved(@ObservationId int observationId, int index, int count);

        /**
         * NOTE: {@link #onBookmarkItemsChanged()} events are never propagated and so this method
         * should not be overridden. Instead, users should implement {@link #onBookmarkItemsAdded()}
         * and {@link #onBookmarkItemsRemoved()} respectively which are propagated from items
         * changed events.
         */
        @Override
        public default void onBookmarkItemsChanged(
                @ObservationId int observationId, List<BookmarkItem> items) {}
    }

    private final ScopedBookmarkModelObservation mLocalFolderObservation;
    private final BookmarkModel mModel;
    private final Observer mObserver;

    private @Nullable ScopedBookmarkModelObservation mAccountFolderObservation;
    private int mAccountFolderSize;
    private int mLocalFolderSize;

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
        incrementSize(observationId, 1);
        mObserver.onBookmarkItemAdded(observationId, item, index + getStartIndex(observationId));
    }

    @Override
    public void onBookmarkItemMoved(int observationId, int index, int oldIndex) {
        final int startIndex = getStartIndex(observationId);
        index += startIndex;
        oldIndex += startIndex;
        mObserver.onBookmarkItemMoved(observationId, index, oldIndex);
    }

    @Override
    public void onBookmarkItemRemoved(int observationId, int index) {
        incrementSize(observationId, -1);
        mObserver.onBookmarkItemRemoved(observationId, index + getStartIndex(observationId));
    }

    @Override
    public void onBookmarkItemUpdated(int observationId, BookmarkItem item, int index) {
        mObserver.onBookmarkItemUpdated(observationId, item, index + getStartIndex(observationId));
    }

    @Override
    public void onBookmarkItemsChanged(int observationId, List<BookmarkItem> items) {
        final int index = getStartIndex(observationId);
        final int oldSize = setSize(observationId, items.size());
        if (oldSize != 0) mObserver.onBookmarkItemsRemoved(observationId, index, oldSize);
        if (!items.isEmpty()) mObserver.onBookmarkItemsAdded(observationId, items, index);
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
                return mAccountFolderSize;
        }
        throw new IllegalArgumentException("Unknown `observationId`.");
    }

    private void incrementSize(@ObservationId int observationId, int delta) {
        setSize(observationId, setSize(observationId, 0) + delta);
    }

    private int setSize(@ObservationId int observationId, int size) {
        int oldSize;
        switch (observationId) {
            case ObservationId.ACCOUNT:
                oldSize = mAccountFolderSize;
                mAccountFolderSize = size;
                return oldSize;
            case ObservationId.LOCAL:
                oldSize = mLocalFolderSize;
                mLocalFolderSize = size;
                return oldSize;
        }
        throw new IllegalArgumentException("Unknown `observationId`.");
    }
}
