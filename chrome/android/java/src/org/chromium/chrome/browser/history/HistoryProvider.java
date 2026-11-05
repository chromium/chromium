// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;

import java.util.List;
import java.util.Objects;

/** Provides methods needed for querying and managing browsing history. */
@NullMarked
public interface HistoryProvider {
    /** Information about a group of synced client devices that share a name. */
    class ClientGroup {
        public final List<String> clientIds;
        public final String name;

        public ClientGroup(List<String> clientIds, String name) {
            this.clientIds = clientIds;
            this.name = name;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof ClientGroup that)) return false;
            return Objects.equals(clientIds, that.clientIds) && Objects.equals(name, that.name);
        }

        @Override
        public int hashCode() {
            return Objects.hash(clientIds, name);
        }
    }

    /**
     * A predefined time period the user can select to restrict the shown history to. Mirrors {@code
     * history::TimePeriodOption}.
     */
    class TimePeriodOption {
        public final @TimePeriodOptionId int id;

        /** The localized label shown to the user in the filter UI. */
        public final String label;

        /**
         * The start of the time period (inclusive), in milliseconds since the Unix epoch. {@link
         * QueryOptions#UNBOUNDED_TIME_MS} if unbounded.
         */
        public final long beginTimeMs;

        /**
         * The end of the time period (exclusive), in milliseconds since the Unix epoch. {@link
         * QueryOptions#UNBOUNDED_TIME_MS} if unbounded.
         */
        public final long endTimeMs;

        public TimePeriodOption(
                @TimePeriodOptionId int id, String label, long beginTimeMs, long endTimeMs) {
            this.id = id;
            this.label = label;
            this.beginTimeMs = beginTimeMs;
            this.endTimeMs = endTimeMs;
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (!(o instanceof TimePeriodOption that)) return false;
            return id == that.id
                    && Objects.equals(label, that.label)
                    && beginTimeMs == that.beginTimeMs
                    && endTimeMs == that.endTimeMs;
        }

        @Override
        public int hashCode() {
            return Objects.hash(id, label, beginTimeMs, endTimeMs);
        }
    }

    /** Observer to be notified of browsing history events. */
    interface BrowsingHistoryObserver {
        /**
         * Called after {@link BrowsingHistoryBridge#queryHistory(String, long)} is complete.
         *
         * @param items The items that matched the #queryHistory() parameters.
         * @param hasMorePotentialMatches Whether there are more items that match the query text.
         *     This will be false once the entire local history database and remote web history has
         *     been searched.
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

        /**
         * Called after {@link BrowsingHistoryBridge#queryClients()} is complete.
         *
         * @param items The list of synced client device groups.
         */
        default void onQueryClientsComplete(List<ClientGroup> items) {}

        /**
         * Called after {@link BrowsingHistoryBridge#queryTimePeriodOptions()} is complete.
         *
         * @param items The list of predefined time period options. Empty if the time period filter
         *     is disabled.
         */
        default void onQueryTimePeriodOptionsComplete(List<TimePeriodOption> items) {}
    }

    /** Sets the {@link BrowsingHistoryObserver} to be notified of browsing history events. */
    void setObserver(BrowsingHistoryObserver observer);

    /**
     * Query browsing history with default {@link QueryOptions}. Only one query may be in-flight at
     * any time. See BrowsingHistoryService::QueryHistory.
     *
     * @param query The query search text. May be empty.
     */
    default void queryHistory(String query) {
        queryHistory(query, new QueryOptions());
    }

    /**
     * Query browsing history with {@link QueryOptions}. Only one query may be in-flight at any
     * time. See BrowsingHistoryService::QueryHistory.
     *
     * @param query The query search text. May be empty.
     * @param options The {@link QueryOptions} for querying browsing history.
     */
    void queryHistory(String query, QueryOptions options);

    /**
     * Fetches more results using the previous query's text, only valid to call after queryHistory
     * is called.
     */
    void queryHistoryContinuation();

    /** Fetches all the app IDs used in the database. */
    void queryApps();

    /** Fetches all synced client devices. */
    void queryClients();

    /** Fetches the predefined time period options for the time period filter. */
    void queryTimePeriodOptions();

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
