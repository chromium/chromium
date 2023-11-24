// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

/** Simple object representing an offline page. */
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

    /** @return URL of the offline page. */
    public String getUrl() {
        return mUrl;
    }

    /** @return offline id for this offline page. */
    public long getOfflineId() {
        return mOfflineId;
    }

    /** @return Client Id related to the offline page. */
    public ClientId getClientId() {
        return mClientId;
    }

    /** @return Title of the page. */
    public String getTitle() {
        return mTitle;
    }

    /** @return File Path to the offline copy of the page. */
    public String getFilePath() {
        return mFilePath;
    }

    /** @return Size of the offline copy of the page. */
    public long getFileSize() {
        return mFileSize;
    }

    /** @return Time in milliseconds the offline page was created. */
    public long getCreationTimeMs() {
        return mCreationTimeMs;
    }

    /** @return Number of times that the offline page has been accessed. */
    public int getAccessCount() {
        return mAccessCount;
    }

    /** @return Last time in milliseconds the offline page has been accessed. */
    public long getLastAccessTimeMs() {
        return mLastAccessTimeMs;
    }

    /** @return The originating application of the request. */
    public String getRequestOrigin() {
        return mRequestOrigin;
    }
}
