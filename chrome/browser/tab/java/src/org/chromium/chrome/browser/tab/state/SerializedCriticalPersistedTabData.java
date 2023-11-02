// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabUserAgent;
import org.chromium.chrome.browser.tab.WebContentsState;

/**
 * Contains serialized {@link CriticalPersistedTabData}
 * TODO(crbug.com/1294620) rename to CriticalPersistedTabDataResult
 */
public class SerializedCriticalPersistedTabData implements PersistedTabDataResult {
    private final int mParentId;
    private final int mRootId;
    private final long mTimestampMillis;
    private final WebContentsState mWebContentsState;
    private final String mOpenerAppId;
    private final int mWebContentsStateVersion;
    private final int mThemeColor;
    private final @Nullable @TabLaunchType Integer mLaunchTypeAtCreation;
    private final @TabUserAgent int mUserAgent;

    /**
     * @param parentId parent identifier for a {@link Tab}
     * @param rootId root identifier for a {@link Tab}
     * @param timestampMillis time {@link Tab} was last accessed
     * @param webContentsState {@link WebContentsState} for the {@link Tab}
     * @param url url the {@link Tab} is currently visiting
     * @param title title of the {@link Tab}
     * @param openerAppId opener app id for the {@link Tab}
     * @param webContentsStateVersion version of the {@link WebContentsState}
     * @param themeColor theme color of the {@link Tab}
     * @param launchTypeAtCreation the way the {@link Tab} was launched
     * @param userAgent user agent for the {@link Tab}
     */
    protected SerializedCriticalPersistedTabData(int parentId, int rootId, long timestampMillis,
            WebContentsState webContentsState, String openerAppId, int webContentsStateVersion,
            int themeColor, @Nullable @TabLaunchType Integer launchTypeAtCreation,
            @TabUserAgent int userAgent) {
        mParentId = parentId;
        mRootId = rootId;
        mTimestampMillis = timestampMillis;
        mWebContentsState = webContentsState;
        mOpenerAppId = openerAppId;
        mWebContentsStateVersion = webContentsStateVersion;
        mThemeColor = themeColor;
        mLaunchTypeAtCreation = launchTypeAtCreation;
        mUserAgent = userAgent;
    }

    protected int getParentId() {
        return mParentId;
    }

    protected int getRootId() {
        return mRootId;
    }

    protected long getTimestampMillis() {
        return mTimestampMillis;
    }

    protected WebContentsState getWebContentsState() {
        return mWebContentsState;
    }

    protected String getUrl() {
        return mWebContentsState.getVirtualUrlFromState();
    }

    protected String getTitle() {
        return mWebContentsState.getDisplayTitleFromState();
    }

    protected String getOpenerAppId() {
        return mOpenerAppId;
    }

    protected int getWebContentsStateVersion() {
        return mWebContentsStateVersion;
    }

    protected int getThemeColor() {
        return mThemeColor;
    }

    protected @Nullable @TabLaunchType Integer getLaunchType() {
        return mLaunchTypeAtCreation;
    }

    protected @TabUserAgent int getUserAgent() {
        return mUserAgent;
    }
}
