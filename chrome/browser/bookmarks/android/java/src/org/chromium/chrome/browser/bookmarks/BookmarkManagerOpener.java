// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;

@NullMarked
public interface BookmarkManagerOpener {
    /**
     * Shows the bookmark manager UI.
     *
     * @param activity An activity to start the manager with, if null then the application context
     *                 will be used instead.
     * @param tab      The current tab if it exists.
     * @param profile  The profile associated with the bookmarks.
     */
    default void showBookmarkManager(Activity activity, @Nullable Tab tab, Profile profile) {
        showBookmarkManager(activity, tab, profile, /* folderId= */ null);
    }

    /**
     * Shows the bookmark manager UI.
     *
     * @param activity An activity to start the manager with, if null then the application context
     *                 will be used instead.
     * @param tab      The current tab if it exists.
     * @param profile  The profile associated with the bookmarks.
     * @param folderId The bookmark folder to open. If null, the bookmark manager will open the most
     *                 recent folder. Can be null.
     */
    void showBookmarkManager(Activity activity, @Nullable Tab tab, Profile profile,
            @Nullable BookmarkId folderId);

    /**
     * Starts an {@link BookmarkEditActivity} for the given {@link BookmarkId}.
     *
     * @param activity A context to start the manager with, if null then the application context
     *     will be used instead.
     * @param profile The profile associated with the bookmarks.
     * @param bookmarkId The bookmark to open.
     */
    void startEditActivity(Context context, Profile profile, BookmarkId bookmarkId);

    /**
     * Starts an {@link BookmarkFolderPickerActivity} for the given {@link BookmarkId}s.
     *
     * @param context A context to start the manager with, if null then the application context will
     *     be used instead.
     * @param profile The profile associated with the bookmarks.
     * @param bookmarkIds The bookmarks that are being moved via the picker.
     */
    void startFolderPickerActivity(Context context, Profile profile, BookmarkId... bookmarkIds);

    /** Closes the {@link BookmarkActivity} on Phone. Does nothing on tablet. */
    // TODO(crbug.com/400793505): Remove this function.
    void finishActivityOnPhone(Context context);

    /** Returns the last used URL for the bookmarks manager. */
    String getLastUsedUrl();
}
