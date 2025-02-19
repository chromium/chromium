// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.components.bookmarks.BookmarkItem;

/**
 * Base empty implementation observer class that provides listeners to be notified of changes to the
 * bookmark model. It's mandatory to implement one method, bookmarkModelChanged. Other methods are
 * optional and if they aren't overridden, the default implementation of them will eventually call
 * bookmarkModelChanged. Unless noted otherwise, all the functions won't be called during extensive
 * change.
 */
public abstract class BookmarkModelObserver {
    /**
     * Invoked when a node has moved.
     *
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
     *
     * @param parent The parent of the node being added.
     * @param index The index of the added node.
     */
    public void bookmarkNodeAdded(BookmarkItem parent, int index) {
        bookmarkModelChanged();
    }

    /**
     * Invoked when a node has been removed, the item may still be starred though. This can be
     * called during extensive change, and have the flag argument indicating it.
     *
     * @param parent The parent of the node that was removed.
     * @param oldIndex The index of the removed node in the parent before it was removed.
     * @param node The node that was removed.
     * @param isDoingExtensiveChanges whether extensive changes are happening.
     */
    public void bookmarkNodeRemoved(
            BookmarkItem parent, int oldIndex, BookmarkItem node, boolean isDoingExtensiveChanges) {
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
     * Invoked when all user-editable nodes have been removed. The exception is partner and managed
     * bookmarks, which are not affected by this operation.
     */
    public void bookmarkAllUserNodesRemoved() {
        bookmarkModelChanged();
    }

    /**
     * Invoked when the title or url of a node changes.
     *
     * @param node The node being changed.
     */
    public void bookmarkNodeChanged(BookmarkItem node) {
        bookmarkModelChanged();
    }

    /**
     * Invoked when the children (just direct children, not descendants) of a node have been
     * reordered in some way, such as sorted.
     *
     * @param node The node whose children are being reordered.
     */
    public void bookmarkNodeChildrenReordered(BookmarkItem node) {
        bookmarkModelChanged();
    }

    /** Invoked when the native side of bookmark is loaded and now in usable state. */
    public void bookmarkModelLoaded() {
        bookmarkModelChanged();
    }

    /** Invoked when bookmarks became editable or non-editable. */
    public void editBookmarksEnabledChanged() {
        bookmarkModelChanged();
    }

    /**
     * Invoked when there are changes to the bookmark model that don't trigger any of the other
     * callback methods or it wasn't handled by other callback methods. Examples: - On partner
     * bookmarks change. - On extensive change finished. - Falling back from other methods that are
     * not overridden in this class.
     */
    public abstract void bookmarkModelChanged();
}
