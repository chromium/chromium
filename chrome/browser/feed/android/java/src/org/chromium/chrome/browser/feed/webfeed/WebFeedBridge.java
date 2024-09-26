// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.webfeed;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Class providing access to functionality provided by the Web Feed native component. */
@JNINamespace("feed")
public class WebFeedBridge {
    // Values from web_feeds.proto:
    public static final int CHANGE_REASON_WEB_PAGE_MENU = 1;
    public static final int CHANGE_REASON_WEB_PAGE_ACCELERATOR = 2;
    public static final int CHANGE_REASON_MANAGEMENT = 3;
    public static final int CHANGE_REASON_RECOMMENDATION_WEB_PAGE_ACCELERATOR = 6;
    public static final int CHANGE_REASON_SINGLE_WEB_FEED = 7;

    // Access to JNI test hooks for other libraries. This can go away once more Feed code is
    // migrated to chrome/browser/feed.
    public static org.jni_zero.JniStaticTestMocker<WebFeedBridge.Natives> getTestHooksForTesting() {
        return WebFeedBridgeJni.TEST_HOOKS;
    }

    private WebFeedBridge() {}

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
    public static void getVisitCountsToHost(GURL url, Callback<VisitCounts> callback) {
        WebFeedBridgeJni.get()
                .getRecentVisitCountsToHost(
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
        public final @WebFeedAvailabilityStatus int availabilityStatus;

        /** Whether the web feed is recommended. */
        public final boolean isRecommended;

        /** Favicon URL for the Web Feed, if one is provided. */
        public final GURL faviconUrl;

        @CalledByNative("WebFeedMetadata")
        public WebFeedMetadata(
                byte[] id,
                String title,
                GURL visitUrl,
                @WebFeedSubscriptionStatus int subscriptionStatus,
                @WebFeedAvailabilityStatus int availabilityStatus,
                boolean isRecommended,
                GURL faviconUrl) {
            this.id = id;
            this.title = title;
            this.visitUrl = visitUrl;
            this.subscriptionStatus = subscriptionStatus;
            this.availabilityStatus = availabilityStatus;
            this.isRecommended = isRecommended;
            this.faviconUrl = faviconUrl;
        }
    }

    /**
     * Returns the Web Feed metadata for the web feed associated with this page. May return a
     * subscribed, recently subscribed, or recommended Web Feed.
     * @param tab The tab showing the page.
     * @param url The URL for which the status is being requested.
     * @param reason The reason why the information is being requested.
     * @param callback The callback to receive the Web Feed metadata, or null if it is not found.
     */
    public static void getWebFeedMetadataForPage(
            Tab tab,
            GURL url,
            @WebFeedPageInformationRequestReason int reason,
            Callback<WebFeedMetadata> callback) {
        WebFeedBridgeJni.get()
                .findWebFeedInfoForPage(new WebFeedPageInformation(url, tab), reason, callback);
    }

    /**
     * Returns the Web Feed id for the web feed associated with this page.
     * @param url The URL for which the status is being requested.
     * @param callback The callback to receive the Web Feed metadata, or null if it is not found.
     */
    public static void queryWebFeed(String url, Callback<QueryResult> callback) {
        WebFeedBridgeJni.get().queryWebFeed(url, callback);
    }

    /**
     * Returns the Web Feed id for the web feed associated with this page.
     *
     * @param id The URL for which the status is being requested.
     * @param callback The callback to receive the Web Feed metadata, or null if it is not found.
     */
    public static void queryWebFeedId(String id, Callback<QueryResult> callback) {
        WebFeedBridgeJni.get().queryWebFeedId(id, callback);
    }

    /**
     * Returns Web Feed metadata respective to the provided identifier. The callback will receive
     * `null` if no matching recommended or followed Web Feed is found.
     * @param webFeedId The idenfitier of the Web Feed.
     * @param callback The callback to receive the Web Feed metadata, or null if it is not found.
     */
    public static void getWebFeedMetadata(byte[] webFeedId, Callback<WebFeedMetadata> callback) {
        WebFeedBridgeJni.get().findWebFeedInfoForWebFeedId(webFeedId, callback);
    }

    /**
     * Fetches the list of followed Web Feeds.
     * @param callback The callback to receive the list of followed Web Feeds.
     */
    public static void getAllFollowedWebFeeds(Callback<List<WebFeedMetadata>> callback) {
        WebFeedBridgeJni.get()
                .getAllSubscriptions(
                        (Object[] webFeeds) -> {
                            ArrayList<WebFeedMetadata> list = new ArrayList<>();
                            for (Object o : webFeeds) {
                                list.add((WebFeedMetadata) o);
                            }
                            callback.onResult(list);
                        });
    }

    /**
     * Refreshes the list of followed web feeds from the server. See
     * `WebFeedSubscriptions.RefreshSubscriptions`.
     */
    public static void refreshFollowedWebFeeds(Callback<Boolean> callback) {
        WebFeedBridgeJni.get().refreshSubscriptions(callback);
    }

    /**
     * Refreshes the list of recommended web feeds from the server. See
     * `WebFeedSubscriptions.RefreshRecommendedFeeds`.
     */
    public static void refreshRecommendedFeeds(Callback<Boolean> callback) {
        WebFeedBridgeJni.get().refreshRecommendedFeeds(callback);
    }

    /** Increase the count of the number of times the user has followed from the web page menu. */
    public static void incrementFollowedFromWebPageMenuCount() {
        WebFeedBridgeJni.get().incrementFollowedFromWebPageMenuCount();
    }

    /** Container for results from a follow request. */
    public static class FollowResults {
        /** Status of follow request. */
        public final @WebFeedSubscriptionRequestStatus int requestStatus;

        /** The metadata from the followed Web Feed. `null` if the operation was not successful. */
        public final @Nullable WebFeedMetadata metadata;

        @CalledByNative("FollowResults")
        public FollowResults(
                @WebFeedSubscriptionRequestStatus int requestStatus, WebFeedMetadata metadata) {
            this.requestStatus = requestStatus;
            this.metadata = metadata;
        }
    }

    /** Container for results from an Unfollow request. */
    public static class UnfollowResults {
        @CalledByNative("UnfollowResults")
        public UnfollowResults(@WebFeedSubscriptionRequestStatus int requestStatus) {
            this.requestStatus = requestStatus;
        }

        // Result of the operation.
        public final @WebFeedSubscriptionRequestStatus int requestStatus;
    }

    /** Container for results from an QueryWebFeed request. */
    public static class QueryResult {
        @CalledByNative("QueryResult")
        public QueryResult(String webFeedId, String title, String url) {
            this.webFeedId = webFeedId;
            this.title = title;
            this.url = url;
        }

        // Result of the operation.
        public final String webFeedId;
        public final String title;
        public final String url;
    }

    public static boolean isCormorantEnabledForLocale() {
        return WebFeedBridgeJni.get().isCormorantEnabledForLocale();
    }

    public static boolean isWebFeedEnabled() {
        return WebFeedBridgeJni.get().isWebFeedEnabled();
    }

    /**
     * Requests to follow of the most relevant Web Feed represented by the provided URL.
     * @param tab The tab with the loaded page that should be followed.
     * @param url The URL that indicates the Web Feed to be followed.
     * @param webFeedChangeReason The reason for this change, a WebFeedChangeReason value.
     * @param callback The callback to receive the follow results.
     */
    public static void followFromUrl(
            Tab tab, GURL url, int webFeedChangeReason, Callback<FollowResults> callback) {
        WebFeedBridgeJni.get()
                .followWebFeed(new WebFeedPageInformation(url, tab), webFeedChangeReason, callback);
    }

    /**
     * Requests to follow of the Web Feed represented by the provided identifier.
     * @param webFeedId The identifier of the Web Feed to be followed.
     * @param isDurable Whether the request should be retried if it initially fails.
     * @param webFeedChangeReason The reason for this change, a WebFeedChangeReason value.
     * @param callback The callback to receive the follow results.
     */
    public static void followFromId(
            byte[] webFeedId,
            boolean isDurable,
            int webFeedChangeReason,
            Callback<FollowResults> callback) {
        WebFeedBridgeJni.get()
                .followWebFeedById(webFeedId, isDurable, webFeedChangeReason, callback);
    }

    /**
     * Requests the unfollowing of the Web Feed subscription from the provided identifier.
     * @param webFeedId The Web Feed identifier.
     * @param isDurable Whether the request should be retried if it initially fails.
     * @param webFeedChangeReason The reason for this change, a WebFeedChangeReason value.
     * @param callback The callback to receive the unfollow result.
     */
    public static void unfollow(
            byte[] webFeedId,
            boolean isDurable,
            int webFeedChangeReason,
            Callback<UnfollowResults> callback) {
        WebFeedBridgeJni.get().unfollowWebFeed(webFeedId, isDurable, webFeedChangeReason, callback);
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

    /** Information about a web page, which may be used to identify an associated Web Feed. */
    public static class WebFeedPageInformation {
        /** The URL of the page. */
        public final GURL mUrl;

        /** The tab hosting the page. */
        public final Tab mTab;

        WebFeedPageInformation(GURL url, Tab tab) {
            mUrl = url;
            mTab = tab;
        }

        @CalledByNative("WebFeedPageInformation")
        GURL getUrl() {
            return mUrl;
        }

        @CalledByNative("WebFeedPageInformation")
        Tab getTab() {
            return mTab;
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        void followWebFeed(
                WebFeedPageInformation pageInfo,
                int webFeedChangeReason,
                Callback<FollowResults> callback);

        void followWebFeedById(
                byte[] webFeedId,
                boolean isDurable,
                int webFeedChangeReason,
                Callback<FollowResults> callback);

        void unfollowWebFeed(
                byte[] webFeedId,
                boolean isDurable,
                int webFeedChangeReason,
                Callback<UnfollowResults> callback);

        void findWebFeedInfoForPage(
                WebFeedPageInformation pageInfo,
                @WebFeedPageInformationRequestReason int reason,
                Callback<WebFeedMetadata> callback);

        void findWebFeedInfoForWebFeedId(byte[] webFeedId, Callback<WebFeedMetadata> callback);

        void getAllSubscriptions(Callback<Object[]> callback);

        void refreshSubscriptions(Callback<Boolean> callback);

        void refreshRecommendedFeeds(Callback<Boolean> callback);

        void getRecentVisitCountsToHost(GURL url, Callback<int[]> callback);

        void incrementFollowedFromWebPageMenuCount();

        void queryWebFeed(String url, Callback<QueryResult> callback);

        void queryWebFeedId(String id, Callback<QueryResult> callback);

        boolean isCormorantEnabledForLocale();

        boolean isWebFeedEnabled();
    }
}
