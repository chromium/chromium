// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.bookmarks.BookmarkId;

import java.util.List;
import java.util.Optional;

/** Consolidates logic about opening bookmarks. */
@NullMarked
public interface BookmarkOpener {
    /** Observer interface for when bookmarks are opened. */
    interface Observer {
        /**
         * Called when a bookmark opened event happens. This is invoked once when multiple bookmarks
         * are opened in new tabs.
         */
        void onAnyBookmarkOpenedEvent();
    }

    /** Destroys the opener. */
    void destroy();

    /** Add an observer to the class. */
    void addObserver(Observer obs);

    /** Remove an observer from the class. */
    void removeObserver(Observer obs);

    /**
     * Open the given id in the current tab.
     *
     * @param incognito Whether the bookmark should be opened in incognito mode.
     * @return Whether the bookmark id was successfully opened.
     */
    boolean openBookmarkInCurrentTab(BookmarkId id, boolean incognito);

    /**
     * Open the given bookmarkIds in new tabs.
     *
     * @param bookmarkIds The bookmark ids to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @return Whether the bookmark ids were successfully opened.
     */
    default boolean openBookmarksInNewTabs(List<BookmarkId> bookmarkIds, boolean incognito) {
        return openBookmarksInNewTabs(
                bookmarkIds, incognito, /* tabLaunchType= */ Optional.empty());
    }

    /**
     * Open the given bookmarkIds in new tabs.
     *
     * @param bookmarkIds The bookmark ids to open.
     * @param incognito Whether the bookmarks should be opened in incognito mode.
     * @param tabLaunchType The launch type to use when creating new tabs.
     * @return Whether the bookmark ids were successfully opened.
     */
    boolean openBookmarksInNewTabs(
            List<BookmarkId> bookmarkIds,
            boolean incognito,
            Optional<@TabLaunchType Integer> tabLaunchType);
}
