// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp.snippets;

import android.annotation.SuppressLint;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;

import org.chromium.base.DiscardableReferencePool.DiscardableReference;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.suggestions.OfflinableSuggestion;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.Collection;
import java.util.Collections;

/**
 * Represents the data for an article card on the NTP.
 */
public class SnippetArticle
        extends PropertyObservable<PartialBindCallback> implements OfflinableSuggestion {
    /** The category of this article. */
    public final int mCategory;

    /** The identifier for this article within the category - not necessarily unique globally. */
    public final String mIdWithinCategory;

    /** The title of this article. */
    public final String mTitle;

    /** The snippet for this article. */
    public final String mSnippet;

    /** The canonical publisher name (e.g., New York Times). */
    public final String mPublisher;

    /** The URL of this article. This may be an AMP url. */
    public final String mUrl;

    /** The time when this article was published. */
    public final long mPublishTimestampMilliseconds;

    /** The score expressing relative quality of the article for the user. */
    public final float mScore;

    /**
     * The time when the article was fetched from the server. This field is only used for remote
     * suggestions.
     */
    public final long mFetchTimestampMilliseconds;

    /** Whether the snippet has a thumbnail to display. **/
    public final boolean mHasThumbnail;

    /** The flag that indicates whether this is a video suggestion. */
    public boolean mIsVideoSuggestion;

    /** Stores whether any part of this article has been exposed to the user. */
    public boolean mExposed;

    /** Stores whether the user has seen a substantial part of this article. */
    public boolean mSeen;

    /** The rank of this article within its section. */
    private int mPerSectionRank = -1;

    /** The global rank of this article in the complete list. */
    private int mGlobalRank = -1;

    /** The thumbnail, fetched lazily when the RecyclerView wants to show the snippet. */
    private DiscardableReference<Drawable> mThumbnail;

    /**
     * The favicon of the publisher, fetched lazily when the RecyclerView wants to show the snippet.
     */
    private DiscardableReference<Drawable> mPublisherFavicon;

    /** The thumbnail dominant color. */
    private @ColorInt Integer mThumbnailDominantColor;

    /** The tab id of the corresponding tab (only for recent tab articles). */
    private int mRecentTabId;

    /** The offline id of the corresponding offline page, if any. */
    private Long mOfflinePageOfflineId;

    /** Whether the corresponding offline page has been automatically prefetched. */
    private boolean mIsPrefetched;

    /**
     * Creates a SnippetArticleListItem object that will hold the data. Default is to have a
     * thumbnail and empty snippet.
     */
    @SuppressLint("SupportAnnotationUsage") // for ColorInt on an Integer rather than int or long
    public SnippetArticle(int category, String idWithinCategory, String title, String publisher,
            String url, long publishTimestamp, float score, long fetchTimestamp,
            boolean isVideoSuggestion, @ColorInt Integer thumbnailDominantColor) {
        this(category, idWithinCategory, title, "", publisher, url, publishTimestamp, score,
                fetchTimestamp, isVideoSuggestion, thumbnailDominantColor, true);
    }

    /**
     * Creates a SnippetArticleListItem object that will hold the data.
     */
    @SuppressLint("SupportAnnotationUsage") // for ColorInt on an Integer rather than int or long
    public SnippetArticle(int category, String idWithinCategory, String title, String snippet,
            String publisher, String url, long publishTimestamp, float score, long fetchTimestamp,
            boolean isVideoSuggestion, @ColorInt Integer thumbnailDominantColor,
            boolean hasThumbnail) {
        mCategory = category;
        mIdWithinCategory = idWithinCategory;
        mTitle = title;
        mSnippet = snippet;
        mPublisher = publisher;
        mUrl = url;
        mPublishTimestampMilliseconds = publishTimestamp;
        mScore = score;
        mFetchTimestampMilliseconds = fetchTimestamp;
        mIsVideoSuggestion = isVideoSuggestion;
        mThumbnailDominantColor = thumbnailDominantColor;
        mHasThumbnail = hasThumbnail;
    }

    @Override
    public boolean equals(Object other) {
        if (!(other instanceof SnippetArticle)) return false;
        SnippetArticle rhs = (SnippetArticle) other;
        return mCategory == rhs.mCategory && mIdWithinCategory.equals(rhs.mIdWithinCategory);
    }

    @Override
    public int hashCode() {
        return mCategory ^ mIdWithinCategory.hashCode();
    }

    /**
     * Returns this article's thumbnail, or {@code null} if it hasn't been fetched yet or has been
     * discarded.
     */
    @Nullable
    public Drawable getThumbnail() {
        return mThumbnail == null ? null : mThumbnail.get();
    }

    /** Sets the thumbnail bitmap for this article. */
    public void setThumbnail(DiscardableReference<Drawable> thumbnail) {
        mThumbnail = thumbnail;
    }

    /**
     * Clears this article's thumbnail if there is one.
     */
    public void clearThumbnail() {
        mThumbnail = null;
    }

    /**
     * Returns the favicon of the publisher for this article, or {@code null} if it hasn't been
     * fetched yet.
     */
    @Nullable
    public Drawable getPublisherFavicon() {
        return mPublisherFavicon == null ? null : mPublisherFavicon.get();
    }

    /** Sets he favicon of the publisher for this article. */
    public void setPublisherFavicon(DiscardableReference<Drawable> favicon) {
        mPublisherFavicon = favicon;
    }

    /**
     * Returns this article's thumbnail dominant color. Can return {@code null} if there is none.
     */
    @Nullable
    @ColorInt
    public Integer getThumbnailDominantColor() {
        return mThumbnailDominantColor;
    }

    /** @return whether a snippet is a remote suggestion. */
    public boolean isArticle() {
        return mCategory == KnownCategories.ARTICLES;
    }

    @Override
    public boolean requiresExactOfflinePage() {
        return false;
    }

    @Override
    public String getUrl() {
        return mUrl;
    }

    @Override
    public void setOfflinePageOfflineId(@Nullable Long offlineId) {
        mOfflinePageOfflineId = offlineId;
    }

    @Override
    @Nullable
    public Long getOfflinePageOfflineId() {
        return mOfflinePageOfflineId;
    }

    public void setIsPrefetched(boolean isPrefetched) {
        mIsPrefetched = isPrefetched;
    }

    public boolean isPrefetched() {
        return mIsPrefetched;
    }

    @Override
    public String toString() {
        // For debugging purposes. Displays the first 42 characters of the title.
        return String.format("{%s, %1.42s}", getClass().getSimpleName(), mTitle);
    }

    public void setRank(int perSectionRank, int globalRank) {
        mPerSectionRank = perSectionRank;
        mGlobalRank = globalRank;
    }

    public int getGlobalRank() {
        return mGlobalRank;
    }

    public int getPerSectionRank() {
        return mPerSectionRank;
    }

    @Override
    public Collection<PartialBindCallback> getAllSetProperties() {
        return Collections.emptyList();
    }

    @Override
    public Collection<PartialBindCallback> getAllProperties() {
        return Collections.emptyList();
    }
}
