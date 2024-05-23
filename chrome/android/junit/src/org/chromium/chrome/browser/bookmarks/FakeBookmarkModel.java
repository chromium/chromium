// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.mockito.Mockito;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;

/**
 * Fake {@link BookmarkModel} for use in tests. Instead of faking the BookmarkModel implementation
 * instead the BookmarkBridge.Natives interface is faked and substituted through TEST_HOOKS. This
 * allows the production BookmarkModel/Bridge to be used as-is.
 */
public class FakeBookmarkModel extends BookmarkModel {
    public static final String OTHER_FOLDER_TITLE = "Other bookmarks";
    public static final String DESKTOP_FOLDER_TITLE = "Bookmarks bar";
    public static final String MOBILE_FOLDER_TITLE = "Mobile bookmarks";
    public static final String PARTNER_FOLDER_TITLE = "Parter bookmarks";
    public static final String READING_LIST_FOLDER_TITLE = "Reading list";

    // Factory constructor for the FakeBoomkarkModel
    public static FakeBookmarkModel createModel() {
        // Temporary Jni mock.
        BookmarkBridgeJni.TEST_HOOKS.setInstanceForTesting(
                Mockito.mock(BookmarkBridge.Natives.class));
        FakeBookmarkModel fakeBookmarkModel = new FakeBookmarkModel();
        return fakeBookmarkModel;
    }

    // Used to assign nodes unique ids.
    private int mNextNodeId;

    // Stores a mapping from BookmarkId to BookmarkItem, and also serves parent lookup requests.
    private final Map<BookmarkId, BookmarkItem> mBookmarkIdToItemMap = new HashMap<>();
    // Stores a mapping from BookmarkId to PowerBookmarkMeta (in byte[] form).
    private final Map<BookmarkId, byte[]> mBookmarkIdToPowerBookmarkMetaMap = new HashMap<>();

    private BookmarkId mRootFolderId;
    private BookmarkId mOtherFolderId;
    private BookmarkId mDesktopFolderId;
    private BookmarkId mMobileFolderId;
    private BookmarkId mAccountOtherFolderId;
    private BookmarkId mAccountDesktopFolderId;
    private BookmarkId mAccountMobileFolderId;
    private BookmarkId mPartnerFolderId;
    private BookmarkId mLocalOrSyncableReadingListFolderId;
    private BookmarkId mAccountReadingListFolderId;
    private boolean mAreAccountBookmarkFoldersActive;

    private FakeBookmarkModel() {
        // The native bookmark bridge pointer will be ignored because the JNI is mocked by
        // BookmarkBridgeNatives.
        super(/* nativeBookmarkBridge= */ 1);
        BookmarkBridgeJni.TEST_HOOKS.setInstanceForTesting(new BookmarkBridgeNatives());
        setupTopLevelFolders();
        bookmarkModelLoaded();
    }

    // Public extensions to the BookmarkModel API for testing.

    /** Adds a managed folder, parent cannot be the root. */
    public BookmarkId addManagedFolder(BookmarkId parent, String title) {
        return addFolder(parent, title, /* isManaged= */ true);
    }

    /** Adds a partner bookmark to the partner bookmark folder. */
    public BookmarkId addPartnerBookmarkItem(String title, GURL url) {
        BookmarkId id = new BookmarkId(mNextNodeId++, BookmarkType.PARTNER);
        return addBookmarkItem(
                id,
                getPartnerFolderId(),
                title,
                url,
                /* isFolder= */ false,
                /* isEditable= */ false,
                /* isManaged= */ false,
                /* read= */ false,
                /* isAccountBookmark= */ false);
    }

    public void setAreAccountBookmarkFoldersActive(boolean active) {
        mAreAccountBookmarkFoldersActive = active;
    }

    // Private functions used internally.

    private void setupTopLevelFolders() {
        // Setup the root folder structure.
        mRootFolderId =
                addPermanentFolder(
                        BookmarkType.NORMAL,
                        /* parent= */ null,
                        /* title= */ "",
                        /* isAccountBookmark= */ false);
        mOtherFolderId =
                addPermanentFolder(
                        BookmarkType.NORMAL,
                        mRootFolderId,
                        OTHER_FOLDER_TITLE,
                        /* isAccountBookmark= */ false);
        mDesktopFolderId =
                addPermanentFolder(
                        BookmarkType.NORMAL,
                        mRootFolderId,
                        DESKTOP_FOLDER_TITLE,
                        /* isAccountBookmark= */ false);
        mMobileFolderId =
                addPermanentFolder(
                        BookmarkType.NORMAL,
                        mRootFolderId,
                        MOBILE_FOLDER_TITLE,
                        /* isAccountBookmark= */ false);
        mAccountOtherFolderId =
                addPermanentFolder(
                        BookmarkType.NORMAL,
                        mRootFolderId,
                        OTHER_FOLDER_TITLE,
                        /* isAccountBookmark= */ true);
        mAccountDesktopFolderId =
                addPermanentFolder(
                        BookmarkType.NORMAL,
                        mRootFolderId,
                        DESKTOP_FOLDER_TITLE,
                        /* isAccountBookmark= */ true);
        mAccountMobileFolderId =
                addPermanentFolder(
                        BookmarkType.NORMAL,
                        mRootFolderId,
                        MOBILE_FOLDER_TITLE,
                        /* isAccountBookmark= */ true);
        mPartnerFolderId =
                addPermanentFolder(
                        BookmarkType.NORMAL,
                        mMobileFolderId,
                        PARTNER_FOLDER_TITLE,
                        /* isAccountBookmark= */ false);
        mLocalOrSyncableReadingListFolderId =
                addPermanentFolder(
                        BookmarkType.READING_LIST,
                        mRootFolderId,
                        READING_LIST_FOLDER_TITLE,
                        /* isAccountBookmark= */ false);
        mAccountReadingListFolderId =
                addPermanentFolder(
                        BookmarkType.READING_LIST,
                        mRootFolderId,
                        READING_LIST_FOLDER_TITLE,
                        /* isAccountBookmark= */ true);
    }

    private BookmarkId addBookmark(
            @BookmarkType int type, BookmarkId parent, String title, GURL url) {
        assert !parent.equals(mRootFolderId);
        assert type == parent.getType();
        return addBookmarkItem(
                type,
                parent,
                title,
                url,
                /* isFolder= */ false,
                /* isEditable= */ true,
                /* isManaged= */ false,
                /* read= */ false,
                FakeBookmarkModel.this.isAccountBookmark(parent));
    }

    private BookmarkId addFolder(BookmarkId parent, String title) {
        return addFolder(parent, title, /* isManaged= */ false);
    }

    private BookmarkId addFolder(BookmarkId parent, String title, boolean isManaged) {
        assert !parent.equals(mRootFolderId);
        assert parent.getType() == BookmarkType.NORMAL;
        return addBookmarkItem(
                BookmarkType.NORMAL,
                parent,
                title,
                /* url= */ null,
                /* isFolder= */ true,
                /* isEditable= */ true,
                isManaged,
                /* read= */ false,
                FakeBookmarkModel.this.isAccountBookmark(parent));
    }

    private BookmarkId addPermanentFolder(
            @BookmarkType int type, BookmarkId parent, String title, boolean isAccountBookmark) {
        return addBookmarkItem(
                type,
                parent,
                title,
                /* url= */ null,
                /* isFolder= */ true,
                /* isEditable= */ false,
                /* isManaged= */ false,
                /* read= */ false,
                isAccountBookmark);
    }

    private BookmarkId addBookmarkItem(
            @BookmarkType int type,
            BookmarkId parent,
            String title,
            GURL url,
            boolean isFolder,
            boolean isEditable,
            boolean isManaged,
            boolean read,
            boolean isAccountBookmark) {
        BookmarkId id = new BookmarkId(mNextNodeId++, type);
        return addBookmarkItem(
                id, parent, title, url, isFolder, isEditable, isManaged, read, isAccountBookmark);
    }

    private BookmarkId addBookmarkItem(
            BookmarkId id,
            BookmarkId parent,
            String title,
            GURL url,
            boolean isFolder,
            boolean isEditable,
            boolean isManaged,
            boolean read,
            boolean isAccountBookmark) {
        assert !mBookmarkIdToItemMap.containsKey(id);
        mBookmarkIdToItemMap.put(
                id,
                new BookmarkItem(
                        id,
                        title,
                        url,
                        isFolder,
                        parent,
                        isEditable,
                        isManaged,
                        /* dateAdded= */ 0,
                        read,
                        /* dateLastOpened= */ 0,
                        isAccountBookmark));
        return id;
    }

    private void updateBookmarkItem(
            BookmarkId id,
            BookmarkId parent,
            String title,
            GURL url,
            boolean isFolder,
            boolean isEditable,
            boolean isManaged,
            boolean read,
            boolean isAccountBookmark) {
        assert mBookmarkIdToItemMap.containsKey(id);
        mBookmarkIdToItemMap.put(
                id,
                new BookmarkItem(
                        id,
                        title,
                        url,
                        isFolder,
                        parent,
                        isEditable,
                        isManaged,
                        /* dateAdded= */ 0,
                        read,
                        /* dateLastOpened= */ 0,
                        isAccountBookmark));
    }

    // BookmarkBridge.Natives implementation.
    private class BookmarkBridgeNatives implements BookmarkBridge.Natives {
        @Override
        public BookmarkModel nativeGetForProfile(Profile profile) {
            return FakeBookmarkModel.this;
        }

        @Override
        public boolean areAccountBookmarkFoldersActive(long nativeBookmarkBridge) {
            return FakeBookmarkModel.this.mAreAccountBookmarkFoldersActive;
        }

        @Override
        public BookmarkId getMostRecentlyAddedUserBookmarkIdForUrl(
                long nativeBookmarkBridge, GURL url) {
            return null;
        }

        @Override
        public BookmarkItem getBookmarkById(long nativeBookmarkBridge, long id, int type) {
            return mBookmarkIdToItemMap.get(new BookmarkId(id, type));
        }

        @Override
        public void getTopLevelFolderIds(
                long nativeBookmarkBridge,
                boolean ignoreVisibility,
                List<BookmarkId> bookmarksList) {
            bookmarksList.addAll(FakeBookmarkModel.this.getChildIds(mRootFolderId));

            // Remove all account folders if the feature flag is disabled.
            if (!areAccountBookmarkFoldersActive(nativeBookmarkBridge)) {
                bookmarksList.remove(mAccountOtherFolderId);
                bookmarksList.remove(mAccountDesktopFolderId);
                bookmarksList.remove(mAccountMobileFolderId);
                bookmarksList.remove(mAccountReadingListFolderId);
            }
        }

        @Override
        public BookmarkId getLocalOrSyncableReadingListFolder(long nativeBookmarkBridge) {
            return mLocalOrSyncableReadingListFolderId;
        }

        @Override
        public BookmarkId getAccountReadingListFolder(long nativeBookmarkBridge) {
            return mAccountReadingListFolderId;
        }

        @Override
        public BookmarkId getDefaultReadingListFolder(long nativeBookmarkBridge) {
            return areAccountBookmarkFoldersActive(nativeBookmarkBridge)
                    ? mAccountReadingListFolderId
                    : mLocalOrSyncableReadingListFolderId;
        }

        @Override
        public BookmarkId getDefaultBookmarkFolder(long nativeBookmarkBridge) {
            return areAccountBookmarkFoldersActive(nativeBookmarkBridge)
                    ? mAccountMobileFolderId
                    : mMobileFolderId;
        }

        @Override
        public void getAllFoldersWithDepths(
                long nativeBookmarkBridge, List<BookmarkId> folderList, List<Integer> depthList) {
            assert false : "Not implemented!";
        }

        @Override
        public BookmarkId getRootFolderId(long nativeBookmarkBridge) {
            return mRootFolderId;
        }

        @Override
        public BookmarkId getMobileFolderId(long nativeBookmarkBridge) {
            return mMobileFolderId;
        }

        @Override
        public BookmarkId getOtherFolderId(long nativeBookmarkBridge) {
            return mOtherFolderId;
        }

        @Override
        public BookmarkId getDesktopFolderId(long nativeBookmarkBridge) {
            return mDesktopFolderId;
        }

        @Override
        public BookmarkId getAccountMobileFolderId(long nativeBookmarkBridge) {
            return mAccountMobileFolderId;
        }

        @Override
        public BookmarkId getAccountOtherFolderId(long nativeBookmarkBridge) {
            return mAccountOtherFolderId;
        }

        @Override
        public BookmarkId getAccountDesktopFolderId(long nativeBookmarkBridge) {
            return mAccountDesktopFolderId;
        }

        @Override
        public BookmarkId getPartnerFolderId(long nativeBookmarkBridge) {
            return mPartnerFolderId;
        }

        @Override
        public String getBookmarkGuidByIdForTesting(long nativeBookmarkBridge, long id, int type) {
            assert false : "Not implemented!";
            return null;
        }

        @Override
        public int getChildCount(long nativeBookmarkBridge, long id, int type) {
            List<BookmarkId> childIds = new ArrayList<>();
            getChildIds(nativeBookmarkBridge, id, type, childIds);
            return childIds.size();
        }

        @Override
        public void getChildIds(
                long nativeBookmarkBridge, long id, int type, List<BookmarkId> bookmarksList) {
            BookmarkId parentId = new BookmarkId(id, type);
            bookmarksList.addAll(
                    mBookmarkIdToItemMap.values().stream()
                            .filter(item -> Objects.equals(item.getParentId(), parentId))
                            .map(item -> item.getId())
                            .sorted((first, second) -> Long.compare(first.getId(), second.getId()))
                            .collect(Collectors.toList()));
        }

        @Override
        public BookmarkId getChildAt(long nativeBookmarkBridge, long id, int type, int index) {
            List<BookmarkId> childIds = new ArrayList<>();
            getChildIds(nativeBookmarkBridge, id, type, childIds);
            return childIds.get(index);
        }

        @Override
        public int getTotalBookmarkCount(long nativeBookmarkBridge, long id, int type) {
            List<BookmarkId> children =
                    FakeBookmarkModel.this.getChildIds(new BookmarkId(id, type));
            int size = children.size();
            while (!children.isEmpty()) {
                BookmarkId childId = children.remove(0);
                BookmarkItem childItem = FakeBookmarkModel.this.getBookmarkById(childId);
                if (!childItem.isFolder()) {
                    continue;
                }

                for (BookmarkId subChildId : FakeBookmarkModel.this.getChildIds(childId)) {
                    size++;
                    children.add(subChildId);
                }
            }
            return size;
        }

        @Override
        public void setBookmarkTitle(long nativeBookmarkBridge, long id, int type, String title) {
            BookmarkId bookmarkId = new BookmarkId(id, type);
            BookmarkItem item = FakeBookmarkModel.this.getBookmarkById(bookmarkId);
            FakeBookmarkModel.this.updateBookmarkItem(
                    bookmarkId,
                    item.getParentId(),
                    title,
                    item.getUrl(),
                    item.isFolder(),
                    item.isEditable(),
                    item.isManaged(),
                    item.isRead(),
                    item.isAccountBookmark());
        }

        @Override
        public void setBookmarkUrl(long nativeBookmarkBridge, long id, int type, GURL url) {
            BookmarkId bookmarkId = new BookmarkId(id, type);
            BookmarkItem item = FakeBookmarkModel.this.getBookmarkById(bookmarkId);
            FakeBookmarkModel.this.updateBookmarkItem(
                    bookmarkId,
                    item.getParentId(),
                    item.getTitle(),
                    url,
                    item.isFolder(),
                    item.isEditable(),
                    item.isManaged(),
                    item.isRead(),
                    item.isAccountBookmark());
        }

        @Override
        public byte[] getPowerBookmarkMeta(long nativeBookmarkBridge, long id, int type) {
            return mBookmarkIdToPowerBookmarkMetaMap.get(new BookmarkId(id, type));
        }

        @Override
        public void setPowerBookmarkMeta(
                long nativeBookmarkBridge, long id, int type, byte[] meta) {
            mBookmarkIdToPowerBookmarkMetaMap.put(new BookmarkId(id, type), meta);
        }

        @Override
        public void deletePowerBookmarkMeta(long nativeBookmarkBridge, long id, int type) {
            mBookmarkIdToPowerBookmarkMetaMap.remove(new BookmarkId(id, type));
        }

        @Override
        public boolean doesBookmarkExist(long nativeBookmarkBridge, long id, int type) {
            return mBookmarkIdToItemMap.containsKey(new BookmarkId(id, type));
        }

        @Override
        public void getBookmarksForFolder(
                long nativeBookmarkBridge, BookmarkId folderId, List<BookmarkItem> bookmarksList) {}

        @Override
        public boolean isFolderVisible(long nativeBookmarkBridge, long id, int type) {
            return true;
        }

        @Override
        public BookmarkId addFolder(
                long nativeBookmarkBridge, BookmarkId parent, int index, String title) {
            return FakeBookmarkModel.this.addFolder(parent, title);
        }

        @Override
        public void deleteBookmark(long nativeBookmarkBridge, BookmarkId bookmarkId) {
            mBookmarkIdToItemMap.remove(bookmarkId);
            mBookmarkIdToPowerBookmarkMetaMap.remove(bookmarkId);
        }

        @Override
        public void removeAllUserBookmarks(long nativeBookmarkBridge) {
            mBookmarkIdToItemMap.clear();
            setupTopLevelFolders();
        }

        @Override
        public void moveBookmark(
                long nativeBookmarkBridge,
                BookmarkId bookmarkId,
                BookmarkId newParentId,
                int index) {
            BookmarkItem item = FakeBookmarkModel.this.getBookmarkById(bookmarkId);
            FakeBookmarkModel.this.updateBookmarkItem(
                    bookmarkId,
                    newParentId,
                    item.getTitle(),
                    item.getUrl(),
                    item.isFolder(),
                    item.isEditable(),
                    item.isManaged(),
                    item.isRead(),
                    item.isAccountBookmark());
        }

        @Override
        public BookmarkId addBookmark(
                long nativeBookmarkBridge, BookmarkId parent, int index, String title, GURL url) {
            return FakeBookmarkModel.this.addBookmark(BookmarkType.NORMAL, parent, title, url);
        }

        @Override
        public BookmarkId addToReadingList(
                long nativeBookmarkBridge, BookmarkId parentId, String title, GURL url) {
            return FakeBookmarkModel.this.addBookmark(
                    BookmarkType.READING_LIST, parentId, title, url);
        }

        @Override
        public void setReadStatus(long nativeBookmarkBridge, BookmarkId bookmarkId, boolean read) {
            BookmarkItem item = FakeBookmarkModel.this.getBookmarkById(bookmarkId);
            FakeBookmarkModel.this.updateBookmarkItem(
                    bookmarkId,
                    item.getParentId(),
                    item.getTitle(),
                    item.getUrl(),
                    item.isFolder(),
                    item.isEditable(),
                    item.isManaged(),
                    read,
                    item.isAccountBookmark());
        }

        @Override
        public int getUnreadCount(long nativeBookmarkBridge, BookmarkId id) {
            List<BookmarkId> childIds = FakeBookmarkModel.this.getChildIds(id);
            return (int)
                    childIds.stream()
                            .map(childId -> FakeBookmarkModel.this.getBookmarkById(childId))
                            .filter(item -> !item.isRead())
                            .count();
        }

        @Override
        public boolean isAccountBookmark(long nativeBookmarkBridge, BookmarkId id) {
            BookmarkItem item = FakeBookmarkModel.this.getBookmarkById(id);
            BookmarkId parentId = item.getParentId();
            BookmarkItem parentItem =
                    parentId == null ? null : FakeBookmarkModel.this.getBookmarkById(parentId);
            return item.isAccountBookmark()
                    || (parentItem != null && parentItem.isAccountBookmark());
        }

        @Override
        public void undo(long nativeBookmarkBridge) {
            assert false : "Not implemented!";
        }

        @Override
        public void startGroupingUndos(long nativeBookmarkBridge) {
            // No-op
        }

        @Override
        public void endGroupingUndos(long nativeBookmarkBridge) {
            // No-op
        }

        @Override
        public void loadEmptyPartnerBookmarkShimForTesting(long nativeBookmarkBridge) {
            assert false : "Not implemented!";
        }

        @Override
        public void loadFakePartnerBookmarkShimForTesting(long nativeBookmarkBridge) {
            assert false : "Not implemented!";
        }

        @Override
        public void searchBookmarks(
                long nativeBookmarkBridge,
                List<BookmarkId> bookmarkMatches,
                String query,
                String[] tags,
                int powerBookmarkType,
                int maxNumber) {
            bookmarkMatches.addAll(
                    mBookmarkIdToItemMap.values().stream()
                            .filter(
                                    item ->
                                            item.getTitle().contains(query)
                                                    || item.getUrlForDisplay().contains(query))
                            .map(item -> item.getId())
                            .sorted((first, second) -> Long.compare(first.getId(), second.getId()))
                            .collect(Collectors.toList()));
        }

        @Override
        public void getBookmarksOfType(
                long nativeBookmarkBridge,
                List<BookmarkId> bookmarkMatches,
                int powerBookmarkType) {
            assert false : "Not implemented!";
        }

        @Override
        public boolean isDoingExtensiveChanges(long nativeBookmarkBridge) {
            return false;
        }

        @Override
        public void destroy(long nativeBookmarkBridge) {
            mBookmarkIdToItemMap.clear();
            setupTopLevelFolders();
        }

        @Override
        public boolean isEditBookmarksEnabled(long nativeBookmarkBridge) {
            assert false : "Not implemented!";
            return false;
        }

        @Override
        public void reorderChildren(
                long nativeBookmarkBridge, BookmarkId parent, long[] orderedNodes) {
            assert false : "Not implemented!";
        }

        @Override
        public boolean isBookmarked(long nativeBookmarkBridge, GURL url) {
            return mBookmarkIdToItemMap.values().stream()
                            .filter(item -> item.getUrl().equals(url))
                            .count()
                    > 0;
        }
    }
}
