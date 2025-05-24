// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/** Object defining a partner bookmark. */
@NullMarked
public class PartnerBookmark {
    // To be provided by the bookmark extractors.
    /** Local id of the read bookmark */
    public long mId;

    /** Read id of the parent node */
    public long mParentId;

    /** True if it's folder */
    public boolean mIsFolder;

    /** URL of the bookmark. Required for non-folders. */
    public @Nullable String mUrl;

    @SuppressWarnings("NullAway.Init")
    /* Title of the bookmark. */
    public String mTitle;

    /** .PNG Favicon of the bookmark. Optional. Not used for folders. */
    byte @Nullable [] mFavicon;

    /** .PNG TouchIcon of the bookmark. Optional. Not used for folders. */
    byte @Nullable [] mTouchicon;

    // For auxiliary use while reading.
    /** Native id of the C++-processed bookmark */
    long mNativeId = PartnerBookmarksReader.INVALID_BOOKMARK_ID;

    /** The parent node if any */
    @Nullable PartnerBookmark mParent;

    /** Children nodes for the perfect garbage collection disaster */
    List<PartnerBookmark> mEntries = new ArrayList<>();

    /** Closable iterator for available bookmarks. */
    public interface BookmarkIterator extends Iterator<PartnerBookmark> {
        void close();
    }
}
