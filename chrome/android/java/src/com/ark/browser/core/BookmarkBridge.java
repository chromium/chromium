//// Copyright 2013 The Chromium Authors. All rights reserved.
//// Use of this source code is governed by a BSD-style license that can be
//// found in the LICENSE file.
//
//package com.ark.browser.core;
//
//import org.chromium.base.annotations.CalledByNative;
//import org.chromium.base.annotations.NativeMethods;
//import org.chromium.chrome.browser.profiles.Profile;
//import org.chromium.components.bookmarks.BookmarkId;
//import org.chromium.components.bookmarks.BookmarkItem;
//import org.chromium.content_public.browser.WebContents;
//import org.chromium.url.GURL;
//
//import java.util.List;
//
///**
// * Provides the communication channel for Android to fetch and manipulate the
// * bookmark model stored in native.
// */
//public class BookmarkBridge {
//    /**
//     * Interface for callback object for fetching bookmarks and folder hierarchy.
//     */
//    public interface BookmarksCallback {
//        /**
//         * Callback method for fetching bookmarks for a folder and the folder hierarchy.
//         * @param folderId The folder id to which the bookmarks belong.
//         * @param bookmarksList List holding the fetched bookmarks and details.
//         */
//        @CalledByNative("BookmarksCallback")
//        void onBookmarksAvailable(BookmarkId folderId, List<BookmarkItem> bookmarksList);
//
//        /**
//         * Callback method for fetching the folder hierarchy.
//         * @param folderId The folder id to which the bookmarks belong.
//         * @param bookmarksList List holding the fetched folder details.
//         */
//        @CalledByNative("BookmarksCallback")
//        void onBookmarksFolderHierarchyAvailable(BookmarkId folderId,
//                List<BookmarkItem> bookmarksList);
//    }
//
//    /** A callback for updates to the price of a product. */
//    public interface PriceUpdateCallback {
//
//    }
//
//    @CalledByNative
//    private void bookmarkModelLoaded() {
//    }
//
//    @CalledByNative
//    private void destroyFromNative() {
//    }
//
//    @CalledByNative
//    private void bookmarkNodeMoved(
//            BookmarkItem oldParent, int oldIndex, BookmarkItem newParent, int newIndex) {
//    }
//
//    @CalledByNative
//    private void bookmarkNodeAdded(BookmarkItem parent, int index) {
//    }
//
//    @CalledByNative
//    private void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node) {
//    }
//
//    @CalledByNative
//    private void bookmarkAllUserNodesRemoved() {
//    }
//
//    @CalledByNative
//    private void bookmarkNodeChanged(BookmarkItem node) {
//    }
//
//    @CalledByNative
//    private void bookmarkNodeChildrenReordered(BookmarkItem node) {
//    }
//
//    @CalledByNative
//    private void extensiveBookmarkChangesBeginning() {
//    }
//
//    @CalledByNative
//    private void extensiveBookmarkChangesEnded() {
//    }
//
//    @CalledByNative
//    private void bookmarkModelChanged() {
//    }
//
//    @CalledByNative
//    private void editBookmarksEnabledChanged() {
//    }
//
//    @CalledByNative
//    private void onProductPriceUpdated(
//            GURL url, byte[] productPriceBytes, PriceUpdateCallback callback) {
//    }
//
//    @CalledByNative
//    private static BookmarkItem createBookmarkItem(long id, int type, String title, GURL url,
//            boolean isFolder, long parentId, int parentIdType, boolean isEditable,
//            boolean isManaged, long dateAdded, boolean read) {
//        BookmarkId bookmarkId = new BookmarkId(id, type);
//        return new BookmarkItem(bookmarkId, title, url, isFolder,
//                new BookmarkId(parentId, parentIdType), isEditable, isManaged, dateAdded, read,
//                false);
//    }
//
//    @CalledByNative
//    private static void addToList(List<BookmarkItem> bookmarksList, BookmarkItem bookmark) {
//        bookmarksList.add(bookmark);
//    }
//
//    @CalledByNative
//    private static void addToBookmarkIdList(List<BookmarkId> bookmarkIdList, long id, int type) {
//        bookmarkIdList.add(new BookmarkId(id, type));
//    }
//
//    @CalledByNative
//    private static void addToBookmarkIdListWithDepth(List<BookmarkId> folderList, long id,
//            int type, List<Integer> depthList, int depth) {
//        folderList.add(new BookmarkId(id, type));
//        depthList.add(depth);
//    }
//
//    @NativeMethods
//    interface Natives {
//        BookmarkId getBookmarkIdForWebContents(long nativeBookmarkBridge, BookmarkBridge caller,
//                WebContents webContents, boolean onlyEditable);
//        BookmarkItem getBookmarkByID(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
//        void getTopLevelFolderParentIDs(
//                long nativeBookmarkBridge, BookmarkBridge caller, List<BookmarkId> bookmarksList);
//        void getTopLevelFolderIDs(long nativeBookmarkBridge, BookmarkBridge caller,
//                boolean getSpecial, boolean getNormal, List<BookmarkId> bookmarksList);
//        void getAllFoldersWithDepths(long nativeBookmarkBridge, BookmarkBridge caller,
//                List<BookmarkId> folderList, List<Integer> depthList);
//        BookmarkId getRootFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
//        BookmarkId getMobileFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
//        BookmarkId getOtherFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
//        BookmarkId getDesktopFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
//        BookmarkId getPartnerFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
//        String getBookmarkGuidByIdForTesting(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
//        int getChildCount(long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
//        void getChildIDs(long nativeBookmarkBridge, BookmarkBridge caller, long id, int type,
//                List<BookmarkId> bookmarksList);
//        BookmarkId getChildAt(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type, int index);
//        int getTotalBookmarkCount(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
//        void getUpdatedProductPrices(long nativeBookmarkBridge, BookmarkBridge caller, GURL[] gurls,
//                PriceUpdateCallback callback);
//        void setBookmarkTitle(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type, String title);
//        void setBookmarkUrl(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type, GURL url);
//        byte[] getPowerBookmarkMeta(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
//        void setPowerBookmarkMeta(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type, byte[] meta);
//        void deletePowerBookmarkMeta(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
//        boolean doesBookmarkExist(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
//        void getBookmarksForFolder(long nativeBookmarkBridge, BookmarkBridge caller,
//                BookmarkId folderId, BookmarksCallback callback, List<BookmarkItem> bookmarksList);
//        boolean isFolderVisible(
//                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
//        void getCurrentFolderHierarchy(long nativeBookmarkBridge, BookmarkBridge caller,
//                BookmarkId folderId, BookmarksCallback callback, List<BookmarkItem> bookmarksList);
//        BookmarkId addFolder(long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId parent,
//                int index, String title);
//        void deleteBookmark(
//                long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId bookmarkId);
//        void removeAllUserBookmarks(long nativeBookmarkBridge, BookmarkBridge caller);
//        void moveBookmark(long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId bookmarkId,
//                BookmarkId newParentId, int index);
//        BookmarkId addBookmark(long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId parent,
//                int index, String title, GURL url);
//        BookmarkId addPowerBookmark(long nativeBookmarkBridge, BookmarkBridge caller,
//                WebContents webContents, BookmarkId parent, int index, String title, GURL url);
//        void undo(long nativeBookmarkBridge, BookmarkBridge caller);
//        void startGroupingUndos(long nativeBookmarkBridge, BookmarkBridge caller);
//        void endGroupingUndos(long nativeBookmarkBridge, BookmarkBridge caller);
//        void loadEmptyPartnerBookmarkShimForTesting(
//                long nativeBookmarkBridge, BookmarkBridge caller);
//        void loadFakePartnerBookmarkShimForTesting(
//                long nativeBookmarkBridge, BookmarkBridge caller);
//        void searchBookmarks(long nativeBookmarkBridge, BookmarkBridge caller,
//                List<BookmarkId> bookmarkMatches, String query, String[] tags,
//                int powerBookmarkType, int maxNumber);
//        void getBookmarksOfType(long nativeBookmarkBridge, BookmarkBridge caller,
//                List<BookmarkId> bookmarkMatches, int powerBookmarkType);
//        long init(BookmarkBridge caller, Profile profile);
//        boolean isDoingExtensiveChanges(long nativeBookmarkBridge, BookmarkBridge caller);
//        void destroy(long nativeBookmarkBridge, BookmarkBridge caller);
//        boolean isEditBookmarksEnabled(long nativeBookmarkBridge);
//        void reorderChildren(long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId parent,
//                long[] orderedNodes);
//        boolean isBookmarked(long nativeBookmarkBridge, GURL url);
//    }
//}
