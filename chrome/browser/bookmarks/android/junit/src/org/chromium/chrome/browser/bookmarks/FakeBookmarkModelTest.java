// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link FakeBookmarkModel}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FakeBookmarkModelTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FakeBookmarkModel mBookmarkModel;
    @Mock private BookmarkModelObserver mBookmarkModelObserver;

    @Before
    public void setup() {
        mBookmarkModel = (FakeBookmarkModel) FakeBookmarkModel.createModel();
        mBookmarkModel.addObserver(mBookmarkModelObserver);
    }

    @Test
    public void testDefaultFolders() {
        List<BookmarkId> expected =
                Arrays.asList(
                        mBookmarkModel.getOtherFolderId(),
                        mBookmarkModel.getDesktopFolderId(),
                        mBookmarkModel.getMobileFolderId(),
                        mBookmarkModel.getLocalOrSyncableReadingListFolder());
        assertEquals(expected, mBookmarkModel.getTopLevelFolderIds());

        expected = Arrays.asList(mBookmarkModel.getPartnerFolderId());
        assertEquals(expected, mBookmarkModel.getChildIds(mBookmarkModel.getMobileFolderId()));
    }

    @Test
    public void testDefaultFolders_accountStorageEnabled() {
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        List<BookmarkId> expected =
                Arrays.asList(
                        mBookmarkModel.getOtherFolderId(),
                        mBookmarkModel.getDesktopFolderId(),
                        mBookmarkModel.getMobileFolderId(),
                        mBookmarkModel.getAccountOtherFolderId(),
                        mBookmarkModel.getAccountDesktopFolderId(),
                        mBookmarkModel.getAccountMobileFolderId(),
                        mBookmarkModel.getLocalOrSyncableReadingListFolder(),
                        mBookmarkModel.getAccountReadingListFolder());
        assertEquals(expected, mBookmarkModel.getTopLevelFolderIds());
        assertTrue(mBookmarkModel.isAccountBookmark(mBookmarkModel.getAccountReadingListFolder()));

        expected = Arrays.asList(mBookmarkModel.getPartnerFolderId());
        assertEquals(expected, mBookmarkModel.getChildIds(mBookmarkModel.getMobileFolderId()));
    }

    @Test
    public void testAddFolder() {
        int index = 0;
        BookmarkItem parent = mBookmarkModel.getBookmarkById(mBookmarkModel.getOtherFolderId());
        BookmarkId id = mBookmarkModel.addFolder(parent.getId(), index, "user folder");
        verify(mBookmarkModelObserver).bookmarkNodeAdded(parent, index, /* addedByUser= */ false);

        List<BookmarkId> expected = Arrays.asList(id);
        assertEquals(expected, mBookmarkModel.getChildIds(parent.getId()));

        BookmarkItem item = mBookmarkModel.getBookmarkById(id);
        assertNotNull(item);
        assertTrue(item.isFolder());
    }

    @Test
    public void testAddBookmark() {
        int index = 0;
        BookmarkItem parent = mBookmarkModel.getBookmarkById(mBookmarkModel.getOtherFolderId());
        BookmarkId id =
                mBookmarkModel.addBookmark(
                        parent.getId(), index, "user bookmark", new GURL("https://google.com"));
        verify(mBookmarkModelObserver).bookmarkNodeAdded(parent, index, /* addedByUser= */ false);

        List<BookmarkId> expected = Arrays.asList(id);
        assertEquals(expected, mBookmarkModel.getChildIds(parent.getId()));

        BookmarkItem item = mBookmarkModel.getBookmarkById(id);
        assertNotNull(item);
        assertFalse(item.isFolder());
    }

    @Test
    public void testAddAccountReadingListBokmark() {
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        BookmarkId parentId = mBookmarkModel.getAccountReadingListFolder();
        BookmarkItem parent = mBookmarkModel.getBookmarkById(parentId);
        BookmarkId id =
                mBookmarkModel.addToReadingList(
                        parentId, "user account bookmark", new GURL("https://google.com"));
        verify(mBookmarkModelObserver)
                .bookmarkNodeAdded(parent, /* index= */ 0, /* addedByUser= */ false);

        List<BookmarkId> expected = Arrays.asList(id);
        assertEquals(expected, mBookmarkModel.getChildIds(parentId));
        assertTrue(mBookmarkModel.getBookmarkById(id).isAccountBookmark());
    }

    @Test
    public void testEditBookmark() {
        // Add bookmark.
        int index = 0;
        BookmarkItem parent = mBookmarkModel.getBookmarkById(mBookmarkModel.getOtherFolderId());
        BookmarkId id =
                mBookmarkModel.addBookmark(
                        parent.getId(), index, "user bookmark", new GURL("https://google.com/"));

        // Update bookmark title.
        assertEquals("user bookmark", mBookmarkModel.getBookmarkById(id).getTitle());
        mBookmarkModel.setBookmarkTitle(id, "user bookmark 2");
        assertEquals("user bookmark 2", mBookmarkModel.getBookmarkById(id).getTitle());
        verify(mBookmarkModelObserver).bookmarkNodeChanged(mBookmarkModel.getBookmarkById(id));

        // Update bookmark url.
        assertEquals("https://google.com/", mBookmarkModel.getBookmarkById(id).getUrl().getSpec());
        mBookmarkModel.setBookmarkUrl(id, new GURL("https://google2.com/"));
        assertEquals("https://google2.com/", mBookmarkModel.getBookmarkById(id).getUrl().getSpec());
        verify(mBookmarkModelObserver).bookmarkNodeChanged(mBookmarkModel.getBookmarkById(id));

        // Move bookmark.
        int newIndex = 0;
        BookmarkItem newParent = mBookmarkModel.getBookmarkById(mBookmarkModel.getMobileFolderId());
        assertEquals(parent.getId(), mBookmarkModel.getBookmarkById(id).getParentId());
        mBookmarkModel.moveBookmark(id, newParent.getId(), newIndex);
        assertEquals(newParent.getId(), mBookmarkModel.getBookmarkById(id).getParentId());
        verify(mBookmarkModelObserver).bookmarkNodeMoved(parent, index, newParent, newIndex);
    }

    @Test
    public void testReadingList() {
        var parentId = mBookmarkModel.getLocalOrSyncableReadingListFolder();
        var parent = mBookmarkModel.getBookmarkById(parentId);
        var id1 = mBookmarkModel.addToReadingList(parentId, "rl1", new GURL("https://test1.com"));
        verify(mBookmarkModelObserver)
                .bookmarkNodeAdded(parent, /* index= */ 0, /* addedByUser= */ false);
        var id2 = mBookmarkModel.addToReadingList(parentId, "rl2", new GURL("https://test2.com"));
        verify(mBookmarkModelObserver)
                .bookmarkNodeAdded(parent, /* index= */ 1, /* addedByUser= */ false);
        var id3 = mBookmarkModel.addToReadingList(parentId, "rl3", new GURL("https://test3.com"));
        verify(mBookmarkModelObserver)
                .bookmarkNodeAdded(parent, /* index= */ 2, /* addedByUser= */ false);

        List<BookmarkId> expected = Arrays.asList(id1, id2, id3);
        assertEquals(expected, mBookmarkModel.getChildIds(parentId));
        assertEquals(3, mBookmarkModel.getUnreadCount(parentId));

        mBookmarkModel.setReadStatusForReadingList(id1, true);
        assertEquals(2, mBookmarkModel.getUnreadCount(parentId));
        verify(mBookmarkModelObserver).bookmarkNodeChanged(mBookmarkModel.getBookmarkById(id1));
    }

    @Test
    public void testChildCount() {
        BookmarkId parent = mBookmarkModel.getOtherFolderId();
        mBookmarkModel.addBookmark(parent, 0, "title1", new GURL("https://test1.com"));
        mBookmarkModel.addBookmark(parent, 0, "title2", new GURL("https://test2.com"));
        mBookmarkModel.addBookmark(parent, 0, "title3", new GURL("https://test3.com"));
        BookmarkId folder = mBookmarkModel.addFolder(parent, 0, "folder1");
        mBookmarkModel.addBookmark(folder, 0, "title11", new GURL("https://test11.com"));
        mBookmarkModel.addBookmark(folder, 0, "title12", new GURL("https://test12.com"));
        mBookmarkModel.addBookmark(folder, 0, "title13", new GURL("https://test13.com"));
        assertEquals(4, mBookmarkModel.getChildCount(parent));
        assertEquals(7, mBookmarkModel.getTotalBookmarkCount(parent));

        parent = mBookmarkModel.getLocalOrSyncableReadingListFolder();
        mBookmarkModel.addToReadingList(parent, "title1", new GURL("https://test1.com"));
        mBookmarkModel.addToReadingList(parent, "title2", new GURL("https://test2.com"));
        mBookmarkModel.addToReadingList(parent, "title3", new GURL("https://test3.com"));
        assertEquals(3, mBookmarkModel.getChildCount(parent));
        assertEquals(3, mBookmarkModel.getTotalBookmarkCount(parent));
    }

    @Test
    public void testChildIds() {
        // Verify empty state.
        var parent = mBookmarkModel.getOtherFolderId();
        assertTrue(mBookmarkModel.getChildIds(parent).isEmpty());

        // Verify populated state.
        var item1 = mBookmarkModel.addBookmark(parent, 0, "1", new GURL("https://test1.com"));
        var item2 = mBookmarkModel.addBookmark(parent, 1, "2", new GURL("https://test2.com"));
        var item3 = mBookmarkModel.addBookmark(parent, 2, "3", new GURL("https://test3.com"));
        assertEquals(List.of(item1, item2, item3), mBookmarkModel.getChildIds(parent));

        // Verify state after move.
        mBookmarkModel.moveBookmark(item3, parent, 0);
        assertEquals(List.of(item3, item1, item2), mBookmarkModel.getChildIds(parent));

        // Verify state after delete.
        mBookmarkModel.deleteBookmark(item3);
        assertEquals(List.of(item1, item2), mBookmarkModel.getChildIds(parent));
    }

    @Test
    public void testDeleteBookmark() {
        // Add bookmarks.
        var parent = mBookmarkModel.getBookmarkById(mBookmarkModel.getOtherFolderId());
        var f1 = mBookmarkModel.getBookmarkById(mBookmarkModel.addFolder(parent.getId(), 0, "f1"));
        var f2 = mBookmarkModel.getBookmarkById(mBookmarkModel.addFolder(parent.getId(), 1, "f2"));
        var f2a = mBookmarkModel.getBookmarkById(mBookmarkModel.addFolder(f2.getId(), 0, "f2a"));
        var f2b = mBookmarkModel.getBookmarkById(mBookmarkModel.addFolder(f2.getId(), 1, "f2b"));
        var f3 = mBookmarkModel.getBookmarkById(mBookmarkModel.addFolder(parent.getId(), 2, "f3"));

        // Verify counts.
        assertEquals(3, mBookmarkModel.getChildCount(parent.getId()));
        assertEquals(5, mBookmarkModel.getTotalBookmarkCount(parent.getId()));

        // Delete bookmark.
        mBookmarkModel.deleteBookmark(f2.getId());

        // Verify events propagated.
        verify(mBookmarkModelObserver).bookmarkNodeRemoved(f2, 1, f2b, false);
        verify(mBookmarkModelObserver).bookmarkNodeRemoved(f2, 0, f2a, false);
        verify(mBookmarkModelObserver).bookmarkNodeRemoved(parent, 1, f2, false);

        // Verify counts.
        assertEquals(2, mBookmarkModel.getChildCount(parent.getId()));
        assertEquals(2, mBookmarkModel.getTotalBookmarkCount(parent.getId()));
    }

    @Test
    public void testPerformExtensiveBookmarkChanges() {
        clearInvocations(mBookmarkModelObserver);
        mBookmarkModel.performExtensiveBookmarkChanges(
                () -> {
                    // Perform some batched operations.
                    var parentId = mBookmarkModel.getOtherFolderId();
                    var parent = mBookmarkModel.getBookmarkById(parentId);
                    var f1Id = mBookmarkModel.addFolder(parentId, 0, "1");
                    var f2Id = mBookmarkModel.addFolder(parentId, 1, "2");
                    var f2 = mBookmarkModel.getBookmarkById(f2Id);
                    mBookmarkModel.moveBookmark(f1Id, parentId, 1);
                    mBookmarkModel.deleteBookmark(f2Id);

                    // NOTE: Node removed events are still propagated during extensive bookmark
                    // changes because they expose an `isDoingExtensiveChanges` argument.
                    verify(mBookmarkModelObserver).bookmarkNodeRemoved(parent, 0, f2, true);

                    // NOTE: Other events should not be propagated during extensive changes.
                    verifyNoMoreInteractions(mBookmarkModelObserver);
                });
        verify(mBookmarkModelObserver).bookmarkModelChanged();
    }
}
