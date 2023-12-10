// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.List;

/** Manages a list of recently closed tabs. */
public interface RecentlyClosedTabManager {
    /**
     * Sets the {@link Runnable} to be called whenever the list of recently closed entries changes.
     * @param runnable The {@link Runnable} to be called, or null.
     */
    void setEntriesUpdatedRunnable(@Nullable Runnable runnable);

    /**
     * @param maxEntryCount The maximum number of recently closed entries to return.
     * @return A snapshot of the list of recently closed entries, with up to maxEntryCount elements.
     */
    List<RecentlyClosedEntry> getRecentlyClosedEntries(int maxEntryCount);

    /**
     * Opens a recently closed tab in the current tab or a new tab. If opened in the current tab,
     * the current tab's entire history is replaced.
     *
     * @param tabModel The {@link TabModel} to open the tab into.
     * @param recentTab The RecentlyClosedTab to open.
     * @param windowOpenDisposition The WindowOpenDisposition value specifying whether to open in
     *         the current tab or a new tab. The current tab is found in native.
     * @return Whether the tab was successfully opened.
     */
    boolean openRecentlyClosedTab(
            TabModel tabModel, RecentlyClosedTab recentTab, int windowOpenDisposition);

    /**
     * Opens a recently closed entry in new tab(s).
     *
     * @param tabModel The {@link TabModel} to open the tab into.
     * @param recentEntry The RecentlyClosedEntry to open.
     * @return Whether the tab was successfully opened.
     */
    boolean openRecentlyClosedEntry(TabModel tabModel, RecentlyClosedEntry recentEntry);

    /**
     * Opens the most recently closed entry in new tab(s).
     *
     * @param tabModel The {@link TabModel} to open the tab(s) into.
     */
    void openMostRecentlyClosedEntry(TabModel tabModel);

    /** Clears all recently closed tabs. */
    void clearRecentlyClosedEntries();

    /**
     * To be called before this instance is abandoned to the garbage collector so it can do any
     * necessary cleanups. This instance must not be used after this method is called.
     */
    void destroy();
}
