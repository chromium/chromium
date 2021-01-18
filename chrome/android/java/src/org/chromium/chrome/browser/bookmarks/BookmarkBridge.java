// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.partnerbookmarks.PartnerBookmarksShim;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides the communication channel for Android to fetch and manipulate the
 * bookmark model stored in native.
 */
public class BookmarkBridge {
    private final Profile mProfile;
    private boolean mIsDoingExtensiveChanges;
    private long mNativeBookmarkBridge;
    private boolean mIsNativeBookmarkModelLoaded;
    private final List<DelayedBookmarkCallback> mDelayedBookmarkCallbacks =
            new ArrayList<DelayedBookmarkCallback>();
    private final ObserverList<BookmarkModelObserver> mObservers =
            new ObserverList<BookmarkModelObserver>();

    /**
     * Interface for callback object for fetching bookmarks and folder hierarchy.
     */
    public interface BookmarksCallback {
        /**
         * Callback method for fetching bookmarks for a folder and the folder hierarchy.
         * @param folderId The folder id to which the bookmarks belong.
         * @param bookmarksList List holding the fetched bookmarks and details.
         */
        @CalledByNative("BookmarksCallback")
        void onBookmarksAvailable(BookmarkId folderId, List<BookmarkItem> bookmarksList);

        /**
         * Callback method for fetching the folder hierarchy.
         * @param folderId The folder id to which the bookmarks belong.
         * @param bookmarksList List holding the fetched folder details.
         */
        @CalledByNative("BookmarksCallback")
        void onBookmarksFolderHierarchyAvailable(BookmarkId folderId,
                List<BookmarkItem> bookmarksList);
    }

    /**
     * Base empty implementation observer class that provides listeners to be notified of changes
     * to the bookmark model. It's mandatory to implement one method, bookmarkModelChanged. Other
     * methods are optional and if they aren't overridden, the default implementation of them will
     * eventually call bookmarkModelChanged. Unless noted otherwise, all the functions won't be
     * called during extensive change.
     */
    public abstract static class BookmarkModelObserver {
        /**
         * Invoked when a node has moved.
         * @param oldParent The parent before the move.
         * @param oldIndex The index of the node in the old parent.
         * @param newParent The parent after the move.
         * @param newIndex The index of the node in the new parent.
         */
        public void bookmarkNodeMoved(
                BookmarkItem oldParent, int oldIndex, BookmarkItem newParent, int newIndex) {
            bookmarkModelChanged();
        }

        /**
         * Invoked when a node has been added.
         * @param parent The parent of the node being added.
         * @param index The index of the added node.
         */
        public void bookmarkNodeAdded(BookmarkItem parent, int index) {
            bookmarkModelChanged();
        }

        /**
         * Invoked when a node has been removed, the item may still be starred though. This can
         * be called during extensive change, and have the flag argument indicating it.
         * @param parent The parent of the node that was removed.
         * @param oldIndex The index of the removed node in the parent before it was removed.
         * @param node The node that was removed.
         * @param isDoingExtensiveChanges whether extensive changes are happening.
         */
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node,
                boolean isDoingExtensiveChanges) {
            if (isDoingExtensiveChanges) return;

            bookmarkNodeRemoved(parent, oldIndex, node);
        }

        /**
         * Invoked when a node has been removed, the item may still be starred though.
         *
         * @param parent The parent of the node that was removed.
         * @param oldIndex The index of the removed node in the parent before it was removed.
         * @param node The node that was removed.
         */
        public void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node) {
            bookmarkModelChanged();
        }

        /**
         * Invoked when all user-editable nodes have been removed. The exception is partner and
         * managed bookmarks, which are not affected by this operation.
         */
        public void bookmarkAllUserNodesRemoved() {
            bookmarkModelChanged();
        }

        /**
         * Invoked when the title or url of a node changes.
         * @param node The node being changed.
         */
        public void bookmarkNodeChanged(BookmarkItem node) {
            bookmarkModelChanged();
        }

        /**
         * Invoked when the children (just direct children, not descendants) of a node have been
         * reordered in some way, such as sorted.
         * @param node The node whose children are being reordered.
         */
        public void bookmarkNodeChildrenReordered(BookmarkItem node) {
            bookmarkModelChanged();
        }

        /**
         * Invoked when the native side of bookmark is loaded and now in usable state.
         */
        public void bookmarkModelLoaded() {
            bookmarkModelChanged();
        }

        /**
         * Invoked when bookmarks became editable or non-editable.
         */
        public void editBookmarksEnabledChanged() {
            bookmarkModelChanged();
        }

        /**
         *  Invoked when there are changes to the bookmark model that don't trigger any of the other
         *  callback methods or it wasn't handled by other callback methods.
         *  Examples:
         *  - On partner bookmarks change.
         *  - On extensive change finished.
         *  - Falling back from other methods that are not overridden in this class.
         */
        public abstract void bookmarkModelChanged();
    }

    /**
     * Contains data about a bookmark or bookmark folder.
     */
    public static class BookmarkItem {
        private final String mTitle;
        private final GURL mUrl;
        private final BookmarkId mId;
        private final boolean mIsFolder;
        private final BookmarkId mParentId;
        private final boolean mIsEditable;
        private final boolean mIsManaged;
        private boolean mForceEditableForTesting;
        private long mDateAdded;
        private boolean mRead;

        @VisibleForTesting
        public BookmarkItem(BookmarkId id, String title, GURL url, boolean isFolder,
                BookmarkId parentId, boolean isEditable, boolean isManaged, long dateAdded,
                boolean read) {
            mId = id;
            mTitle = title;
            mUrl = url;
            mIsFolder = isFolder;
            mParentId = parentId;
            mIsEditable = isEditable;
            mIsManaged = isManaged;
            mDateAdded = dateAdded;
            mRead = read;
        }

        /** @return Title of the bookmark item. */
        public String getTitle() {
            return mTitle;
        }

        /** @return Url of the bookmark item. */
        public GURL getUrl() {
            return mUrl;
        }

        /** @return The string to display for the item's url. */
        public String getUrlForDisplay() {
            return UrlFormatter.formatUrlForSecurityDisplay(
                    getUrl(), SchemeDisplay.OMIT_HTTP_AND_HTTPS);
        }

        /** @return Whether item is a folder or a bookmark. */
        public boolean isFolder() {
            return mIsFolder;
        }

        /** @return Parent id of the bookmark item. */
        public BookmarkId getParentId() {
            return mParentId;
        }

        /** @return Whether this bookmark can be edited. */
        public boolean isEditable() {
            return mForceEditableForTesting || mIsEditable;
        }

        /**@return Whether this bookmark's URL can be edited */
        public boolean isUrlEditable() {
            return isEditable() && mId.getType() == BookmarkType.NORMAL;
        }

        /**@return Whether this bookmark can be moved */
        public boolean isMovable() {
            return isEditable() && mId.getType() == BookmarkType.NORMAL;
        }

        /** @return Whether this is a managed bookmark. */
        public boolean isManaged() {
            return mIsManaged;
        }

        public BookmarkId getId() {
            return mId;
        }

        /**
         * @return The timestamp in milliseconds since epoch that the bookmark is added.
         */
        public long getDateAdded() {
            return mDateAdded;
        }

        /**
         * @return Whether the bookmark is read. Only valid for {@link BookmarkType#READING_LIST}.
         *         Defaults to "false" for other types.
         */
        public boolean isRead() {
            return mRead;
        }

        // TODO(https://crbug.com/1019217): Remove when BookmarkModel is stubbed in tests instead.
        void forceEditableForTesting() {
            mForceEditableForTesting = true;
        }
    }

    /**
     * Handler to fetch the bookmarks, titles, urls and folder hierarchy.
     * @param profile Profile instance corresponding to the active profile.
     */
    public BookmarkBridge(Profile profile) {
        ThreadUtils.assertOnUiThread();
        mProfile = profile;
        mNativeBookmarkBridge = BookmarkBridgeJni.get().init(BookmarkBridge.this, profile);
        mIsDoingExtensiveChanges = BookmarkBridgeJni.get().isDoingExtensiveChanges(
                mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * Destroys this instance so no further calls can be executed.
     */
    public void destroy() {
        if (mNativeBookmarkBridge != 0) {
            BookmarkBridgeJni.get().destroy(mNativeBookmarkBridge, BookmarkBridge.this);
            mNativeBookmarkBridge = 0;
            mIsNativeBookmarkModelLoaded = false;
            mDelayedBookmarkCallbacks.clear();
        }
        mObservers.clear();
    }

    /**
     * @param tab Tab whose current URL is checked against.
     * @return {@code true} if the current Tab URL has a bookmark associated with it. If the
     *         bookmark backend is not loaded, return {@code false}.
     */
    public boolean hasBookmarkIdForTab(Tab tab) {
        ThreadUtils.assertOnUiThread();
        if (tab.isFrozen() || mNativeBookmarkBridge == 0) return false;
        return BookmarkBridgeJni.get().getBookmarkIdForWebContents(
                       mNativeBookmarkBridge, this, tab.getWebContents(), false)
                != BookmarkId.INVALID_ID;
    }

    /**
     * @param tab Tab whose current URL is checked against.
     * @return User-editable bookmark ID or {@link BookmarkId#INVALID_ID} if bookmark backend is
     *         not loaded or the tab is frozen.
     */
    public long getUserBookmarkIdForTab(Tab tab) {
        ThreadUtils.assertOnUiThread();
        if (tab.isFrozen()) return BookmarkId.INVALID_ID;
        return BookmarkBridgeJni.get().getBookmarkIdForWebContents(
                mNativeBookmarkBridge, this, tab.getWebContents(), true);
    }

    /**
     * Load an empty partner bookmark shim for testing. The root node for bookmark will be an
     * empty node.
     */
    @VisibleForTesting
    public void loadEmptyPartnerBookmarkShimForTesting() {
        BookmarkBridgeJni.get().loadEmptyPartnerBookmarkShimForTesting(
                mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * Load a fake partner bookmark shim for testing. To see (or edit) the titles and URLs of the
     * partner bookmarks, go to bookmark_bridge.cc.
     */
    @VisibleForTesting
    public void loadFakePartnerBookmarkShimForTesting() {
        BookmarkBridgeJni.get().loadFakePartnerBookmarkShimForTesting(
                mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * Add an observer to bookmark model changes.
     * @param observer The observer to be added.
     */
    public void addObserver(BookmarkModelObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer of bookmark model changes.
     * @param observer The observer to be removed.
     */
    public void removeObserver(BookmarkModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * @return Whether or not the underlying bookmark model is loaded.
     */
    public boolean isBookmarkModelLoaded() {
        return mIsNativeBookmarkModelLoaded;
    }

    /**
     * Schedules a runnable to run after the bookmark model is loaded. If the
     * model is already loaded, executes the runnable immediately. If not, also
     * kick off partner bookmark reading.
     * @return Whether the given runnable is executed synchronously.
     */
    public boolean finishLoadingBookmarkModel(final Runnable runAfterModelLoaded) {
        if (isBookmarkModelLoaded()) {
            runAfterModelLoaded.run();
            return true;
        }

        long startTime = SystemClock.elapsedRealtime();
        addObserver(new BookmarkModelObserver() {
            @Override
            public void bookmarkModelLoaded() {
                removeObserver(this);
                RecordHistogram.recordTimesHistogram(
                        "PartnerBookmark.LoadingTime", SystemClock.elapsedRealtime() - startTime);
                runAfterModelLoaded.run();
            }
            @Override
            public void bookmarkModelChanged() {
            }
        });

        // Start reading as a fail-safe measure to avoid waiting forever if the caller forgets to
        // call kickOffReading().
        PartnerBookmarksShim.kickOffReading(ContextUtils.getApplicationContext());
        return false;
    }

    /**
     * @return A BookmarkItem instance for the given BookmarkId.
     *         <code>null</code> if it doesn't exist.
     */
    @Nullable
    public BookmarkItem getBookmarkById(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getBookmarkByID(
                mNativeBookmarkBridge, BookmarkBridge.this, id.getId(), id.getType());
    }

    /**
     * @return The top level folder's parents.
     */
    public List<BookmarkId> getTopLevelFolderParentIDs() {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        List<BookmarkId> result = new ArrayList<BookmarkId>();
        BookmarkBridgeJni.get().getTopLevelFolderParentIDs(
                mNativeBookmarkBridge, BookmarkBridge.this, result);
        return result;
    }

    /**
     * @param getSpecial Whether special top folders should be returned.
     * @param getNormal  Whether normal top folders should be returned.
     * @return The top level folders. Note that special folders come first and normal top folders
     *         will be in the alphabetical order.
     */
    public List<BookmarkId> getTopLevelFolderIDs(boolean getSpecial, boolean getNormal) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        List<BookmarkId> result = new ArrayList<BookmarkId>();
        BookmarkBridgeJni.get().getTopLevelFolderIDs(
                mNativeBookmarkBridge, BookmarkBridge.this, getSpecial, getNormal, result);
        return result;
    }

    /**
     * Populates folderList with BookmarkIds of folders users can move bookmarks
     * to and all folders have corresponding depth value in depthList. Folders
     * having depths of 0 will be shown as top-layered folders. These include
     * "Desktop Folder" itself as well as all children of "mobile" and "other".
     * Children of 0-depth folders have depth of 1, and so on.
     *
     * The result list will be sorted alphabetically by title. "mobile", "other",
     * root node, managed folder, partner folder are NOT included as results.
     */
    @VisibleForTesting
    public void getAllFoldersWithDepths(List<BookmarkId> folderList,
            List<Integer> depthList) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        BookmarkBridgeJni.get().getAllFoldersWithDepths(
                mNativeBookmarkBridge, BookmarkBridge.this, folderList, depthList);
    }

    /**
     * Calls {@link #getAllFoldersWithDepths(List, List)} and remove all folders and children
     * in bookmarksToMove. This method is useful when finding a list of possible parent folers when
     * moving some folders (a folder cannot be moved to its own children).
     */
    public void getMoveDestinations(List<BookmarkId> folderList,
            List<Integer> depthList, List<BookmarkId> bookmarksToMove) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        BookmarkBridgeJni.get().getAllFoldersWithDepths(
                mNativeBookmarkBridge, BookmarkBridge.this, folderList, depthList);
        if (bookmarksToMove == null || bookmarksToMove.size() == 0) return;

        boolean shouldTrim = false;
        int trimThreshold = -1;
        for (int i = 0; i < folderList.size(); i++) {
            int depth = depthList.get(i);
            if (shouldTrim) {
                if (depth <= trimThreshold) {
                    shouldTrim = false;
                    trimThreshold = -1;
                } else {
                    folderList.remove(i);
                    depthList.remove(i);
                    i--;
                }
            }
            // Do not use else here because shouldTrim could be set true after if (shouldTrim)
            // statement.
            if (!shouldTrim) {
                BookmarkId folder = folderList.get(i);
                if (bookmarksToMove.contains(folder)) {
                    shouldTrim = true;
                    trimThreshold = depth;
                    folderList.remove(i);
                    depthList.remove(i);
                    i--;
                }
            }
        }
    }

    /**
     * @return The BookmarkId for root folder node
     */
    public BookmarkId getRootFolderId() {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getRootFolderId(mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * @return The BookmarkId for Mobile folder node
     */
    public BookmarkId getMobileFolderId() {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getMobileFolderId(
                mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * @return Id representing the special "other" folder from bookmark model.
     */
    public BookmarkId getOtherFolderId() {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getOtherFolderId(mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * @return BookmarkId representing special "desktop" folder, namely "bookmark bar".
     */
    public BookmarkId getDesktopFolderId() {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getDesktopFolderId(
                mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * @return The number of children that the given node has.
     */
    public int getChildCount(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getChildCount(
                mNativeBookmarkBridge, BookmarkBridge.this, id.getId(), id.getType());
    }

    /**
     * Reads sub-folder IDs, sub-bookmark IDs, or both of the given folder.
     *
     * @return Child IDs of the given folder, with the specified type.
     */
    public List<BookmarkId> getChildIDs(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        List<BookmarkId> result = new ArrayList<BookmarkId>();
        BookmarkBridgeJni.get().getChildIDs(
                mNativeBookmarkBridge, BookmarkBridge.this, id.getId(), id.getType(), result);
        return result;
    }

    /**
     * Gets the child of a folder at the specific position.
     * @param folderId Id of the parent folder
     * @param index Position of child among all children in folder
     * @return BookmarkId of the child, which will be null if folderId does not point to a folder or
     *         index is invalid.
     */
    public BookmarkId getChildAt(BookmarkId folderId, int index) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getChildAt(mNativeBookmarkBridge, BookmarkBridge.this,
                folderId.getId(), folderId.getType(), index);
    }

    /**
     * Get the total number of bookmarks in the sub tree of the specified folder.
     * @param id The {@link BookmarkId} of the folder to be queried.
     * @return The total number of bookmarks in the folder.
     */
    public int getTotalBookmarkCount(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getTotalBookmarkCount(
                mNativeBookmarkBridge, BookmarkBridge.this, id.getId(), id.getType());
    }

    /**
     * Synchronously gets a list of bookmarks that match the specified search query.
     * @param query Keyword used for searching bookmarks.
     * @param maxNumberOfResult Maximum number of result to fetch.
     * @return List of bookmark IDs that are related to the given query.
     */
    public List<BookmarkId> searchBookmarks(String query, int maxNumberOfResult) {
        ThreadUtils.assertOnUiThread();
        List<BookmarkId> bookmarkMatches = new ArrayList<BookmarkId>();
        BookmarkBridgeJni.get().searchBookmarks(mNativeBookmarkBridge, BookmarkBridge.this,
                bookmarkMatches, query, maxNumberOfResult);
        return bookmarkMatches;
    }


    /**
     * Set title of the given bookmark.
     */
    public void setBookmarkTitle(BookmarkId id, String title) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        BookmarkBridgeJni.get().setBookmarkTitle(
                mNativeBookmarkBridge, BookmarkBridge.this, id.getId(), id.getType(), title);
    }

    /**
     * Set URL of the given bookmark.
     */
    public void setBookmarkUrl(BookmarkId id, GURL url) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        assert id.getType() == BookmarkType.NORMAL;
        BookmarkBridgeJni.get().setBookmarkUrl(
                mNativeBookmarkBridge, BookmarkBridge.this, id.getId(), id.getType(), url);
    }

    /**
     * @return Whether the given bookmark exist in the current bookmark model, e.g., not deleted.
     */
    public boolean doesBookmarkExist(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().doesBookmarkExist(
                mNativeBookmarkBridge, BookmarkBridge.this, id.getId(), id.getType());
    }

    /**
     * Fetches the bookmarks of the given folder. This is an always-synchronous version of another
     * getBookmarksForFolder function.
     *
     * @param folderId The parent folder id.
     * @return Bookmarks of the given folder.
     */
    public List<BookmarkItem> getBookmarksForFolder(BookmarkId folderId) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        List<BookmarkItem> result = new ArrayList<BookmarkItem>();
        BookmarkBridgeJni.get().getBookmarksForFolder(
                mNativeBookmarkBridge, BookmarkBridge.this, folderId, null, result);
        return result;
    }

    /**
     * Fetches the bookmarks of the current folder. Callback will be
     * synchronous if the bookmark model is already loaded and async if it is loaded in the
     * background.
     * @param folderId The current folder id.
     * @param callback Instance of a callback object.
     */
    public void getBookmarksForFolder(BookmarkId folderId, BookmarksCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (mIsNativeBookmarkModelLoaded) {
            BookmarkBridgeJni.get().getBookmarksForFolder(mNativeBookmarkBridge,
                    BookmarkBridge.this, folderId, callback, new ArrayList<BookmarkItem>());
        } else {
            mDelayedBookmarkCallbacks.add(new DelayedBookmarkCallback(folderId, callback,
                    DelayedBookmarkCallback.GET_BOOKMARKS_FOR_FOLDER, this));
        }
    }

    /**
     * Check whether the given folder should be visible. This is for top permanent folders that we
     * want to hide when there is no child.
     * @return Whether the given folder should be visible.
     */
    public boolean isFolderVisible(BookmarkId id) {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().isFolderVisible(
                mNativeBookmarkBridge, BookmarkBridge.this, id.getId(), id.getType());
    }

    /**
     * Fetches the folder hierarchy of the given folder. Callback will be
     * synchronous if the bookmark model is already loaded and async if it is loaded in the
     * background.
     * @param folderId The current folder id.
     * @param callback Instance of a callback object.
     */
    public void getCurrentFolderHierarchy(BookmarkId folderId, BookmarksCallback callback) {
        ThreadUtils.assertOnUiThread();
        if (mIsNativeBookmarkModelLoaded) {
            BookmarkBridgeJni.get().getCurrentFolderHierarchy(mNativeBookmarkBridge,
                    BookmarkBridge.this, folderId, callback, new ArrayList<BookmarkItem>());
        } else {
            mDelayedBookmarkCallbacks.add(new DelayedBookmarkCallback(folderId, callback,
                    DelayedBookmarkCallback.GET_CURRENT_FOLDER_HIERARCHY, this));
        }
    }

    /**
     * Deletes a specified bookmark node.
     * @param bookmarkId The ID of the bookmark to be deleted.
     */
    public void deleteBookmark(BookmarkId bookmarkId) {
        ThreadUtils.assertOnUiThread();
        BookmarkBridgeJni.get().deleteBookmark(
                mNativeBookmarkBridge, BookmarkBridge.this, bookmarkId);
    }

    /**
     * Removes all the non-permanent bookmark nodes that are editable by the user. Observers are
     * only notified when all nodes have been removed. There is no notification for individual node
     * removals.
     */
    public void removeAllUserBookmarks() {
        ThreadUtils.assertOnUiThread();
        BookmarkBridgeJni.get().removeAllUserBookmarks(mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * Move the bookmark to the new index within same folder or to a different folder.
     * @param bookmarkId The id of the bookmark that is being moved.
     * @param newParentId The parent folder id.
     * @param index The new index for the bookmark.
     */
    public void moveBookmark(BookmarkId bookmarkId, BookmarkId newParentId, int index) {
        ThreadUtils.assertOnUiThread();
        BookmarkBridgeJni.get().moveBookmark(
                mNativeBookmarkBridge, BookmarkBridge.this, bookmarkId, newParentId, index);
    }

    /**
     * Add a new folder to the given parent folder
     *
     * @param parent Folder where to add. Must be a normal editable folder, instead of a partner
     *               bookmark folder or a managed bookmark folder or root node of the entire
     *               bookmark model.
     * @param index The position to locate the new folder
     * @param title The title text of the new folder
     * @return Id of the added node. If adding failed (index is invalid, string is null, parent is
     *         not editable), returns null.
     */
    public BookmarkId addFolder(BookmarkId parent, int index, String title) {
        ThreadUtils.assertOnUiThread();
        assert parent.getType() == BookmarkType.NORMAL;
        assert index >= 0;
        assert title != null;

        return BookmarkBridgeJni.get().addFolder(
                mNativeBookmarkBridge, BookmarkBridge.this, parent, index, title);
    }

    /**
     * Add a new bookmark to a specific position below parent
     *
     * @param parent Folder where to add. Must be a normal editable folder, instead of a partner
     *               bookmark folder or a managed bookmark folder or root node of the entire
     *               bookmark model.
     * @param index The position where the bookmark will be placed in parent folder
     * @param title Title of the new bookmark. If empty, the URL will be used as the title.
     * @param url Url of the new bookmark
     * @return Id of the added node. If adding failed (index is invalid, string is null, parent is
     *         not editable), returns null.
     */
    public BookmarkId addBookmark(BookmarkId parent, int index, String title, GURL url) {
        ThreadUtils.assertOnUiThread();
        assert parent.getType() == BookmarkType.NORMAL;
        assert index >= 0;
        assert title != null;
        assert url != null;

        if (TextUtils.isEmpty(title)) title = url.getSpec();
        return BookmarkBridgeJni.get().addBookmark(
                mNativeBookmarkBridge, BookmarkBridge.this, parent, index, title, url);
    }

    @Deprecated // Only included until internal repository is updated.
    public BookmarkId addBookmark(BookmarkId parent, int index, String title, String url) {
        return addBookmark(parent, index, title, new GURL(url));
    }

    /**
     * Undo the last undoable action on the top of the bookmark undo stack
     */
    public void undo() {
        ThreadUtils.assertOnUiThread();
        BookmarkBridgeJni.get().undo(mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * Start grouping actions for a single undo operation
     * Note: This only works with BookmarkModel, not partner bookmarks.
     */
    public void startGroupingUndos() {
        ThreadUtils.assertOnUiThread();
        BookmarkBridgeJni.get().startGroupingUndos(mNativeBookmarkBridge, BookmarkBridge.this);
    }

    /**
     * End grouping actions for a single undo operation
     * Note: This only works with BookmarkModel, not partner bookmarks.
     */
    public void endGroupingUndos() {
        ThreadUtils.assertOnUiThread();
        BookmarkBridgeJni.get().endGroupingUndos(mNativeBookmarkBridge, BookmarkBridge.this);
    }

    public boolean isEditBookmarksEnabled() {
        ThreadUtils.assertOnUiThread();
        if (mNativeBookmarkBridge == 0) return false;
        return BookmarkBridgeJni.get().isEditBookmarksEnabled(mNativeBookmarkBridge);
    }

    /** Gets the profile. */
    protected Profile getProfile() {
        return mProfile;
    }

    /**
     * Notifies the observer that bookmark model has been loaded.
     */
    @VisibleForTesting
    public void notifyBookmarkModelLoaded() {
        // Call isBookmarkModelLoaded() to do the check since it could be overridden by the child
        // class to add the addition logic.
        if (isBookmarkModelLoaded()) {
            for (BookmarkModelObserver observer : mObservers) {
                observer.bookmarkModelLoaded();
            }
        }
    }

    /**
     * Reorders the bookmarks of the folder "parent" to be as specified by newOrder.
     *
     * @param parent The parent folder for the reordered bookmarks.
     * @param newOrder A list of bookmark IDs that represents the new order for these bookmarks.
     */
    public void reorderBookmarks(BookmarkId parent, long[] newOrder) {
        ThreadUtils.assertOnUiThread();
        BookmarkBridgeJni.get().reorderChildren(
                mNativeBookmarkBridge, BookmarkBridge.this, parent, newOrder);
    }

    /**
     * Adds an article to the reading list. If the article was already bookmarked, the existing
     * bookmark ID will be returned.
     * @param title The title to be used for the reading list item.
     * @param url The URL of the reading list item.
     * @return The bookmark ID created after saving the article to the reading list, or null on
     *         error.
     */
    public @Nullable BookmarkId addToReadingList(String title, GURL url) {
        ThreadUtils.assertOnUiThread();
        assert title != null;
        assert url != null;
        assert mIsNativeBookmarkModelLoaded;

        return BookmarkBridgeJni.get().addToReadingList(
                mNativeBookmarkBridge, BookmarkBridge.this, title, url);
    }

    /**
     * @param url The URL of the reading list item.
     * @return The reading list item with the URL, or null if no such reading list item.
     */
    public BookmarkItem getReadingListItem(GURL url) {
        ThreadUtils.assertOnUiThread();
        assert url != null;
        assert mIsNativeBookmarkModelLoaded;

        return BookmarkBridgeJni.get().getReadingListItem(
                mNativeBookmarkBridge, BookmarkBridge.this, url);
    }

    /**
     * Helper method to mark an article as read.
     * @param url The URL of the reading list item.
     * @param read Whether the article should be marked as read.
     */
    public void setReadStatusForReadingList(GURL url, boolean read) {
        BookmarkBridgeJni.get().setReadStatus(
                mNativeBookmarkBridge, BookmarkBridge.this, url, read);
    }

    /**
     * Checks whether supplied URL has already been bookmarked.
     * @param url The URL to check.
     * @return Whether the URL has been bookmarked.
     */
    public boolean isBookmarked(GURL url) {
        return BookmarkBridgeJni.get().isBookmarked(mNativeBookmarkBridge, url);
    }

    @VisibleForTesting
    BookmarkId getPartnerFolderId() {
        ThreadUtils.assertOnUiThread();
        assert mIsNativeBookmarkModelLoaded;
        return BookmarkBridgeJni.get().getPartnerFolderId(
                mNativeBookmarkBridge, BookmarkBridge.this);
    }

    @CalledByNative
    private void bookmarkModelLoaded() {
        mIsNativeBookmarkModelLoaded = true;

        notifyBookmarkModelLoaded();

        if (!mDelayedBookmarkCallbacks.isEmpty()) {
            for (int i = 0; i < mDelayedBookmarkCallbacks.size(); i++) {
                mDelayedBookmarkCallbacks.get(i).callCallbackMethod();
            }
            mDelayedBookmarkCallbacks.clear();
        }
    }

    @CalledByNative
    private void destroyFromNative() {
        destroy();
    }

    @CalledByNative
    private void bookmarkNodeMoved(
            BookmarkItem oldParent, int oldIndex, BookmarkItem newParent, int newIndex) {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeMoved(oldParent, oldIndex, newParent, newIndex);
        }
    }

    @CalledByNative
    private void bookmarkNodeAdded(BookmarkItem parent, int index) {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeAdded(parent, index);
        }
    }

    @CalledByNative
    private void bookmarkNodeRemoved(BookmarkItem parent, int oldIndex, BookmarkItem node) {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeRemoved(parent, oldIndex, node,
                    mIsDoingExtensiveChanges);
        }
    }

    @CalledByNative
    private void bookmarkAllUserNodesRemoved() {
        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkAllUserNodesRemoved();
        }
    }

    @CalledByNative
    private void bookmarkNodeChanged(BookmarkItem node) {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeChanged(node);
        }
    }

    @CalledByNative
    private void bookmarkNodeChildrenReordered(BookmarkItem node) {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkNodeChildrenReordered(node);
        }
    }

    @CalledByNative
    private void extensiveBookmarkChangesBeginning() {
        mIsDoingExtensiveChanges = true;
    }

    @CalledByNative
    private void extensiveBookmarkChangesEnded() {
        mIsDoingExtensiveChanges = false;
        bookmarkModelChanged();
    }

    @CalledByNative
    private void bookmarkModelChanged() {
        if (mIsDoingExtensiveChanges) return;

        for (BookmarkModelObserver observer : mObservers) {
            observer.bookmarkModelChanged();
        }
    }

    @CalledByNative
    private void editBookmarksEnabledChanged() {
        for (BookmarkModelObserver observer : mObservers) {
            observer.editBookmarksEnabledChanged();
        }
    }

    @CalledByNative
    private static BookmarkItem createBookmarkItem(long id, int type, String title, GURL url,
            boolean isFolder, long parentId, int parentIdType, boolean isEditable,
            boolean isManaged, long dateAdded, boolean read) {
        return new BookmarkItem(new BookmarkId(id, type), title, url, isFolder,
                new BookmarkId(parentId, parentIdType), isEditable, isManaged, dateAdded, read);
    }

    @CalledByNative
    private static void addToList(List<BookmarkItem> bookmarksList, BookmarkItem bookmark) {
        bookmarksList.add(bookmark);
    }

    @CalledByNative
    private static void addToBookmarkIdList(List<BookmarkId> bookmarkIdList, long id, int type) {
        bookmarkIdList.add(new BookmarkId(id, type));
    }

    @CalledByNative
    private static void addToBookmarkIdListWithDepth(List<BookmarkId> folderList, long id,
            int type, List<Integer> depthList, int depth) {
        folderList.add(new BookmarkId(id, type));
        depthList.add(depth);
    }

    private static List<Pair<Integer, Integer>> createPairsList(int[] left, int[] right) {
        List<Pair<Integer, Integer>> pairList = new ArrayList<Pair<Integer, Integer>>();
        for (int i = 0; i < left.length; i++) {
            pairList.add(new Pair<Integer, Integer>(left[i], right[i]));
        }
        return pairList;
    }

    /**
     * Details about callbacks that need to be called once the bookmark model has loaded.
     */
    private static class DelayedBookmarkCallback {

        private static final int GET_BOOKMARKS_FOR_FOLDER = 0;
        private static final int GET_CURRENT_FOLDER_HIERARCHY = 1;

        private final BookmarksCallback mCallback;
        private final BookmarkId mFolderId;
        private final int mCallbackMethod;
        private final BookmarkBridge mHandler;

        private DelayedBookmarkCallback(BookmarkId folderId, BookmarksCallback callback,
                int method, BookmarkBridge handler) {
            mFolderId = folderId;
            mCallback = callback;
            mCallbackMethod = method;
            mHandler = handler;
        }

        /**
         * Invoke the callback method.
         */
        private void callCallbackMethod() {
            switch (mCallbackMethod) {
                case GET_BOOKMARKS_FOR_FOLDER:
                    mHandler.getBookmarksForFolder(mFolderId, mCallback);
                    break;
                case GET_CURRENT_FOLDER_HIERARCHY:
                    mHandler.getCurrentFolderHierarchy(mFolderId, mCallback);
                    break;
                default:
                    assert false;
                    break;
            }
        }
    }

    @NativeMethods
    interface Natives {
        long getBookmarkIdForWebContents(long nativeBookmarkBridge, BookmarkBridge caller,
                WebContents webContents, boolean onlyEditable);
        BookmarkItem getBookmarkByID(
                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
        void getTopLevelFolderParentIDs(
                long nativeBookmarkBridge, BookmarkBridge caller, List<BookmarkId> bookmarksList);
        void getTopLevelFolderIDs(long nativeBookmarkBridge, BookmarkBridge caller,
                boolean getSpecial, boolean getNormal, List<BookmarkId> bookmarksList);
        void getAllFoldersWithDepths(long nativeBookmarkBridge, BookmarkBridge caller,
                List<BookmarkId> folderList, List<Integer> depthList);
        BookmarkId getRootFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
        BookmarkId getMobileFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
        BookmarkId getOtherFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
        BookmarkId getDesktopFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
        BookmarkId getPartnerFolderId(long nativeBookmarkBridge, BookmarkBridge caller);
        int getChildCount(long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
        void getChildIDs(long nativeBookmarkBridge, BookmarkBridge caller, long id, int type,
                List<BookmarkId> bookmarksList);
        BookmarkId getChildAt(
                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type, int index);
        int getTotalBookmarkCount(
                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
        void setBookmarkTitle(
                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type, String title);
        void setBookmarkUrl(
                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type, GURL url);
        boolean doesBookmarkExist(
                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
        void getBookmarksForFolder(long nativeBookmarkBridge, BookmarkBridge caller,
                BookmarkId folderId, BookmarksCallback callback, List<BookmarkItem> bookmarksList);
        boolean isFolderVisible(
                long nativeBookmarkBridge, BookmarkBridge caller, long id, int type);
        void getCurrentFolderHierarchy(long nativeBookmarkBridge, BookmarkBridge caller,
                BookmarkId folderId, BookmarksCallback callback, List<BookmarkItem> bookmarksList);
        BookmarkId addFolder(long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId parent,
                int index, String title);
        void deleteBookmark(
                long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId bookmarkId);
        void removeAllUserBookmarks(long nativeBookmarkBridge, BookmarkBridge caller);
        void moveBookmark(long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId bookmarkId,
                BookmarkId newParentId, int index);
        BookmarkId addBookmark(long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId parent,
                int index, String title, GURL url);
        BookmarkId addToReadingList(
                long nativeBookmarkBridge, BookmarkBridge caller, String title, GURL url);
        BookmarkItem getReadingListItem(long nativeBookmarkBridge, BookmarkBridge caller, GURL url);
        void setReadStatus(
                long nativeBookmarkBridge, BookmarkBridge caller, GURL url, boolean read);
        void undo(long nativeBookmarkBridge, BookmarkBridge caller);
        void startGroupingUndos(long nativeBookmarkBridge, BookmarkBridge caller);
        void endGroupingUndos(long nativeBookmarkBridge, BookmarkBridge caller);
        void loadEmptyPartnerBookmarkShimForTesting(
                long nativeBookmarkBridge, BookmarkBridge caller);
        void loadFakePartnerBookmarkShimForTesting(
                long nativeBookmarkBridge, BookmarkBridge caller);
        void searchBookmarks(long nativeBookmarkBridge, BookmarkBridge caller,
                List<BookmarkId> bookmarkMatches, String query, int maxNumber);
        long init(BookmarkBridge caller, Profile profile);
        boolean isDoingExtensiveChanges(long nativeBookmarkBridge, BookmarkBridge caller);
        void destroy(long nativeBookmarkBridge, BookmarkBridge caller);
        boolean isEditBookmarksEnabled(long nativeBookmarkBridge);
        void reorderChildren(long nativeBookmarkBridge, BookmarkBridge caller, BookmarkId parent,
                long[] orderedNodes);
        boolean isBookmarked(long nativeBookmarkBridge, GURL url);
    }
}
