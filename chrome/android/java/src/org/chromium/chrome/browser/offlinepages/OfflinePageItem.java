// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.build.annotations.NullMarked;

/** Simple object representing an offline page. */
@NullMarked
public class OfflinePageItem {
    private final String mUrl;
    private final long mOfflineId;
    private final ClientId mClientId;
    private final String mTitle;
    private final String mFilePath;
    private final long mFileSize;
    private final long mCreationTimeMs;
    private final int mAccessCount;
    private final long mLastAccessTimeMs;
    private final String mRequestOrigin;

    public OfflinePageItem(
            String url,
            long offlineId,
            String clientNamespace,
            String clientId,
            String title,
            String filePath,
            long fileSize,
            long creationTimeMs,
            int accessCount,
            long lastAccessTimeMs,
            String requestOrigin) {
        mUrl = url;
        mOfflineId = offlineId;
        mClientId = new ClientId(clientNamespace, clientId);
        mTitle = title;
        mFilePath = filePath;
        mFileSize = fileSize;
        mCreationTimeMs = creationTimeMs;
        mAccessCount = accessCount;
        mLastAccessTimeMs = lastAccessTimeMs;
        mRequestOrigin = requestOrigin;
    }

    /** Returns the URL of the offline page. */
    public String getUrl() {
        return mUrl;
    }

    /** Returns the offline id for this offline page. */
    public long getOfflineId() {
        return mOfflineId;
    }

    /** Returns the Client Id related to the offline page. */
    public ClientId getClientId() {
        return mClientId;
    }

    /** Returns the title of the page. */
    public String getTitle() {
        return mTitle;
    }

    /** Returns the file path to the offline copy of the page. */
    public String getFilePath() {
        return mFilePath;
    }

    /** Returns the size of the offline copy of the page. */
    public long getFileSize() {
        return mFileSize;
    }

    /** Returns the time in milliseconds the offline page was created. */
    public long getCreationTimeMs() {
        return mCreationTimeMs;
    }

    /** Returns the number of times that the offline page has been accessed. */
    public int getAccessCount() {
        return mAccessCount;
    }

    /** Returns the last time in milliseconds the offline page has been accessed. */
    public long getLastAccessTimeMs() {
        return mLastAccessTimeMs;
    }

    /** Returns the originating application of the request. */
    public String getRequestOrigin() {
        return mRequestOrigin;
    }
}
