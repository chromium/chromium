// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.DateDividedAdapter.TimedItem;

import java.util.Arrays;

/** Contains information about a single browsing history item. */
public class HistoryItem extends TimedItem {
    private final String mUrl;
    private final String mDomain;
    private final String mTitle;
    private final boolean mWasBlockedVisit;
    private final long mMostRecentJavaTimestamp;
    private final long[] mNativeTimestampList;
    private Long mStableId;

    private HistoryManager mManager;

    /**
     * @param url The url for this item.
     * @param domain The string to display for the item's domain.
     * @param title The string to display for the item's title.
     * @param mostRecentJavaTimestamp Most recent Java compatible navigation time.
     * @param nativeTimestamps Microsecond resolution navigation times.
     * @param blockedVisit Whether the visit to this item was blocked when it was attempted.
     */
    public HistoryItem(String url, String domain, String title, long mostRecentJavaTimestamp,
            long[] nativeTimestamps, boolean blockedVisit) {
        mUrl = url;
        mDomain = domain;
        mTitle = blockedVisit ? ContextUtils.getApplicationContext().getString(
                R.string.android_history_blocked_site)
                : TextUtils.isEmpty(title) ? url : title;
        mMostRecentJavaTimestamp = mostRecentJavaTimestamp;
        mNativeTimestampList = Arrays.copyOf(nativeTimestamps, nativeTimestamps.length);
        mWasBlockedVisit = blockedVisit;
    }

    /** @return The url for this item. */
    public String getUrl() {
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
     * @param manager The HistoryManager associated with this item.
     */
    public void setHistoryManager(HistoryManager manager) {
        mManager = manager;
    }

    /**
     * Navigates a tab to this item's URL.
     */
    public void open() {
        if (mManager != null) {
            mManager.recordUserActionWithOptionalSearch("OpenItem");
            mManager.recordOpenedItemMetrics(this);
            mManager.openUrl(mUrl, null, false);
        }
    }

    /**
     * Removes this item.
     */
    public void remove() {
        if (mManager != null) {
            mManager.recordUserActionWithOptionalSearch("RemoveItem");
            mManager.removeItem(this);
        }
    }
}
