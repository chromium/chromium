// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.GURL;

/** This is a util class for TabUi unit tests. */
// TODO(crbug.com/40107134): Generalize all prepareTab method from tab_ui/junit directory.
@SuppressWarnings({"ResultOfMethodCallIgnored"})
public class TabUiUnitTestUtils {

    /** Returns a mocked and initialized tab. */
    public static Tab prepareTab() {
        Tab tab = mock(Tab.class);
        doReturn(true).when(tab).isInitialized();
        return tab;
    }

    /** Returns a mocked and initialized tab with an id. */
    public static Tab prepareTab(int tabId) {
        Tab tab = prepareTab();
        doReturn(tabId).when(tab).getId();
        UserDataHost userDataHost = new UserDataHost();
        doReturn(userDataHost).when(tab).getUserDataHost();
        doReturn(GURL.emptyGURL()).when(tab).getUrl();
        return tab;
    }

    /** Returns a mocked and initialized tab with an id and title. */
    public static Tab prepareTab(int id, String title) {
        Tab tab = prepareTab(id);
        doReturn(title).when(tab).getTitle();
        return tab;
    }

    /** Returns a mocked and initialized tab with an id, title, and url. */
    public static Tab prepareTab(int id, String title, GURL url) {
        Tab tab = prepareTab(id, title);
        doReturn(url).when(tab).getOriginalUrl();
        doReturn(url).when(tab).getUrl();
        return tab;
    }
}
