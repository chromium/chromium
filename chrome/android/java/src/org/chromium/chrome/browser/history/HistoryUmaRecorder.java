// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.profile_metrics.BrowserProfileType;

/** UMA/Histogram recorder for history UI. */
public class HistoryUmaRecorder {
    private static final String METRICS_PREFIX = "Android.HistoryPage.";

    protected void recordUserAction(String action) {
        RecordUserAction.record(getPrefix() + action);
    }

    /** Records the given action, prepending "Search." if addSearchPrefix is true. */
    private void recordActionWithCorrectSearchPrefix(boolean addSearchPrefix, String action) {
        StringBuilder name = new StringBuilder();
        if (addSearchPrefix) name.append("Search.");
        name.append(action);
        recordUserAction(name.toString());
    }

    /** Returns the string to be prefixed to the action to build the right name. */
    public String getPrefix() {
        return METRICS_PREFIX;
    }

    /** Record the action that opens history UI surface. */
    public void recordOpenHistory() {
        recordUserAction("Show");
    }

    /** Record the action that taps on search from history. */
    public void recordSearchHistory() {
        recordUserAction("Search");
    }

    /**
     * Record the action that opens a history item.
     *
     * @param isSearching Whether the UI is in search mode.
     */
    public void recordOpenItem(boolean isSearching) {
        recordActionWithCorrectSearchPrefix(isSearching, "OpenItem");
    }

    /**
     * Record the action that removes a history item.
     *
     * @param isSearching Whether the UI is in search mode.
     */
    public void recordRemoveItem(boolean isSearching) {
        recordActionWithCorrectSearchPrefix(isSearching, "RemoveItem");
    }

    /**
     * Record the action that links are selected.
     *
     * @param isSearching Whether the UI is in search mode.
     */
    public void recordSelectionEstablished(boolean isSearching) {
        recordActionWithCorrectSearchPrefix(isSearching, "SelectionEstablished");
    }

    /**
     * Record the action that selected links are deleted.
     *
     * @param isSearching Whether the UI is in search mode.
     */
    public void recordRemoveSelected(boolean isSearching) {
        recordActionWithCorrectSearchPrefix(isSearching, "RemoveSelected");
    }

    /**
     * Record the action that user copies the selected link.
     *
     * @param isSearching Whether the UI is in search mode.
     */
    public void recordCopyLink(boolean isSearching) {
        recordActionWithCorrectSearchPrefix(isSearching, "CopyLink");
    }

    /**
     * Record the action that the selected links are being opened to new tabs.
     *
     * @param isSearching Whether the UI is in search mode.
     * @param isIncognito Whether the action is triggered for incognito tab.
     */
    public void recordOpenInTabs(boolean isSearching, boolean isIncognito) {
        recordActionWithCorrectSearchPrefix(
                isSearching, "OpenSelected" + (isIncognito ? "Incognito" : ""));
    }

    /**
     * Record that action that UI is scrolled to load more entries.
     *
     * @param isSearching Whether the UI is in search mode.
     */
    public void recordLoadMoreOnScroll(boolean isSearching) {
        recordActionWithCorrectSearchPrefix(isSearching, "LoadMoreOnScroll");
    }

    /** Record the action that navigates to Chrome BrApp history from in-app history. */
    public void recordOpenFullHistory() {
        // Do nothing for browser history UI.
    }

    /**
     * Record the action/histogram for clear browsing data.
     *
     * @param isIncognito Whether the action is triggered for incognito tab.
     */
    public void recordClearBrowsingData(boolean isIncognito) {
        recordUserAction("ClearBrowsingData");
        recordClearBrowsingDataMetric(isIncognito);
    }

    private void recordClearBrowsingDataMetric(boolean incognito) {
        @BrowserProfileType
        int type = incognito ? BrowserProfileType.INCOGNITO : BrowserProfileType.REGULAR;
        RecordHistogram.recordEnumeratedHistogram(
                getPrefix() + "ClearBrowsingData.PerProfileType",
                type,
                BrowserProfileType.MAX_VALUE + 1);
    }

    /**
     * Record the time taken to query app list to the local database.
     *
     * @param timeMs Query time.
     */
    public void recordQueryAppDuration(long timeMs) {
        RecordHistogram.recordTimesHistogram("History.QueryAppDuration", timeMs);
    }

    /** Record the user action of opending the app filter sheet. */
    public void recordAppFilterSheetOpened() {
        recordUserAction("OpenAppFilterSheet");
    }
}
