// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.util.Pair;

import androidx.test.filters.LargeTest;

import com.google.protobuf.InvalidProtocolBufferException;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.sync.protocol.BookmarkSpecifics;
import org.chromium.components.sync.protocol.SyncEntity;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Test suite for the bookmarks sync data type. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BookmarksTest {
    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    private static final String BOOKMARKS_TYPE_STRING = "Bookmarks";

    private static final GURL URL = new GURL("http://chromium.org/");
    private static final String TITLE = "Chromium";
    private static final String MODIFIED_TITLE = "Chromium2";
    private static final String FOLDER_TITLE = "Tech";

    private BookmarkModel mBookmarkModel;

    // A container to store bookmark information for data verification.
    private static class Bookmark {
        public final String id;
        public final String guid;
        public final String title;
        public final String url;
        public final String parentId;

        private Bookmark(
                String id,
                String guid,
                String title,
                String url,
                String parentId,
                String parentGuid) {
            this.id = id;
            this.guid = guid;
            this.title = title;
            this.url = url;
            this.parentId = parentId;
        }

        public boolean isFolder() {
            return url == null;
        }
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel =
                            BookmarkModel.getForProfile(ProfileManager.getLastUsedRegularProfile());
                    // The BookmarkModel needs to know how to handle partner bookmarks.
                    // Without this call to fake that knowledge for testing, it crashes.
                    mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        // Make sure initial state is clean.
        assertClientBookmarkCount(0);
        assertServerBookmarkCountWithName(0, TITLE);
        assertServerBookmarkCountWithName(0, MODIFIED_TITLE);
        assertServerBookmarkCountWithName(0, FOLDER_TITLE);
    }

    // Test syncing a new bookmark from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadBookmark() throws Exception {
        addServerBookmark(TITLE, URL);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(1);

        List<Bookmark> bookmarks = getClientBookmarks();
        Assert.assertEquals(
                "Only the injected bookmark should exist on the client.", 1, bookmarks.size());
        Bookmark bookmark = bookmarks.get(0);
        Assert.assertEquals("The wrong title was found for the bookmark.", TITLE, bookmark.title);
        Assert.assertEquals(
                "The wrong URL was found for the bookmark.", URL.getSpec(), bookmark.url);
    }

    // Test syncing a bookmark modification from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadBookmarkModification() throws Exception {
        // Add the entity to test modifying.
        addServerBookmark(TITLE, URL);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(1);

        // Modify on server, sync, and verify the modification locally.
        Bookmark bookmark = getClientBookmarks().get(0);
        modifyServerBookmark(bookmark.id, bookmark.guid, MODIFIED_TITLE, URL);
        SyncTestUtil.triggerSync();
        mSyncTestRule.pollInstrumentationThread(
                () -> {
                    Bookmark modifiedBookmark = getClientBookmarks().get(0);
                    Criteria.checkThat(modifiedBookmark.title, Matchers.is(MODIFIED_TITLE));
                });
    }

    // Test syncing a bookmark tombstone from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadBookmarkTombstone() throws Exception {
        // Add the entity to test deleting.
        addServerBookmark(TITLE, URL);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(1);

        // Delete on server, sync, and verify deleted locally.
        Bookmark bookmark = getClientBookmarks().get(0);
        mSyncTestRule.getFakeServerHelper().deleteEntity(bookmark.id);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(0);
    }

    // Test syncing a bookmark modification from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadMovedBookmark() throws Exception {
        // Add the entity to test moving.
        addServerBookmark(TITLE, URL);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(1);

        // Add the folder to move to.
        addServerBookmarkFolder(FOLDER_TITLE);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(2);

        // We should have exactly two (2) bookmark items: one a folder and the
        // other a bookmark. We need to figure out which is which because the
        // order is not being explicitly set on creation.
        //
        // See http://crbug/642128 - Explicitly set order on bookmark creation
        // and verify the order here.
        List<Bookmark> clientBookmarks = getClientBookmarks();
        Assert.assertEquals(2, clientBookmarks.size());
        Bookmark item0 = clientBookmarks.get(0);
        Bookmark item1 = clientBookmarks.get(1);
        Assert.assertTrue(item0.isFolder() != item1.isFolder());
        final int bookmarkIndex = item0.isFolder() ? 1 : 0;
        final Bookmark folder = item0.isFolder() ? item0 : item1;
        final Bookmark bookmark = item0.isFolder() ? item1 : item0;

        // On the server, move the bookmark into the folder then sync, and
        // verify the move locally.
        mSyncTestRule
                .getFakeServerHelper()
                .modifyBookmarkEntity(
                        bookmark.id, bookmark.guid, TITLE, URL, folder.id, folder.guid);
        SyncTestUtil.triggerSync();
        mSyncTestRule.pollInstrumentationThread(
                () -> {
                    List<Bookmark> bookmarks = getClientBookmarks();
                    Bookmark modifiedBookmark = bookmarks.get(bookmarks.get(0).isFolder() ? 1 : 0);
                    // The "s" is prepended because the server adds one to the parentId.
                    Criteria.checkThat(modifiedBookmark.parentId, Matchers.is("s" + folder.id));
                });
    }

    // Test syncing a new bookmark folder from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadBookmarkFolder() throws Exception {
        addServerBookmarkFolder(TITLE);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(1);

        List<Bookmark> bookmarks = getClientBookmarks();
        Assert.assertEquals(
                "Only the injected bookmark folder should exist on the client.",
                1,
                bookmarks.size());
        Bookmark folder = bookmarks.get(0);
        Assert.assertEquals("The wrong title was found for the folder.", TITLE, folder.title);
        Assert.assertEquals("Bookmark folders do not have a URL.", null, folder.url);
    }

    // Test syncing a bookmark folder modification from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadBookmarkFolderModification() throws Exception {
        // Add the entity to test modifying.
        addServerBookmarkFolder(TITLE);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(1);

        // Modify on server, sync, and verify the modification locally.
        Bookmark folder = getClientBookmarks().get(0);
        Assert.assertTrue(folder.isFolder());
        modifyServerBookmarkFolder(folder.id, folder.guid, MODIFIED_TITLE);
        SyncTestUtil.triggerSync();

        mSyncTestRule.pollInstrumentationThread(
                () -> {
                    Bookmark modifiedFolder = getClientBookmarks().get(0);
                    Criteria.checkThat(modifiedFolder.isFolder(), Matchers.is(true));
                    Criteria.checkThat(modifiedFolder.title, Matchers.is(MODIFIED_TITLE));
                });
    }

    // Test syncing a bookmark folder tombstone from server to client.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDownloadBookmarkFolderTombstone() throws Exception {
        // Add the entity to test deleting.
        addServerBookmarkFolder(TITLE);
        assertServerBookmarkCountWithName(1, TITLE);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(1);

        // Delete on server, sync, and verify deleted locally.
        Bookmark folder = getClientBookmarks().get(0);
        Assert.assertTrue(folder.isFolder());

        mSyncTestRule.getFakeServerHelper().deleteEntity(folder.id);
        assertServerBookmarkCountWithName(0, TITLE);
        SyncTestUtil.triggerSync();
        waitForClientBookmarkCount(0);
    }

    // Test syncing a new bookmark from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadBookmark() {
        addClientBookmark(TITLE, URL);
        waitForClientBookmarkCount(1);
        waitForServerBookmarkCountWithName(1, TITLE);
    }

    // Test syncing a bookmark modification from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadBookmarkModification() {
        // Add the entity to test modifying.
        BookmarkId bookmarkId = addClientBookmark(TITLE, URL);
        waitForClientBookmarkCount(1);
        waitForServerBookmarkCountWithName(1, TITLE);

        setClientBookmarkTitle(bookmarkId, MODIFIED_TITLE);
        waitForServerBookmarkCountWithName(1, MODIFIED_TITLE);
        assertServerBookmarkCountWithName(0, TITLE);
    }

    // Test syncing a bookmark tombstone from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadBookmarkTombstone() {
        // Add the entity to test deleting.
        BookmarkId bookmarkId = addClientBookmark(TITLE, URL);
        waitForClientBookmarkCount(1);
        waitForServerBookmarkCountWithName(1, TITLE);

        deleteClientBookmark(bookmarkId);
        waitForClientBookmarkCount(0);
        waitForServerBookmarkCountWithName(0, TITLE);
        assertServerBookmarkCountWithName(0, MODIFIED_TITLE);
    }

    // Test syncing a bookmark modification from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadMovedBookmark() throws Exception {
        // Add the entity to test moving.
        BookmarkId bookmarkId = addClientBookmark(TITLE, URL);
        SyncTestUtil.triggerSync();
        waitForServerBookmarkCountWithName(1, TITLE);

        // Add the folder to move to.
        BookmarkId folderId = addClientBookmarkFolder(FOLDER_TITLE);
        SyncTestUtil.triggerSync();
        waitForServerBookmarkCountWithName(1, FOLDER_TITLE);

        List<Bookmark> bookmarks = getServerBookmarks();
        Assert.assertEquals("Wrong number of bookmarks.", 2, bookmarks.size());
        final Bookmark folder = bookmarks.get(bookmarks.get(0).isFolder() ? 0 : 1);

        // Move on client, sync, and verify the move on the server.
        moveClientBookmark(bookmarkId, folderId);
        SyncTestUtil.triggerSync();
        mSyncTestRule.pollInstrumentationThread(
                () -> {
                    List<Bookmark> serverBookmarks = getServerBookmarks();
                    Bookmark modifiedBookmark =
                            serverBookmarks.get(serverBookmarks.get(0).isFolder() ? 1 : 0);
                    Criteria.checkThat(modifiedBookmark.parentId, Matchers.is(folder.id));
                });
    }

    // Test syncing a new bookmark folder from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadBookmarkFolder() {
        addClientBookmarkFolder(TITLE);
        waitForClientBookmarkCount(1);
        waitForServerBookmarkCountWithName(1, TITLE);
    }

    // Test syncing a bookmark folder modification from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadBookmarkFolderModification() {
        // Add the entity to test modifying.
        BookmarkId bookmarkId = addClientBookmarkFolder(TITLE);
        waitForClientBookmarkCount(1);
        waitForServerBookmarkCountWithName(1, TITLE);

        setClientBookmarkTitle(bookmarkId, MODIFIED_TITLE);
        waitForServerBookmarkCountWithName(1, MODIFIED_TITLE);
        assertServerBookmarkCountWithName(0, TITLE);
    }

    // Test syncing a bookmark folder tombstone from client to server.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testUploadBookmarkFolderTombstone() {
        // Add the entity to test deleting.
        BookmarkId bookmarkId = addClientBookmarkFolder(TITLE);
        waitForClientBookmarkCount(1);
        waitForServerBookmarkCountWithName(1, TITLE);

        deleteClientBookmark(bookmarkId);
        waitForClientBookmarkCount(0);
        waitForServerBookmarkCountWithName(0, TITLE);
    }

    // Test that bookmarks don't get downloaded if the data type is disabled.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDisabledNoDownloadBookmark() throws Exception {
        mSyncTestRule.disableDataType(UserSelectableType.BOOKMARKS);
        addServerBookmark(TITLE, URL);
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        assertClientBookmarkCount(0);
    }

    // Test that bookmarks don't get uploaded if the data type is disabled.
    @Test
    @LargeTest
    @Feature({"Sync"})
    public void testDisabledNoUploadBookmark() {
        mSyncTestRule.disableDataType(UserSelectableType.BOOKMARKS);
        addClientBookmark(TITLE, URL);
        SyncTestUtil.triggerSyncAndWaitForCompletion();
        assertServerBookmarkCountWithName(0, TITLE);
    }

    private BookmarkId addClientBookmark(final String title, final GURL url) {
        BookmarkId id =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            BookmarkId parentId = mBookmarkModel.getMobileFolderId();
                            return mBookmarkModel.addBookmark(parentId, 0, title, url);
                        });
        Assert.assertNotNull("Failed to create bookmark.", id);
        return id;
    }

    private BookmarkId addClientBookmarkFolder(final String title) {
        BookmarkId id =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            BookmarkId parentId = mBookmarkModel.getMobileFolderId();
                            return mBookmarkModel.addFolder(parentId, 0, title);
                        });
        Assert.assertNotNull("Failed to create bookmark folder.", id);
        return id;
    }

    private String getBookmarkBarGuid() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return mBookmarkModel.getBookmarkGuidByIdForTesting(
                            mBookmarkModel.getDesktopFolderId());
                });
    }

    private void addServerBookmark(String title, GURL url) {
        mSyncTestRule
                .getFakeServerHelper()
                .injectBookmarkEntity(
                        title,
                        url,
                        mSyncTestRule.getFakeServerHelper().getBookmarkBarFolderId(),
                        getBookmarkBarGuid());
    }

    private void addServerBookmarkFolder(String title) {
        mSyncTestRule
                .getFakeServerHelper()
                .injectBookmarkFolderEntity(
                        title,
                        mSyncTestRule.getFakeServerHelper().getBookmarkBarFolderId(),
                        getBookmarkBarGuid());
    }

    private void modifyServerBookmark(
            String bookmarkId, String bookmarkGuid, String title, GURL url) {
        mSyncTestRule
                .getFakeServerHelper()
                .modifyBookmarkEntity(
                        bookmarkId,
                        bookmarkGuid,
                        title,
                        url,
                        mSyncTestRule.getFakeServerHelper().getBookmarkBarFolderId(),
                        getBookmarkBarGuid());
    }

    private void modifyServerBookmarkFolder(String folderId, String folderGuid, String title) {
        mSyncTestRule
                .getFakeServerHelper()
                .modifyBookmarkFolderEntity(
                        folderId,
                        folderGuid,
                        title,
                        mSyncTestRule.getFakeServerHelper().getBookmarkBarFolderId(),
                        getBookmarkBarGuid());
    }

    private void deleteClientBookmark(final BookmarkId id) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.deleteBookmark(id);
                });
    }

    private void setClientBookmarkTitle(final BookmarkId id, final String title) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.setBookmarkTitle(id, title);
                });
    }

    private void moveClientBookmark(final BookmarkId id, final BookmarkId newParentId) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.moveBookmark(id, newParentId, 0 /* new index */);
                });
    }

    private List<Bookmark> getClientBookmarks() {
        try {
            List<Pair<String, JSONObject>> rawBookmarks =
                    SyncTestUtil.getLocalData(
                            mSyncTestRule.getTargetContext(), BOOKMARKS_TYPE_STRING);
            List<Bookmark> bookmarks = new ArrayList<Bookmark>(rawBookmarks.size());
            for (Pair<String, JSONObject> rawBookmark : rawBookmarks) {
                String id = rawBookmark.first;
                JSONObject json = rawBookmark.second;
                bookmarks.add(
                        new Bookmark(
                                id,
                                json.getString("guid"),
                                json.getString("legacy_canonicalized_title"),
                                json.optString("url", null),
                                json.getString("parent_id"),
                                json.getString("parent_guid")));
            }
            return bookmarks;
        } catch (JSONException ex) {
            Assert.fail(ex.toString());
            return null;
        }
    }

    private List<Bookmark> getServerBookmarks() {
        try {
            List<SyncEntity> entities =
                    mSyncTestRule
                            .getFakeServerHelper()
                            .getSyncEntitiesByDataType(DataType.BOOKMARKS);
            List<Bookmark> bookmarks = new ArrayList<Bookmark>(entities.size());
            for (SyncEntity entity : entities) {
                String id = entity.getIdString();
                String parentId = entity.getParentIdString();
                BookmarkSpecifics specifics = entity.getSpecifics().getBookmark();
                bookmarks.add(
                        new Bookmark(
                                id,
                                specifics.getGuid(),
                                specifics.getLegacyCanonicalizedTitle(),
                                entity.getFolder() ? null : specifics.getUrl(),
                                parentId,
                                specifics.getParentGuid()));
            }
            return bookmarks;
        } catch (InvalidProtocolBufferException ex) {
            Assert.fail(ex.toString());
            return null;
        }
    }

    private void assertClientBookmarkCount(int count) throws JSONException {
        Assert.assertEquals(
                "There should be " + count + " local bookmarks.",
                count,
                SyncTestUtil.getLocalData(mSyncTestRule.getTargetContext(), BOOKMARKS_TYPE_STRING)
                        .size());
    }

    private void assertServerBookmarkCountWithName(int count, String name) {
        Assert.assertTrue(
                "There should be " + count + " remote bookmarks with name " + name + ".",
                mSyncTestRule
                        .getFakeServerHelper()
                        .verifyEntityCountByTypeAndName(count, DataType.BOOKMARKS, name));
    }

    private void waitForClientBookmarkCount(int n) {
        mSyncTestRule.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                SyncTestUtil.getLocalData(
                                                mSyncTestRule.getTargetContext(),
                                                BOOKMARKS_TYPE_STRING)
                                        .size(),
                                Matchers.is(n));
                    } catch (JSONException ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                });
    }

    private void waitForServerBookmarkCountWithName(final int count, final String name) {
        mSyncTestRule.pollInstrumentationThread(
                () ->
                        mSyncTestRule
                                .getFakeServerHelper()
                                .verifyEntityCountByTypeAndName(count, DataType.BOOKMARKS, name),
                "Expected " + count + " remote bookmarks with name " + name + ".");
    }
}
