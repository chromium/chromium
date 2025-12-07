// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Represents a recently closed window from MultiInstanceManager. */
@NullMarked
public class RecentlyClosedWindow extends RecentlyClosedEntry {
    private static final String WINDOW_DEFAULT_TITLE = "Window";
    private final int mInstanceId;
    private final int mTabCount;
    private final String mTitle;
    private final String mUrl;

    /**
     * @param timestamp The milliseconds since the Unix Epoch this entry was created.
     * @param instanceId The instance ID of the window.
     * @param url The URL of the active tab.
     * @param title The title of the window.
     * @param tabCount The number of tabs associated with the window.
     */
    public RecentlyClosedWindow(
            long timestamp, int instanceId, String url, @Nullable String title, int tabCount) {
        super(timestamp);
        mUrl = url;
        mTabCount = tabCount;
        mInstanceId = instanceId;
        if (title == null || title.trim().isEmpty()) {
            mTitle = WINDOW_DEFAULT_TITLE;
        } else {
            mTitle = title;
        }
    }

    /** Returns the instance ID of the window. */
    public int getInstanceId() {
        return mInstanceId;
    }

    /** The number of tabs associated with the window. */
    public int getTabCount() {
        return mTabCount;
    }

    /** Returns the title of the window. */
    public String getTitle() {
        return mTitle;
    }

    /** Returns the active tab's url in the window. */
    public String getUrl() {
        return mUrl;
    }
}
