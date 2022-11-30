// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.PersistedTabData;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * This is a util class for TabUi unit tests.
 */
// TODO(crbug.com/1023701): Generalize all prepareTab method from tab_ui/junit directory.
@SuppressWarnings({"DoNotMock", "ResultOfMethodCallIgnored"}) // Mocks GURL
public class TabUiUnitTestUtils {
    public static TabImpl prepareTab() {
        TabImpl tab = mock(TabImpl.class);
        doReturn(true).when(tab).isInitialized();
        return tab;
    }

    public static TabImpl prepareTab(int tabId) {
        TabImpl tab = prepareTab();
        doReturn(tabId).when(tab).getId();
        UserDataHost userDataHost = new UserDataHost();
        doReturn(userDataHost).when(tab).getUserDataHost();
        return tab;
    }

    public static TabImpl prepareTab(int id, String title) {
        return prepareTab(id, title, id);
    }

    public static TabImpl prepareTab(int id, String title, int rootId) {
        TabImpl tab = prepareTab(id, rootId);
        doReturn(title).when(tab).getTitle();
        return tab;
    }

    public static <T extends PersistedTabData> void prepareTab(
            Tab tab, Class<T> clazz, T persistedTabData) {
        UserDataHost userDataHost = new UserDataHost();
        userDataHost.setUserData(clazz, persistedTabData);
        doReturn(userDataHost).when(tab).getUserDataHost();
    }

    public static TabImpl prepareTab(int id, String title, GURL url) {
        CriticalPersistedTabData criticalPersistedTabData = mock(CriticalPersistedTabData.class);
        TabImpl tab = prepareTab(id, criticalPersistedTabData);
        doReturn(id).when(criticalPersistedTabData).getRootId();
        doReturn(title).when(tab).getTitle();
        doReturn(url).when(tab).getOriginalUrl();
        doReturn(url).when(tab).getUrl();
        return tab;
    }

    public static TabImpl prepareTab(int tabId, CriticalPersistedTabData criticalPersistedTabData) {
        TabImpl tab = prepareTab();
        doReturn(tabId).when(tab).getId();
        prepareCriticalPersistedTabData(tab, criticalPersistedTabData);
        return tab;
    }

    private static void prepareCriticalPersistedTabData(
            TabImpl tab, CriticalPersistedTabData criticalPersistedTabData) {
        UserDataHost userDataHost = new UserDataHost();
        userDataHost.setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
        when(tab.getUserDataHost()).thenReturn(userDataHost);
    }

    public static TabImpl prepareTab(int tabId, int rootId) {
        TabImpl tab = prepareTab(tabId);
        UserDataHost userDataHost = new UserDataHost();
        CriticalPersistedTabData criticalPersistedTabData = mock(CriticalPersistedTabData.class);
        userDataHost.setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
        doReturn(userDataHost).when(tab).getUserDataHost();
        doReturn(rootId).when(criticalPersistedTabData).getRootId();
        doReturn(GURL.emptyGURL()).when(tab).getUrl();
        return tab;
    }

    public static TabImpl prepareTab(int tabId, int rootId, String visibleUrl) {
        TabImpl tab = prepareTab(tabId, rootId);
        WebContents webContents = mock(WebContents.class);
        GURL gurl = mock(GURL.class);
        doReturn(visibleUrl).when(gurl).getSpec();
        doReturn(gurl).when(webContents).getVisibleUrl();
        doReturn(webContents).when(tab).getWebContents();
        doReturn(GURL.emptyGURL()).when(tab).getOriginalUrl();
        return tab;
    }
}
