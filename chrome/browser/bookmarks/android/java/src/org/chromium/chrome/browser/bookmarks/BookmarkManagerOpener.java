// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;

@NullMarked
public interface BookmarkManagerOpener {
    /**
     * Shows the bookmark manager UI.
     *
     * @param activity An activity to start the manager with, if null then the application context
     *     will be used instead.
     * @param profile The profile associated with the bookmarks.
     */
    default void showBookmarkManager(Activity activity, Profile profile) {
        showBookmarkManager(activity, profile, /* folderId= */ null);
    }

    /**
     * Shows the bookmark manager UI.
     *
     * @param activity An activity to start the manager with, if null then the application context
     *     will be used instead.
     * @param profile The profile associated with the bookmarks.
     * @param folderId The bookmark folder to open. If null, the bookmark manager will open the most
     *     recent folder. Can be null.
     */
    void showBookmarkManager(Activity activity, Profile profile, @Nullable BookmarkId folderId);

    /** Returns the last used URL for the bookmarks manager. */
    String getLastUsedUrl();
}
