// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * An interface to be notified about changes to a TabList.
 */
public interface TabInfoObserver {

    /**
     * Called when a tab is selected.
     *
     * @param page The newly selected page.
     * @param type The type of selection.
     * @param lastId The ID of the last selected tab, or {@link Tab#INVALID_PAGE_ID} if no tab was
     *               selected.
     */
    void didSelectTab(ITab tab, @TabSelectionType int type, int lastId);

    /**
     * Called right after {@code tab} has been destroyed.
     *
     * @param tabId The ID of the tab that was destroyed.
     * @param incognito True if the closed tab was incognito.
     */
    void didCloseTab(int tabId, boolean incognito);

    /**
     * Called after a tab has been added to the {@link ITabGroup}.
     *
     * @param pageInfo The newly added page.
     * @param type The type of tab launch.
     */
    void didAddTab(ITab tab, @TabSelectionType int type);

}
