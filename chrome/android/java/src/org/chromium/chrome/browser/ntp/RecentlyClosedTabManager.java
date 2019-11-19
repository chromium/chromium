// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * Manages a list of recently closed tabs.
 */
public interface RecentlyClosedTabManager {
    /**
     * Sets the {@link Runnable} to be called whenever the list of recently closed tabs changes.
     * @param runnable The {@link Runnable} to be called, or null.
     */
    void setTabsUpdatedRunnable(@Nullable Runnable runnable);

    /**
     * @param maxTabCount The maximum number of recently closed tabs to return.
     * @return A snapshot of the list of recently closed tabs, with up to maxTabCount elements.
     */
    List<RecentlyClosedTab> getRecentlyClosedTabs(int maxTabCount);

    /**
     * Opens a recently closed tab in the current tab or a new tab. If opened in the current tab,
     * the current tab's entire history is replaced.
     *
     * @param tab The current Tab.
     * @param recentTab The RecentlyClosedTab to open.
     * @param windowOpenDisposition The WindowOpenDisposition value specifying whether to open in
     *         the current tab or a new tab.
     * @return Whether the tab was successfully opened.
     */
    boolean openRecentlyClosedTab(Tab tab, RecentlyClosedTab recentTab, int windowOpenDisposition);

    /**
     * Opens the most recently closed tab in a new tab.
     */
    void openRecentlyClosedTab();

    /**
     * Clears all recently closed tabs.
     */
    void clearRecentlyClosedTabs();

    /**
     * To be called before this instance is abandoned to the garbage collector so it can do any
     * necessary cleanups. This instance must not be used after this method is called.
     */
    void destroy();
}
