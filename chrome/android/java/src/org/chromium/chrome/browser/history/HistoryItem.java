// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.TimedItem;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;

import java.util.Arrays;

/** Contains information about a single browsing history item. */
public class HistoryItem extends TimedItem {
    private final GURL mUrl;
    private final String mDomain;
    private final String mTitle;
    private final String mAppId;
    private final boolean mWasBlockedVisit;
    private final long mMostRecentJavaTimestamp;
    private final long[] mNativeTimestampList;
    private Long mStableId;

    private HistoryContentManager mManager;

    /**
     * @param url The url for this item.
     * @param domain The string to display for the item's domain.
     * @param title The string to display for the item's title.
     * @param appId ID of the app that this item was generated for. {@code null} if this is
     *     generated for BrApp, or the app can't be identified.
     * @param mostRecentJavaTimestamp Most recent Java compatible navigation time.
     * @param nativeTimestamps Microsecond resolution navigation times.
     * @param blockedVisit Whether the visit to this item was blocked when it was attempted.
     */
    public HistoryItem(
            GURL url,
            String domain,
            String title,
            String appId,
            long mostRecentJavaTimestamp,
            long[] nativeTimestamps,
            boolean blockedVisit) {
        mUrl = url;
        mDomain = domain;
        mTitle =
                blockedVisit
                        ? ContextUtils.getApplicationContext()
                                .getString(R.string.android_history_blocked_site)
                        : TextUtils.isEmpty(title) ? url.getSpec() : title;
        mAppId = appId;
        mMostRecentJavaTimestamp = mostRecentJavaTimestamp;
        mNativeTimestampList = Arrays.copyOf(nativeTimestamps, nativeTimestamps.length);
        mWasBlockedVisit = blockedVisit;
    }

    /** @return The url for this item. */
    public GURL getUrl() {
        return mUrl;
    }

    /** @return The string to display for the item's domain. */
    public String getDomain() {
        return mDomain;
    }

    /** @return The string to display for the item's title. */
    public String getTitle() {
        return mTitle;
    }

    /**
     * @return The app ID associated with the history item. Can be {@code null} on BrApp, or if app
     *     can't be identified.
     */
    public String getAppId() {
        return mAppId;
    }

    /** @return Whether the visit to this item was blocked when it was attempted. */
    public Boolean wasBlockedVisit() {
        return mWasBlockedVisit;
    }

    @Override
    public long getTimestamp() {
        return mMostRecentJavaTimestamp;
    }

    /**
     * @return An array of timestamps representing visits to this item's url that matches the
     * resolution used in native code.
     */
    public long[] getNativeTimestamps() {
        return Arrays.copyOf(mNativeTimestampList, mNativeTimestampList.length);
    }

    @Override
    public long getStableId() {
        if (mStableId == null) {
            // Generate a stable ID that combines the timestamp and the URL.
            mStableId = (long) mUrl.hashCode();
            mStableId = (mStableId << 32) + (getTimestamp() & 0x0FFFFFFFF);
        }
        return mStableId;
    }

    /**
     * @param manager The HistoryContentManager associated with this item.
     */
    public void setHistoryManager(HistoryContentManager manager) {
        mManager = manager;
    }

    /** Notifies when a history item was clicked. */
    public void onItemClicked() {
        if (mManager != null) {
            mManager.onItemClicked(this);
        }
    }

    /** Removes this item. */
    public void onItemRemoved() {
        if (mManager != null) {
            mManager.onItemRemoved(this);
        }
    }

    /**
     * Given a URL, returns a large icon for that URL if one is available.
     * @param desiredSizePx The desired size of the icon in pixels.
     * @param callback The method to call asynchronously when the result is available. This callback
     *                 will not be called if this method returns false.
     */
    void getLargeIconForUrl(int desiredSizePx, final LargeIconCallback callback) {
        if (mManager == null || mManager.getLargeIconBridge() == null) return;

        mManager.getLargeIconBridge().getLargeIconForUrl(getUrl(), desiredSizePx, callback);
    }
}
