// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnerbookmarks;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;

import java.util.HashMap;
import java.util.Map;

/**
 * Stores failed favicon loads along with a timestamp determining what point we can attempt
 * retrieval again, as a way of throttling the number of requests made for previously failed favicon
 * fetch attempts.
 */
public class PartnerBookmarksFaviconThrottle {
    private static final String PREFERENCES_NAME = "partner_bookmarks_favicon_throttle";
    private static final long FAVICON_RETRIEVAL_TIMEOUT_MS = DateUtils.DAY_IN_MILLIS * 30;

    private final SharedPreferences mSharedPreferences;

    private Map<String, Long> mCurrentEntries;
    private Map<String, Long> mNewEntries;

    public PartnerBookmarksFaviconThrottle() {
        this(PREFERENCES_NAME);
    }

    @VisibleForTesting
    PartnerBookmarksFaviconThrottle(String name) {
        mSharedPreferences = ContextUtils.getApplicationContext().getSharedPreferences(name, 0);
        init();
    }

    /**
     * Reads the favicon retrieval timestamp information from our own {@link SharedPreferences}.
     *
     * Suppressing "unchecked" because we're 100% sure we're storing only <String, Long> pairs.
     */
    @SuppressWarnings("unchecked")
    @VisibleForTesting
    void init() {
        mCurrentEntries = (Map<String, Long>) mSharedPreferences.getAll();
        mNewEntries = new HashMap<>();
    }

    /**
     * Writes the new map that was built as a result of the calls to {@link #onFaviconFetched} to
     * disk in our {@link SharedPreferences}. This overwrites previously stored entries.
     */
    public void commit() {
        assert mNewEntries != null;

        // Save ourselves a write to disk if the current entries and new entries are identical.
        if (mCurrentEntries.equals(mNewEntries)) {
            return;
        }

        Editor editor = mSharedPreferences.edit();
        editor.clear();
        for (Map.Entry<String, Long> entry : mNewEntries.entrySet()) {
            editor.putLong(entry.getKey(), entry.getValue());
        }
        editor.apply();
    }

    /**
     * Calling this with each favicon fetch URL and result builds the new output entries to be
     * written to disk when {@link #commit} is called.
     *
     * @param url The page URL we attempted to fetch a favicon for.
     * @param result The {@link FaviconFetchResult} response we got for this URL.
     */
    public void onFaviconFetched(String url, @FaviconFetchResult int result) {
        assert mCurrentEntries != null;
        assert mNewEntries != null;

        if (result == FaviconFetchResult.FAILURE_SERVER_ERROR) {
            mNewEntries.put(url, System.currentTimeMillis() + FAVICON_RETRIEVAL_TIMEOUT_MS);
        } else if (!isSuccessfulFetchResult(result) && !shouldFetchFromServerIfNecessary(url)
                && (System.currentTimeMillis() < mCurrentEntries.get(url))) {
            // Keep storing an entry if it hasn't yet expired and we get didn't just get a success
            // response.
            mNewEntries.put(url, mCurrentEntries.get(url));
        }
    }

    private boolean isSuccessfulFetchResult(@FaviconFetchResult int result) {
        return result == FaviconFetchResult.SUCCESS_FROM_CACHE
                || result == FaviconFetchResult.SUCCESS_FROM_SERVER;
    }

    /**
     * Determines, based on the contents of our entry set, whether or not we should even attempt to
     * reach out to a server to retrieve a favicon that isn't currently in our favicon image cache.
     *
     * @param url The page URL we need a favicon for.
     * @return Whether or not we should fetch the favicon from server if necessary.
     */
    public boolean shouldFetchFromServerIfNecessary(String url) {
        Long expiryTimeMs = getExpiryOf(url);
        return expiryTimeMs == null || System.currentTimeMillis() >= expiryTimeMs;
    }

    /**
     * Gets the expiry time in ms of a particular URL for which we're fetching a favicon. URLs that
     * have previously failed to retrieve a favicon from a server will have a value at which point
     * we should attempt a retrieval again, otherwise we return null for entries not in our set of
     * entries.
     *
     * @param url The page URL we're trying to fetch a favicon for.
     * @return The expiry time of the favicon fetching restriction in milliseconds, if we have a
     *         corresponding entry for this URL.
     */
    private Long getExpiryOf(String url) {
        assert mCurrentEntries != null;

        if (mCurrentEntries.containsKey(url)) {
            return mCurrentEntries.get(url);
        }
        return null;
    }

    /**
     * Called after tests so we don't leave behind test {@link SharedPreferences}, and have data
     * from one test run into another.
     */
    @VisibleForTesting
    void clearEntries() {
        mSharedPreferences.edit().clear().apply();
    }

    /**
     * @return Number of stored entries.
     */
    @VisibleForTesting
    int numEntries() {
        assert mCurrentEntries != null;
        return mCurrentEntries.size();
    }
}
