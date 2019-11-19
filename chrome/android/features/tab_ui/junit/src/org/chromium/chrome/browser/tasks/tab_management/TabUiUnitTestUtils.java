// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import org.chromium.chrome.browser.tab.Tab;

/**
 * This is a util class for TabUi unit tests.
 */
// TODO(crbug.com/1023701): Generalize all prepareTab method from tab_ui/junit directory.
public class TabUiUnitTestUtils {
    public static Tab prepareTab() {
        return mock(Tab.class);
    }

    public static Tab prepareTab(int tabId) {
        Tab tab = prepareTab();
        doReturn(tabId).when(tab).getId();
        return tab;
    }

    public static Tab prepareTab(int tabId, int rootId) {
        Tab tab = prepareTab(tabId);
        doReturn(rootId).when(tab).getRootId();
        return tab;
    }
}
