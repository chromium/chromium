// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.Callback;

import java.util.List;

/** Provides methods needed for querying and managing browsing history. */
public interface HistoryProvider {
    /** Observer to be notified of browsing history events. */
    public interface BrowsingHistoryObserver {
        /**
         * Called after {@link BrowsingHistoryBridge#queryHistory(String, long)} is complete.
         * @param items The items that matched the #queryHistory() parameters.
         * @param hasMorePotentialMatches Whether there are more items that match the query text.
         *                                This will be false once the entire local history database
         *                                and remote web history has been searched.
         */
        void onQueryHistoryComplete(List<HistoryItem> items, boolean hasMorePotentialMatches);

        /**
         * Called when history has been deleted through something other than a call to
         * BrowsingHistoryBridge#removeItems(). For example, if two instances of the history page
         * are open and the user removes items in one instance, the other instance will be notified
         * via this method.
         */
        void onHistoryDeleted();

        /**
         * Called after querying history to indicate whether other forms of browsing history were
         * found.
         *
         * @param hasOtherForms Whether other forms of browsing history were found.
         */
        void hasOtherFormsOfBrowsingData(boolean hasOtherForms);

        /**
         * Called after {@link BrowsingHistoryBridge#getAllAppIds()} is complete.
         *
         * @param items The list of app IDs.
         */
        void onQueryAppsComplete(List<String> items);
    }

    /** Sets the {@link BrowsingHistoryObserver} to be notified of browsing history events. */
    void setObserver(BrowsingHistoryObserver observer);

    /**
     * Query browsing history. Only one query may be in-flight at any time. See
     * BrowsingHistoryService::QueryHistory.
     *
     * @param query The query search text. May be empty.
     * @param appId The package name of the app to filter the query result visited by CCT. Can be
     *     null for the results visited by BrApp.
     */
    void queryHistory(String query, String appId);

    /**
     * Query browsing history for a particular host. Only one query may be in-flight at any time.
     * See BrowsingHistoryService::QueryHistory.
     *
     * @param hostName The host name.
     */
    void queryHistoryForHost(String hostName);

    /**
     * Fetches more results using the previous query's text, only valid to call after queryHistory
     * is called.
     */
    void queryHistoryContinuation();

    /** Fetches all the app IDs used in the database. */
    void queryApps();

    /**
     * Gets the last time any webpage on the given host was visited, excluding the last navigation
     * and with an internal time buffer.
     *
     * @param hostName The hostname of the query.
     * @param callback The Callback to call with the last visit timestamp in milliseconds.
     */
    void getLastVisitToHostBeforeRecentNavigations(String hostName, Callback<Long> callback);

    /**
     * Adds the HistoryItem to the list of items being removed. The removal will not be committed
     * until {@link #removeItems()} is called.
     * @param item The item to mark for removal.
     */
    void markItemForRemoval(HistoryItem item);

    /** Removes all items that have been marked for removal through #markItemForRemoval(). */
    void removeItems();

    /** Destroys the HistoryProvider. */
    void destroy();
}
