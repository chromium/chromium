// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.bookmarks.bar.BookmarkBarItemsProvider.ObservationId.ACCOUNT;
import static org.chromium.chrome.browser.bookmarks.bar.BookmarkBarItemsProvider.ObservationId.LOCAL;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.ScopedBookmarkModelObservation;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarItemsProvider.ObservationId;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;

import java.util.Collections;
import java.util.List;

/** Unit tests for {@link BookmarkBarItemsProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarItemsProviderTest {

    /** A factory for creating {@link ScopedBookmarkModelObservation} instances. */
    @FunctionalInterface
    private interface ScopedBookmarkModelObservationFactory {
        /**
         * Invoked to create a scoped bookmark model observation.
         *
         * @param observationId the ID for the observation.
         * @param folderId the ID for the folder to observe.
         * @param model the model to observe.
         * @param observer the observer to which events are propagated.
         * @return the created observation.
         */
        ScopedBookmarkModelObservation create(
                @ObservationId int observationId,
                BookmarkId folderId,
                BookmarkModel model,
                ScopedBookmarkModelObservation.Observer observer);
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BookmarkId mAccountFolderId;
    @Mock private BookmarkItem mAccountFolderItem1;
    @Mock private BookmarkItem mAccountFolderItem2;
    @Mock private ScopedBookmarkModelObservation mAccountFolderObservation;
    @Mock private BookmarkId mLocalFolderId;
    @Mock private BookmarkItem mLocalFolderItem1;
    @Mock private BookmarkItem mLocalFolderItem2;
    @Mock private ScopedBookmarkModelObservation mLocalFolderObservation;
    @Mock private ScopedBookmarkModelObservation.Observer mLocalFolderObserver;
    @Mock private BookmarkModel mModel;
    @Mock private BookmarkItem mNewItem1;
    @Mock private BookmarkItem mNewItem2;
    @Mock private BookmarkBarItemsProvider.Observer mObserver;
    @Mock private ScopedBookmarkModelObservationFactory mObservationFactory;

    private ScopedBookmarkModelObservation.Observer mAccountFolderObserver;
    private List<BookmarkItem> mAccountFolderItems;
    private List<BookmarkItem> mLocalFolderItems;
    private BookmarkModelObserver mModelObserver;
    private BookmarkBarItemsProvider mProvider;

    @Before
    @SuppressWarnings("DirectInvocationOnMock")
    public void setUp() {
        mAccountFolderItems = List.of(mAccountFolderItem1, mAccountFolderItem2);
        mLocalFolderItems = List.of(mLocalFolderItem1, mLocalFolderItem2);

        when(mModel.getAccountDesktopFolderId()).thenReturn(mAccountFolderId);
        when(mModel.getDesktopFolderId()).thenReturn(mLocalFolderId);
        when(mModel.isBookmarkModelLoaded()).thenReturn(true);

        // Cache/release the underlying model observer.
        doAnswer(i -> mModelObserver = i.getArgument(0)).when(mModel).addObserver(any());
        doAnswer(i -> mModelObserver = null).when(mModel).removeObserver(any());

        // Cache the underlying account folder observation observer.
        when(mObservationFactory.create(eq(ACCOUNT), eq(mAccountFolderId), eq(mModel), any()))
                .thenAnswer(
                        invocation -> {
                            // NOTE: In prod, `#onBookmarkItemsChanged()` fires during construction.
                            mAccountFolderObserver = invocation.getArgument(3);
                            mAccountFolderObserver.onBookmarkItemsChanged(
                                    ACCOUNT, mAccountFolderItems);
                            return mAccountFolderObservation;
                        });

        // Cache the underlying local folder observation observer.
        when(mObservationFactory.create(eq(LOCAL), eq(mLocalFolderId), eq(mModel), any()))
                .thenAnswer(
                        invocation -> {
                            // NOTE: In prod, `#onBookmarkItemsChanged()` fires during construction.
                            mLocalFolderObserver = invocation.getArgument(3);
                            mLocalFolderObserver.onBookmarkItemsChanged(LOCAL, mLocalFolderItems);
                            return mLocalFolderObservation;
                        });

        // Most tests aren't interested in events that are propagated during construction, so reset
        // the observer when creating the provider.
        createProvider(/* resetObserver= */ true);
    }

    private void createProvider(boolean resetObserver) {
        if (mProvider != null) {
            mProvider.destroy();
            clearInvocations(mAccountFolderObservation, mLocalFolderObservation, mObserver);
        }

        mProvider =
                new BookmarkBarItemsProvider(mModel, mObserver) {
                    @Override
                    @SuppressWarnings("DirectInvocationOnMock")
                    protected ScopedBookmarkModelObservation createObservation(
                            @ObservationId int observationId,
                            BookmarkId folderId,
                            BookmarkModel model,
                            ScopedBookmarkModelObservation.Observer observer) {
                        // NOTE: Use a mock factory to facilitate mocking of observations.
                        return mObservationFactory.create(observationId, folderId, model, observer);
                    }
                };

        if (resetObserver) {
            reset(mObserver);
        }
    }

    @Test
    @SmallTest
    public void testConstructor() {
        final int startIndex = mAccountFolderItems.size();
        createProvider(/* resetObserver= */ false);
        verify(mObserver).onBookmarkItemsAdded(ACCOUNT, mAccountFolderItems, 0);
        verify(mObserver).onBookmarkItemsAdded(LOCAL, mLocalFolderItems, startIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testConstructorWithEmptyAccountFolder() {
        mAccountFolderItems = Collections.emptyList();
        createProvider(/* resetObserver= */ false);
        verify(mObserver).onBookmarkItemsAdded(LOCAL, mLocalFolderItems, 0);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testConstructorWithEmptyAccountAndLocalFolders() {
        mAccountFolderItems = Collections.emptyList();
        mLocalFolderItems = Collections.emptyList();
        createProvider(/* resetObserver= */ false);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testConstructorWithEmptyLocalFolder() {
        mLocalFolderItems = Collections.emptyList();
        createProvider(/* resetObserver= */ false);
        verify(mObserver).onBookmarkItemsAdded(ACCOUNT, mAccountFolderItems, 0);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testConstructorWithNullAccountFolder() {
        when(mModel.getAccountDesktopFolderId()).thenReturn(null);
        createProvider(/* resetObserver= */ false);
        verify(mObserver).onBookmarkItemsAdded(LOCAL, mLocalFolderItems, 0);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        final var modelObserver = mModelObserver;
        mProvider.destroy();
        verify(mAccountFolderObservation).destroy();
        verify(mLocalFolderObservation).destroy();
        verify(mModel).removeObserver(modelObserver);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testDestroyWithNullAccountFolder() {
        when(mModel.getAccountDesktopFolderId()).thenReturn(null);
        createProvider(/* resetObserver= */ true);

        final var modelObserver = mModelObserver;
        mProvider.destroy();
        verify(mLocalFolderObservation).destroy();
        verify(mModel).removeObserver(modelObserver);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkModelChanged() {
        mModelObserver.bookmarkModelChanged();
        verifyNoMoreInteractions(mAccountFolderObservation);
        verifyNoMoreInteractions(mLocalFolderObservation);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkModelChangedToAddAccountFolder() {
        when(mModel.getAccountDesktopFolderId()).thenReturn(null);
        createProvider(/* resetObserver= */ true);

        // Simulate creation of the account folder.
        when(mModel.getAccountDesktopFolderId()).thenReturn(mAccountFolderId);
        mModelObserver.bookmarkModelChanged();

        verifyNoMoreInteractions(mAccountFolderObservation);
        verifyNoMoreInteractions(mLocalFolderObservation);
        verify(mObserver).onBookmarkItemsAdded(ACCOUNT, mAccountFolderItems, 0);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkModelChangedToRemoveAccountFolder() {
        // Simulate removal of the account folder.
        when(mModel.getAccountDesktopFolderId()).thenReturn(null);
        mModelObserver.bookmarkModelChanged();

        verify(mAccountFolderObservation).destroy();
        verifyNoMoreInteractions(mAccountFolderObservation);
        verifyNoMoreInteractions(mLocalFolderObservation);
        verify(mObserver).onBookmarkItemsRemoved(ACCOUNT, 0, mAccountFolderItems.size());
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkItemAddedToAccountFolder() {
        final int index = 10;
        mAccountFolderObserver.onBookmarkItemAdded(ACCOUNT, mNewItem1, index);
        verify(mObserver).onBookmarkItemAdded(ACCOUNT, mNewItem1, index);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemAddedToLocalFolder() {
        final int index = 10;
        final int startIndex = mAccountFolderItems.size();
        mLocalFolderObserver.onBookmarkItemAdded(LOCAL, mNewItem1, index);
        verify(mObserver).onBookmarkItemAdded(LOCAL, mNewItem1, index + startIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkItemMovedWithinAccountFolder() {
        final int index = 10;
        final int oldIndex = 27;
        mAccountFolderObserver.onBookmarkItemMoved(ACCOUNT, index, oldIndex);
        verify(mObserver).onBookmarkItemMoved(ACCOUNT, index, oldIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemMovedWithinLocalFolder() {
        final int index = 10;
        final int oldIndex = 27;
        final int startIndex = mAccountFolderItems.size();
        mLocalFolderObserver.onBookmarkItemMoved(LOCAL, index, oldIndex);
        verify(mObserver).onBookmarkItemMoved(LOCAL, index + startIndex, oldIndex + startIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkItemRemovedFromAccountFolder() {
        final int index = 10;
        mAccountFolderObserver.onBookmarkItemRemoved(ACCOUNT, index);
        verify(mObserver).onBookmarkItemRemoved(ACCOUNT, index);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemRemovedFromLocalFolder() {
        final int index = 10;
        final int startIndex = mAccountFolderItems.size();
        mLocalFolderObserver.onBookmarkItemRemoved(LOCAL, index);
        verify(mObserver).onBookmarkItemRemoved(LOCAL, index + startIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkItemUpdatedWithinAccountFolder() {
        final int index = 10;
        mAccountFolderObserver.onBookmarkItemUpdated(ACCOUNT, mAccountFolderItem1, 10);
        verify(mObserver).onBookmarkItemUpdated(ACCOUNT, mAccountFolderItem1, index);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemUpdatedWithinLocalFolder() {
        final int index = 10;
        final int startIndex = mAccountFolderItems.size();
        mLocalFolderObserver.onBookmarkItemUpdated(LOCAL, mLocalFolderItem1, index);
        verify(mObserver).onBookmarkItemUpdated(LOCAL, mLocalFolderItem1, index + startIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkItemsChangedToEmptyAccountFolder() {
        mAccountFolderObserver.onBookmarkItemsChanged(ACCOUNT, Collections.emptyList());
        verify(mObserver).onBookmarkItemsRemoved(ACCOUNT, 0, mAccountFolderItems.size());
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedToEmptyLocalFolder() {
        final int startIndex = mAccountFolderItems.size();
        mLocalFolderObserver.onBookmarkItemsChanged(LOCAL, Collections.emptyList());
        verify(mObserver).onBookmarkItemsRemoved(LOCAL, startIndex, mLocalFolderItems.size());
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkItemsChangedToPopulateAccountFolder() {
        final var accountFolderItems = mAccountFolderItems;
        mAccountFolderItems = Collections.emptyList();
        createProvider(/* resetObserver= */ true);

        mAccountFolderItems = accountFolderItems;
        mAccountFolderObserver.onBookmarkItemsChanged(ACCOUNT, mAccountFolderItems);
        verify(mObserver).onBookmarkItemsAdded(ACCOUNT, mAccountFolderItems, 0);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedToPopulateLocalFolder() {
        final int startIndex = mAccountFolderItems.size();
        final var localFolderItems = mLocalFolderItems;
        mLocalFolderItems = Collections.emptyList();
        createProvider(/* resetObserver= */ true);

        mLocalFolderItems = localFolderItems;
        mLocalFolderObserver.onBookmarkItemsChanged(LOCAL, mLocalFolderItems);
        verify(mObserver).onBookmarkItemsAdded(LOCAL, mLocalFolderItems, startIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    public void testOnBookmarkItemsChangedToSwapAccountFolder() {
        final int oldSize = mAccountFolderItems.size();
        mAccountFolderItems = List.of(mNewItem1, mNewItem2);
        mAccountFolderObserver.onBookmarkItemsChanged(ACCOUNT, mAccountFolderItems);
        verify(mObserver).onBookmarkItemsRemoved(ACCOUNT, 0, oldSize);
        verify(mObserver).onBookmarkItemsAdded(ACCOUNT, mAccountFolderItems, 0);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedToSwapLocalFolder() {
        final int startIndex = mAccountFolderItems.size();
        final int oldSize = mLocalFolderItems.size();
        mLocalFolderItems = List.of(mNewItem1, mNewItem2);
        mLocalFolderObserver.onBookmarkItemsChanged(LOCAL, mLocalFolderItems);
        verify(mObserver).onBookmarkItemsRemoved(LOCAL, startIndex, oldSize);
        verify(mObserver).onBookmarkItemsAdded(LOCAL, mLocalFolderItems, startIndex);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedWithIdenticalAccountFolderItems() {
        mAccountFolderObserver.onBookmarkItemsChanged(ACCOUNT, mAccountFolderItems);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedWithIdenticalLocalFolderItems() {
        mLocalFolderObserver.onBookmarkItemsChanged(LOCAL, mLocalFolderItems);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedToUpdateSingleItemInAccountFolder() {
        BookmarkId id1 = new BookmarkId(1, 0);
        BookmarkId id2 = new BookmarkId(2, 0);
        when(mAccountFolderItem1.getId()).thenReturn(id1);
        when(mAccountFolderItem2.getId()).thenReturn(id2);

        BookmarkItem updatedItem2 = org.mockito.Mockito.mock(BookmarkItem.class);
        when(updatedItem2.getId()).thenReturn(id2);
        when(updatedItem2.getTitle()).thenReturn("New Title");

        mAccountFolderObserver.onBookmarkItemsChanged(
                ACCOUNT, List.of(mAccountFolderItem1, updatedItem2));
        verify(mObserver).onBookmarkItemUpdated(ACCOUNT, updatedItem2, 1);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedToInsertSingleItemInAccountFolder() {
        BookmarkId id1 = new BookmarkId(1, 0);
        BookmarkId id2 = new BookmarkId(2, 0);
        BookmarkId id3 = new BookmarkId(3, 0);
        when(mAccountFolderItem1.getId()).thenReturn(id1);
        when(mAccountFolderItem2.getId()).thenReturn(id2);
        when(mNewItem1.getId()).thenReturn(id3);

        mAccountFolderObserver.onBookmarkItemsChanged(
                ACCOUNT, List.of(mAccountFolderItem1, mNewItem1, mAccountFolderItem2));
        verify(mObserver).onBookmarkItemAdded(ACCOUNT, mNewItem1, 1);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedToDeleteSingleItemInAccountFolder() {
        BookmarkId id1 = new BookmarkId(1, 0);
        BookmarkId id2 = new BookmarkId(2, 0);
        when(mAccountFolderItem1.getId()).thenReturn(id1);
        when(mAccountFolderItem2.getId()).thenReturn(id2);

        mAccountFolderObserver.onBookmarkItemsChanged(ACCOUNT, List.of(mAccountFolderItem1));
        verify(mObserver).onBookmarkItemRemoved(ACCOUNT, 1);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedToReorderItemsInAccountFolder() {
        BookmarkId id1 = new BookmarkId(1, 0);
        BookmarkId id2 = new BookmarkId(2, 0);
        when(mAccountFolderItem1.getId()).thenReturn(id1);
        when(mAccountFolderItem2.getId()).thenReturn(id2);

        mAccountFolderObserver.onBookmarkItemsChanged(
                ACCOUNT, List.of(mAccountFolderItem2, mAccountFolderItem1));
        verify(mObserver).onBookmarkItemMoved(ACCOUNT, 0, 1);
        verifyNoMoreInteractions(mObserver);
    }

    @Test
    @SmallTest
    @SuppressWarnings("DirectInvocationOnMock")
    public void testOnBookmarkItemsChangedToReorderMultipleItemsInAccountFolder() {
        BookmarkId id1 = new BookmarkId(1, 0);
        BookmarkId id2 = new BookmarkId(2, 0);
        BookmarkId id3 = new BookmarkId(3, 0);
        when(mAccountFolderItem1.getId()).thenReturn(id1);
        when(mAccountFolderItem2.getId()).thenReturn(id2);
        when(mNewItem1.getId()).thenReturn(id3);

        // Populate with 3 items first.
        mAccountFolderObserver.onBookmarkItemsChanged(
                ACCOUNT, List.of(mAccountFolderItem1, mAccountFolderItem2, mNewItem1));
        verify(mObserver).onBookmarkItemAdded(ACCOUNT, mNewItem1, 2);

        // Permute to [mNewItem1, mAccountFolderItem1, mAccountFolderItem2] (3rd moved to 1st).
        mAccountFolderObserver.onBookmarkItemsChanged(
                ACCOUNT, List.of(mNewItem1, mAccountFolderItem1, mAccountFolderItem2));
        verify(mObserver).onBookmarkItemMoved(ACCOUNT, 0, 2);
        verifyNoMoreInteractions(mObserver);
    }
}
