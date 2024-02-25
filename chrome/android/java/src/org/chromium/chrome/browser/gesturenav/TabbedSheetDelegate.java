// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.url.GURL;

import java.util.function.Consumer;

/**
 * Implementation of {@link NavigationSheet#Delegate} that works with
 * native/rendered pages in tabbed mode. Uses interface methods of {@link Tab}.
 */
public class TabbedSheetDelegate implements NavigationSheet.Delegate {
    private static final int MAXIMUM_HISTORY_ITEMS = 8;
    private static final int FULL_HISTORY_ENTRY_INDEX = -1;

    private final Tab mTab;
    private final Consumer<Tab> mShowHistoryManager;
    private final String mFullHistoryMenu;

    public TabbedSheetDelegate(Tab tab, Consumer<Tab> showHistoryManager, String historyMenu) {
        mTab = tab;
        mShowHistoryManager = showHistoryManager;
        mFullHistoryMenu = historyMenu;
    }

    @Override
    public NavigationHistory getHistory(boolean forward, boolean isOffTheRecord) {
        NavigationHistory history =
                mTab.getWebContents()
                        .getNavigationController()
                        .getDirectedNavigationHistory(forward, MAXIMUM_HISTORY_ITEMS);
        if (!isOffTheRecord) {
            history.addEntry(
                    new NavigationEntry(
                            FULL_HISTORY_ENTRY_INDEX,
                            new GURL(UrlConstants.HISTORY_URL),
                            GURL.emptyGURL(),
                            GURL.emptyGURL(),
                            mFullHistoryMenu,
                            null,
                            0,
                            0,
                            /* isInitialEntry= */ false));
        }
        return history;
    }

    @Override
    public void navigateToIndex(int index) {
        if (index == FULL_HISTORY_ENTRY_INDEX) {
            mShowHistoryManager.accept(mTab);
        } else {
            mTab.getWebContents().getNavigationController().goToNavigationIndex(index);
        }
    }
}
