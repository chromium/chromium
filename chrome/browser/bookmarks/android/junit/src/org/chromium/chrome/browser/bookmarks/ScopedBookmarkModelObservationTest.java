// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;

import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

/** Unit tests for {@link ScopedBookmarkModelObservation}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ScopedBookmarkModelObservationTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkItem mFolder;
    @Mock private BookmarkId mFolderId;
    @Mock private BookmarkItem mItem;
    @Mock private BookmarkId mItemId;
    @Mock private BookmarkModel mModel;
    @Mock private ScopedBookmarkModelObservation.Observer mObserver;
    @Mock private BookmarkItem mUnobservedFolder;
    @Mock private BookmarkId mUnobservedFolderId;
    @Mock private BookmarkItem mUnobservedItem;
    @Mock private BookmarkId mUnobservedItemId;

    private List<BookmarkItem> mFolderItems;
    private ScopedBookmarkModelObservation mObservation;
    private int mObservationId;
    private BookmarkModelObserver mUnderlyingObserver;
    private List<BookmarkItem> mUnobservedFolderItems;

    @Before
    public void setUp() {
        BookmarkModel.setInstanceForTesting(mModel);
        mFolderItems = List.of(mItem);
        mUnobservedFolderItems = List.of(mUnobservedItem);

        when(mFolder.getId()).thenReturn(mFolderId);
        when(mItem.getId()).thenReturn(mItemId);
        when(mItem.getParentId()).thenReturn(mFolderId);
        when(mModel.getBookmarkById(mFolderId)).thenReturn(mFolder);
        when(mModel.getBookmarkById(mItemId)).thenReturn(mItem);
        when(mModel.getBookmarkById(mUnobservedFolderId)).thenReturn(mUnobservedFolder);
        when(mModel.getBookmarkById(mUnobservedItemId)).thenReturn(mUnobservedItem);
        when(mModel.getChildIds(mFolderId)).thenAnswer((i) -> getIds(mFolderItems));
        when(mModel.getChildIds(mUnobservedFolderId))
                .thenAnswer((i) -> getIds(mUnobservedFolderItems));
        when(mModel.isBookmarkModelLoaded()).thenReturn(true);
        when(mUnobservedFolder.getId()).thenReturn(mUnobservedFolderId);
        when(mUnobservedItem.getId()).thenReturn(mUnobservedItemId);
        when(mUnobservedItem.getParentId()).thenReturn(mUnobservedFolderId);

        // Cache/release the underlying observer since most tests wish to interact with it.
        doAnswer(i -> mUnderlyingObserver = i.getArgument(0)).when(mModel).addObserver(any());
        doAnswer(i -> mUnderlyingObserver = null).when(mModel).removeObserver(any());

        // Most tests aren't interested in events that are propagated during construction, so reset
        // the observer when creating the observation.
        createObservation(/* resetObserver= */ true);
    }

    private void createObservation(boolean resetObserver) {
        if (mObservation != null) {
            mObservation.destroy();
            clearInvocations(mModel, mObserver);
        }

        mObservation =
                new ScopedBookmarkModelObservation(++mObservationId, mFolderId, mModel, mObserver);

        if (resetObserver) {
            reset(mObserver);
        }
    }

    private static List<BookmarkId> getIds(@NonNull List<BookmarkItem> items) {
        return items.stream().map(BookmarkItem::getId).collect(Collectors.toList());
    }

    @Test
    @SmallTest
    public void testConstructor() {
        createObservation(/* resetObserver= */ false);
        verify(mObserver).onBookmarkItemsChanged(mObservationId, mFolderItems);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkAllUserNodesRemoved() {
        mUnderlyingObserver.bookmarkAllUserNodesRemoved();
        verify(mObserver).onBookmarkItemsChanged(mObservationId, Collections.emptyList());
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkModelChanged() {
        mUnderlyingObserver.bookmarkModelChanged();
        verify(mObserver).onBookmarkItemsChanged(mObservationId, mFolderItems);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeAddedToObservedFolder() {
        final int index = 10;
        when(mModel.getChildAt(mFolderId, index)).thenReturn(mItemId);
        mUnderlyingObserver.bookmarkNodeAdded(mFolder, index, /* addedByUser= */ false);
        verify(mObserver).onBookmarkItemAdded(mObservationId, mItem, index);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeAddedToUnobservedFolder() {
        final int index = 10;
        mUnderlyingObserver.bookmarkNodeAdded(mUnobservedFolder, index, /* addedByUser= */ false);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeChangedWithinObservedFolder() {
        final int index = getIds(mFolderItems).indexOf(mItemId);
        mUnderlyingObserver.bookmarkNodeChanged(mItem);
        verify(mObserver).onBookmarkItemUpdated(mObservationId, mItem, index);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeChangedWithinUnobservedFolder() {
        mUnderlyingObserver.bookmarkNodeChanged(mUnobservedItem);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeChildrenReorderedWithinObservedFolder() {
        mUnderlyingObserver.bookmarkNodeChildrenReordered(mFolder);
        verify(mObserver).onBookmarkItemsChanged(mObservationId, mFolderItems);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeChildrenReorderedWithinUnobservedFolder() {
        mUnderlyingObserver.bookmarkNodeChildrenReordered(mUnobservedFolder);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeMovedFromObservedFolder() {
        final int index = 10;
        final int oldIndex = 27;
        mUnderlyingObserver.bookmarkNodeMoved(mFolder, oldIndex, mUnobservedFolder, index);
        verify(mObserver).onBookmarkItemRemoved(mObservationId, oldIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeMovedToObservedFolder() {
        final int index = 10;
        final int oldIndex = 27;
        when(mModel.getChildAt(mFolderId, index)).thenReturn(mItemId);
        mUnderlyingObserver.bookmarkNodeMoved(mUnobservedFolder, oldIndex, mFolder, index);
        verify(mObserver).onBookmarkItemAdded(mObservationId, mItem, index);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeMovedWithinObservedFolder() {
        final int index = 10;
        final int oldIndex = 27;
        mUnderlyingObserver.bookmarkNodeMoved(mFolder, oldIndex, mFolder, index);
        verify(mObserver).onBookmarkItemMoved(mObservationId, index, oldIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeMovedWithinUnobservedFolder() {
        final int index = 10;
        final int oldIndex = 27;
        mUnderlyingObserver.bookmarkNodeMoved(
                mUnobservedFolder, oldIndex, mUnobservedFolder, index);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeRemovedFromObservedFolder() {
        final int index = 10;
        mUnderlyingObserver.bookmarkNodeRemoved(mFolder, index, mItem);
        verify(mObserver).onBookmarkItemRemoved(mObservationId, index);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testBookmarkNodeRemovedFromUnobservedFolder() {
        final int index = 10;
        mUnderlyingObserver.bookmarkNodeRemoved(mUnobservedFolder, index, mUnobservedItem);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mObservation.destroy();
        verify(mModel).removeObserver(mObservation);
        verifyNoMoreInteractions(mObserver);
    }
}
