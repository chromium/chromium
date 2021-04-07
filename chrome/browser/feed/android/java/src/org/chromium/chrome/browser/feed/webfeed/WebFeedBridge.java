// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

/**
 * Class providing access to functionality provided by the Web Feed native component.
 */
@JNINamespace("feed")
public class WebFeedBridge {
    // TODO(crbug/1152592): remove members needed only for returning mock results.
    private static Random sRandom = new Random();
    private static int sCounter;

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
     * @param url The URL for which the host will be queried for past visits.
     * @param callback The callback to receive the past visits query results.
     *            Upon failure, VisitCounts is populated with 0 visits.
     */
    public void getVisitCountsToHost(GURL url, Callback<VisitCounts> callback) {
        WebFeedBridgeJni.get().getRecentVisitCountsToHost(
                url, (result) -> callback.onResult(new VisitCounts(result[0], result[1])));
    }

    /** Container for a Web Feed metadata. */
    public static class WebFeedMetadata {
        /** Unique identifier of this web feed. */
        public final byte[] id;
        /** The title of the Web Feed. */
        public final String title;
        /** The URL that best represents this Web Feed. */
        public final GURL visitUrl;
        /** Subscription status */
        public final @WebFeedSubscriptionStatus int subscriptionStatus;
        /** Whether the web feed has content available. */
        public final boolean isActive;
        /** Whether the web feed is recommended. */
        public final boolean isRecommended;

        @CalledByNative("WebFeedMetadata")
        WebFeedMetadata(byte[] id, String title, GURL visitUrl,
                @WebFeedSubscriptionStatus int subscriptionStatus, boolean isActive,
                boolean isRecommended) {
            this.id = id;
            this.title = title;
            this.visitUrl = visitUrl;
            this.subscriptionStatus = subscriptionStatus;
            this.isActive = isActive;
            this.isRecommended = false;
        }

        // TODO(crbug/1152592): remove mock implementation.
        private WebFeedMetadata() {
            sCounter += 1;
            id = ("Id" + sCounter).getBytes();
            title = "Title #" + sCounter;
            visitUrl = new GURL("https://publisher-url-" + sCounter + ".com");
            subscriptionStatus = (sCounter % 2) == 0 ? WebFeedSubscriptionStatus.SUBSCRIBED
                                                     : WebFeedSubscriptionStatus.NOT_SUBSCRIBED;
            isActive = true;
            isRecommended = false;
        }
    }

    /**
     * Returns the Web Feed metadata for the web feed associated with this page. May return a
     * subscribed, recently subscribed, or recommended Web Feed.
     * @param url The URL for which the status is being requested.
     * @param callback The callback to receive the Web Feed metadata, or null if it is not found.
     */
    public void getWebFeedMetadataForPage(GURL url, Callback<WebFeedMetadata> callback) {
        WebFeedBridgeJni.get().findWebFeedInfoForPage(new WebFeedPageInformation(url), callback);
    }

    /**
     * Returns Web Feed metadata respective to the provided identifier. The callback will receive
     * `null` if no matching recommended or followed Web Feed is found.
     * @param webFeedId The idenfitier of the Web Feed.
     * @param callback The callback to receive the Web Feed metadata, or null if it is not found.
     */
    public void getWebFeedMetadata(byte[] webFeedId, Callback<WebFeedMetadata> callback) {
        WebFeedBridgeJni.get().findWebFeedInfoForWebFeedId(webFeedId, callback);
    }

    /**
     * Fetches the list of followed Web Feeds.
     * @param callback The callback to receive the list of followed Web Feeds.
     */
    public void getAllFollowedWebFeeds(Callback<List<WebFeedMetadata>> callback) {
        WebFeedBridgeJni.get().getAllSubscriptions((Object[] webFeeds) -> {
            ArrayList<WebFeedMetadata> list = new ArrayList<>();
            for (Object o : webFeeds) {
                list.add((WebFeedMetadata) o);
            }
            callback.onResult(list);
        });
    }

    /** Container for results from a follow request. */
    public static class FollowResults {
        // TODO(crbug/1152592): replace `boolean success` with `FollowUrlResponseStatus status`.
        /** `true` if the follow request was successful. */
        public final @WebFeedSubscriptionRequestStatus int requestStatus;
        /** The metadata from the followed Web Feed. `null` if the operation was not successful. */
        public final @Nullable WebFeedMetadata metadata;

        @CalledByNative("FollowResults")
        FollowResults(
                @WebFeedSubscriptionRequestStatus int requestStatus, WebFeedMetadata metadata) {
            this.requestStatus = requestStatus;
            this.metadata = metadata;
        }

        // TODO(crbug/1152592): remove mock implementation.
        FollowResults(boolean success) {
            this(success ? WebFeedSubscriptionRequestStatus.SUCCESS
                         : WebFeedSubscriptionRequestStatus.FAILED_UNKNOWN_ERROR,
                    success ? new WebFeedMetadata() : null);
        }
    }

    /** Container for results from an Unfollow request. */
    public static class UnfollowResults {
        @CalledByNative("UnfollowResults")
        UnfollowResults(@WebFeedSubscriptionRequestStatus int requestStatus) {
            this.requestStatus = requestStatus;
        }
        // Result of the operation.
        public final @WebFeedSubscriptionRequestStatus int requestStatus;
    }

    /**
     * Requests to follow of the most relevant Web Feed represented by the provided URL.
     * @param url The URL that indicates the Web Feed to be followed.
     * @param callback The callback to receive the follow results.
     */
    public void followFromUrl(GURL url, Callback<FollowResults> callback) {
        WebFeedBridgeJni.get().followWebFeed(new WebFeedPageInformation(url), callback);
    }
    public void followFromUrlFake(GURL url, Callback<FollowResults> callback) {
        // TODO(crbug/1152592): remove mock implementation.
        followFromIdFake(null, callback);
    }

    /**
     * Requests to follow of the Web Feed represented by the provided identifier.
     * @param webFeedId The identifier of the Web Feed to be followed.
     * @param callback The callback to receive the follow results.
     */
    public void followFromId(byte[] webFeedId, Callback<FollowResults> callback) {
        WebFeedBridgeJni.get().followWebFeedById(webFeedId, callback);
    }
    public void followFromIdFake(byte[] webFeedId, Callback<FollowResults> callback) {
        // TODO(crbug/1152592): remove mock implementation.
        boolean success = sRandom.nextFloat() > 0.1;
        PostTask.postTask(
                UiThreadTaskTraits.DEFAULT, () -> callback.onResult(new FollowResults(success)));
    }
    /**
     * Requests the unfollowing of the Web Feed subscription from the provided identifier.
     * TODO(crbug/1152592): replace `boolean success` with `UnfollowUrlResponseStatus status`.
     * @param webFeedId The Web Feed identifier.
     * @param callback The callback to receive the unfollow success/failure result (`true`/`false`
     *         respectively).
     */
    public void unfollow(byte[] webFeedId, Callback<UnfollowResults> callback) {
        WebFeedBridgeJni.get().unfollowWebFeed(webFeedId, callback);
    }
    public void unfollowFake(byte[] webFeedId, Callback<UnfollowResults> callback) {
        // TODO(crbug/1152592): remove mock implementation.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> callback.onResult(new UnfollowResults(sRandom.nextBoolean()
                                        ? WebFeedSubscriptionRequestStatus.SUCCESS
                                        : WebFeedSubscriptionRequestStatus.FAILED_UNKNOWN_ERROR)));
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

    /** This is deprecated, do not use. */
    @Deprecated
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

    /** This is deprecated, do not use. */
    @Deprecated
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

    static class WebFeedPageInformation {
        final GURL mUrl;
        WebFeedPageInformation(GURL url) {
            mUrl = url;
        }

        @CalledByNative("WebFeedPageInformation")
        GURL getUrl() {
            return mUrl;
        }
    }

    @NativeMethods
    interface Natives {
        void followWebFeed(WebFeedPageInformation pageInfo, Callback<FollowResults> callback);
        void followWebFeedById(byte[] webFeedId, Callback<FollowResults> callback);
        void unfollowWebFeed(byte[] webFeedId, Callback<UnfollowResults> callback);
        void findWebFeedInfoForPage(
                WebFeedPageInformation pageInfo, Callback<WebFeedMetadata> callback);
        void findWebFeedInfoForWebFeedId(byte[] webFeedId, Callback<WebFeedMetadata> callback);
        void getAllSubscriptions(Callback<Object[]> callback);
        void getRecentVisitCountsToHost(GURL url, Callback<int[]> callback);
    }
}
