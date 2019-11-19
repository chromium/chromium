// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.history.HistoryManagerUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHistory;

/**
 * Implementation of {@link NavigationSheet#Delegate} that works with
 * native/rendered pages in tabbed mode. Uses interface methods of {@link Tab}.
 */
public class TabbedSheetDelegate implements NavigationSheet.Delegate {
    private static final int MAXIMUM_HISTORY_ITEMS = 8;
    private static final int FULL_HISTORY_ENTRY_INDEX = -1;

    private final Tab mTab;
    private final String mFullHistoryMenu;

    public TabbedSheetDelegate(Tab tab) {
        mTab = tab;
        mFullHistoryMenu = tab.getActivity().getResources().getString(R.string.show_full_history);
    }

    @Override
    public NavigationHistory getHistory(boolean forward) {
        NavigationHistory history =
                mTab.getWebContents().getNavigationController().getDirectedNavigationHistory(
                        forward, MAXIMUM_HISTORY_ITEMS);
        history.addEntry(new NavigationEntry(FULL_HISTORY_ENTRY_INDEX, UrlConstants.HISTORY_URL,
                null, null, null, mFullHistoryMenu, null, 0, 0));
        return history;
    }

    @Override
    public void navigateToIndex(int index) {
        if (index == FULL_HISTORY_ENTRY_INDEX) {
            HistoryManagerUtils.showHistoryManager(mTab.getActivity(), mTab);
        } else {
            mTab.getWebContents().getNavigationController().goToNavigationIndex(index);
        }
    }
}
