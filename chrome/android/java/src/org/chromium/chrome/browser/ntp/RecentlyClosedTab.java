// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.Token;
import org.chromium.url.GURL;

/** Represents a recently closed tab from TabRestoreService. */
public class RecentlyClosedTab extends RecentlyClosedEntry {
    private final String mTitle;
    private final GURL mUrl;
    private final @Nullable Token mTabGroupId;

    /**
     * @param sessionId The unique ID in the tab restore service for the entry.
     * @param title The title of the tab.
     * @param url The URL of the tab.
     * @param tabGroupId The tab group ID the tab had before being closed or null if it was not part
     *     of a group.
     */
    @CalledByNative
    public RecentlyClosedTab(
            int sessionId,
            long timestamp,
            @JniType("std::u16string") String title,
            @JniType("GURL") GURL url,
            @JniType("std::optional<base::Token>") @Nullable Token tabGroupId) {
        super(sessionId, timestamp);
        mTitle = title;
        mUrl = url;
        mTabGroupId = tabGroupId;
    }

    /** Returns the title of tab. */
    public String getTitle() {
        return mTitle;
    }

    /** Returns the URL of the tab. */
    public GURL getUrl() {
        return mUrl;
    }

    /**
     * Returns the tab group ID of the tab or null if the tab is not part of tab group. Useful when
     * displaying groups from a {@link RecentlyClosedBulkEvent}.
     */
    public @Nullable Token getTabGroupId() {
        return mTabGroupId;
    }
}
