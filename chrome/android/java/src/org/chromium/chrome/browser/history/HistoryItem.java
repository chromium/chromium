// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.DateDividedAdapter.TimedItem;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** Contains information about a single browsing history item. */
@NullMarked
public class HistoryItem extends TimedItem {
    private final GURL mUrl;
    private final String mDomain;
    private final String mTitle;
    private final @Nullable String mAppId;
    private final boolean mWasBlockedVisit;
    private final boolean mIsActorVisit;
    private final long mMostRecentJavaTimestamp;
    private final long[] mNativeTimestampList;
    private @Nullable Long mStableId;
    private final @Nullable List<HistoryItem> mSubItems;
    private final boolean mIsExpanded;
    private final boolean mIsClusterHead;
    private final @Nullable Long mClusterId;

    private @Nullable HistoryContentManager mManager;

    /**
     * @param url The url for this item.
     * @param domain The string to display for the item's domain.
     * @param title The string to display for the item's title.
     * @param appId ID of the app that this item was generated for. {@code null} if this is
     *     generated for BrApp, or the app can't be identified.
     * @param mostRecentJavaTimestamp Most recent Java compatible navigation time.
     * @param nativeTimestamps Microsecond resolution navigation times.
     * @param blockedVisit Whether the visit to this item was blocked when it was attempted.
     * @param isActorVisit Whether the visit is actor initiated.
     */
    public HistoryItem(
            GURL url,
            String domain,
            String title,
            @Nullable String appId,
            long mostRecentJavaTimestamp,
            long[] nativeTimestamps,
            boolean blockedVisit,
            boolean isActorVisit) {
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
        mIsActorVisit = isActorVisit;
        mSubItems = null;
        mIsExpanded = false;
        mIsClusterHead = false;
        mClusterId = null;
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
    public @Nullable String getAppId() {
        return mAppId;
    }

    /** @return Whether the visit to this item was blocked when it was attempted. */
    public boolean wasBlockedVisit() {
        return mWasBlockedVisit;
    }

    /**
     * @return Whether the visit is actor initiated.
     */
    public boolean isActorVisit() {
        return mIsActorVisit;
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
            if (isClusterHead()) {
                // For cluster heads, the stable ID is derived directly and purely from its unique
                // cluster ID, making it entirely invariant to the addition/removal of sub-items.
                assert mClusterId != null : "Cluster heads must have a cluster ID.";
                assumeNonNull(mClusterId);
                mStableId = mClusterId;
            } else {
                // Generate a stable ID that combines the timestamp and the URL.
                mStableId = (long) mUrl.hashCode();
                mStableId = (mStableId << 32) + (getTimestamp() & 0x0FFFFFFFF);
            }
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

    /**
     * @return The ID of the cluster this item belongs to.
     */
    public @Nullable Long getClusterId() {
        return mClusterId;
    }

    /**
     * @return The list of sub-items for this clustered item.
     */
    public @Nullable List<HistoryItem> getSubItems() {
        return mSubItems;
    }

    /**
     * @return Whether the clustered item is expanded.
     */
    public boolean isExpanded() {
        return mIsExpanded;
    }

    /**
     * @return Whether this item is a cluster head.
     */
    public boolean isClusterHead() {
        return mIsClusterHead;
    }

    /**
     * @return A Builder to create a new mutated HistoryItem from this one.
     */
    public Builder toBuilder() {
        return new Builder(this);
    }

    /** Builder class to support final fields for clustered items. */
    public static class Builder {
        private final GURL mUrl;
        private final String mDomain;
        private final @Nullable String mAppId;
        private final long mMostRecentJavaTimestamp;
        private final long[] mNativeTimestampList;
        private final boolean mWasBlockedVisit;
        private final boolean mIsActorVisit;
        private final @Nullable HistoryContentManager mManager;

        private String mTitle;
        private boolean mIsExpanded;
        private boolean mIsClusterHead;
        private @Nullable Long mClusterId;
        private @Nullable List<HistoryItem> mSubItems;

        private Builder(HistoryItem item) {
            mUrl = item.getUrl();
            mDomain = item.getDomain();
            mTitle = item.getTitle();
            mAppId = item.getAppId();
            mMostRecentJavaTimestamp = item.getTimestamp();
            mNativeTimestampList = item.getNativeTimestamps();
            mWasBlockedVisit = item.wasBlockedVisit();
            mIsActorVisit = item.isActorVisit();
            mManager = item.mManager;

            mIsExpanded = item.isExpanded();
            mIsClusterHead = item.isClusterHead();
            mClusterId = item.getClusterId();
            mSubItems = item.getSubItems();
        }

        public Builder setTitle(String title) {
            mTitle = title;
            return this;
        }

        public Builder setIsExpanded(boolean isExpanded) {
            mIsExpanded = isExpanded;
            return this;
        }

        public Builder setIsClusterHead(boolean isClusterHead) {
            mIsClusterHead = isClusterHead;
            return this;
        }

        public Builder setClusterId(@Nullable Long clusterId) {
            mClusterId = clusterId;
            return this;
        }

        public Builder setSubItems(@Nullable List<HistoryItem> subItems) {
            mSubItems = subItems;
            return this;
        }

        public HistoryItem build() {
            return new HistoryItem(this);
        }
    }

    private HistoryItem(Builder builder) {
        mUrl = builder.mUrl;
        mDomain = builder.mDomain;
        mTitle = builder.mTitle;
        mAppId = builder.mAppId;
        mMostRecentJavaTimestamp = builder.mMostRecentJavaTimestamp;
        mNativeTimestampList = builder.mNativeTimestampList;
        mWasBlockedVisit = builder.mWasBlockedVisit;
        mIsActorVisit = builder.mIsActorVisit;
        mManager = builder.mManager;

        mIsExpanded = builder.mIsExpanded;
        mIsClusterHead = builder.mIsClusterHead;
        mClusterId = builder.mClusterId;
        mSubItems = builder.mSubItems;
    }
}
