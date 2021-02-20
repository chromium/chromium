// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.url.GURL;

import java.util.Random;

/**
 * Class providing access to functionality provided by the Web Feed native component.
 */
public class WebFeedBridge {
    // TODO(crbug/1152592): remove members needed only for returning mock results.
    private static Random sRandom = new Random();
    private static int sCounter;

    /**
     * Returns a Web Feed identifier if the URL matches a recommended Web Feed.
     * @param url The URL to be checked for being recommended.
     * @return The identifier of the recommended Web Feed in case the URL matches one; null
     * otherwise.
     */
    public @Nullable String getWebFeedIdIfRecommended(GURL url) {
        // TODO(crbug/1152592): replace mock implementation. Current implementation will return
        // stable results for each origin.
        int originHash = url.getOrigin().getSpec().hashCode();
        boolean isRecommended = originHash % 2 == 0;
        return isRecommended ? Integer.toString(originHash) : null;
    }

    /** Container for past visit counts. */
    public static class VisitCounts {
        /** The total number of visits. */
        public final int visits;
        /** The number of per day boolean visits (days when at least one visit happened) */
        public final int dailyVisits;

        VisitCounts(int visits, int dailyVisits) {
            this.visits = visits;
            this.dailyVisits = dailyVisits;
        }
    }

    /**
     * Obtains visit information for a website within a limited number of days in the past.
     * @param url The URL for which the domain will be queried for past visits.
     * @param pastDaysCount The number of past days to consider for querying visits.
     * @param callback The callback to receive the past visits query results.
     */
    public void getVisitsInRecentDays(GURL url, int pastDaysCount, Callback<VisitCounts> callback) {
        // TODO(crbug/1152592): replace mock implementation.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> callback.onResult(new VisitCounts(
                                sRandom.nextInt(100), sRandom.nextInt(pastDaysCount))));
    }

    /** Container for identifiers for a followed Web Feed. */
    public static class FollowedIds {
        /** The follow subscription identifier. */
        public final String followId;
        /** The identifier of the followed Web Feed. */
        public final String webFeedId;

        @VisibleForTesting
        public FollowedIds(String followId, String webFeedId) {
            this.followId = followId;
            this.webFeedId = webFeedId;
        }
    }

    /**
     * Returns the respective identifiers if the provided URL maps to a followed Web Feed.
     * @param url The URL for which the status is being requested.
     * @return A FollowedIds instance in case the URL maps to a followed Web Feed; null otherwise.
     */
    public @Nullable FollowedIds getFollowedIds(GURL url) {
        // TODO(crbug/1152592): replace mock implementation.
        boolean isFollowed = sRandom.nextBoolean();
        FollowedIds ids = null;
        if (isFollowed) {
            ids = new FollowedIds(
                    Long.toString(sRandom.nextLong()), Long.toString(sRandom.nextLong()));
        }
        return ids;
    }

    /** Container for a Web Feed metadata. */
    public static class WebFeedMetadata {
        /** The title of the Web Feed. */
        public final String title;
        /** The main URL for the publisher of the Web Feed. */
        public final GURL publisherUrl;

        WebFeedMetadata(String title, GURL publisherUrl) {
            this.title = title;
            this.publisherUrl = publisherUrl;
        }

        // TODO(crbug/1152592): remove mock implementation.
        private WebFeedMetadata() {
            sCounter += 1;
            this.title = "Title #" + sCounter;
            this.publisherUrl = new GURL("https://publisher-url-" + sCounter + ".com");
        }
    }

    /**
     * Returns Web Feed metadata respective to the provided identifier. The callback will receive
     * `null` if no matching recommended or followed Web Feed is found.
     * @param webFeedId The idenfitier of the Web Feed.
     * @param callback The callback to receive the Web Feed metadata.
     */
    public void getWebFeedMetadata(String webFeedId, Callback<WebFeedMetadata> callback) {
        // TODO(crbug/1152592): replace mock implementation.
        boolean hasMetadata = sRandom.nextBoolean();
        WebFeedMetadata metadata = hasMetadata ? new WebFeedMetadata() : null;
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(metadata));
    }

    /** Container for results from a follow request. */
    public static class FollowResults {
        // TODO(crbug/1152592): replace `boolean success` with `FollowUrlResponseStatus status`.
        /** `true` if the follow request was successful. */
        public final boolean success;
        /** The follow subscription identifier. `null` if the operation was not successful. */
        public final @Nullable String followId;
        /**
         * `true` if the subscribed Web Feed has content available for fetching. Irrelevant if the
         * operation was not successful.
         */
        public final boolean hasContentAvailable;
        /** The metadata from the followed Web Feed. `null` if the operation was not successful. */
        public final @Nullable WebFeedMetadata metadata;

        FollowResults(boolean success, String followId, boolean hasContentAvailable,
                WebFeedMetadata metadata) {
            this.success = success;
            this.followId = followId;
            this.hasContentAvailable = hasContentAvailable;
            this.metadata = metadata;
        }

        // TODO(crbug/1152592): remove mock implementation.
        FollowResults(boolean success) {
            this(success, success ? Long.toString(sRandom.nextLong()) : null,
                    success ? sRandom.nextBoolean() : false,
                    success ? new WebFeedMetadata() : null);
        }
    }

    /**
     * Requests to follow of the most relevant Web Feed represented by the provided URL.
     * @param url The URL that indicates the Web Feed to be followed.
     * @param callback The callback to receive the follow results.
     */
    public void followFromUrl(GURL url, Callback<FollowResults> callback) {
        // TODO(crbug/1152592): replace mock implementation.
        followFromId(null, callback);
    }

    /**
     * Requests to follow of the Web Feed represented by the provided identifier.
     * @param webFeedId The identifier of the Web Feed to be followed.
     * @param callback The callback to receive the follow results.
     */
    public void followFromId(String webFeedId, Callback<FollowResults> callback) {
        // TODO(crbug/1152592): replace mock implementation.
        boolean success = sRandom.nextFloat() > 0.1;
        PostTask.postTask(
                UiThreadTaskTraits.DEFAULT, () -> callback.onResult(new FollowResults(success)));
    }

    /**
     * Requests the unfollowing of the Web Feed subscription from the provided identifier.
     * TODO(crbug/1152592): replace `boolean success` with `UnfollowUrlResponseStatus status`.
     * @param followId The follow subscription identifier.
     * @param callback The callback to receive the unfollow success/failure result (`true`/`false`
     *         respectively).
     */
    public void unfollow(String followId, Callback<Boolean> callback) {
        // TODO(crbug/1152592): replace mock implementation.
        boolean success = sRandom.nextBoolean();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> callback.onResult(success));
    }

    /**
     * Returns true if the user has subscribed to a new Web Feed and, since then, new content has
     * been fetched in the background and the user hasn’t seen it yet.
     * @return True if new content is available after the last follow.
     */
    public boolean hasUnreadArticlesSinceLastFollow() {
        // TODO(crbug/1152592): replace mock implementation.
        return sRandom.nextBoolean();
    }
}
