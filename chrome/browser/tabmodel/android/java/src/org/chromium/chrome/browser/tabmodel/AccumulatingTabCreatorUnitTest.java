// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateFrozenTabArguments;
import org.chromium.chrome.browser.tabmodel.AccumulatingTabCreator.CreateNewTabArguments;
import org.chromium.content_public.browser.LoadUrlParams;

/** Unit tests for {@link AccumulatingTabCreator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AccumulatingTabCreatorUnitTest {

    private @Mock LoadUrlParams mLoadUrlParams;
    private @Mock Tab mTab;
    private @Mock TabState mTabState;

    @Test
    public void testCreateNewTab() {
        AccumulatingTabCreator tabCreator = new AccumulatingTabCreator();
        assertNull(tabCreator.createNewTab(mLoadUrlParams, TabLaunchType.FROM_LINK, mTab));
        assertEquals(1, tabCreator.createNewTabArgumentsList.size());
        CreateNewTabArguments arguments = tabCreator.createNewTabArgumentsList.get(0);
        assertEquals(mLoadUrlParams, arguments.loadUrlParams);
        assertEquals(TabLaunchType.FROM_LINK, arguments.tabLaunchType);
        assertEquals(mTab, arguments.parent);
    }

    @Test
    public void testCreateFrozenTab() {
        AccumulatingTabCreator tabCreator = new AccumulatingTabCreator();
        assertNull(
                tabCreator.createFrozenTab(
                        mTabState, Tab.INVALID_TAB_ID, TabModel.INVALID_TAB_INDEX));
        assertEquals(1, tabCreator.createFrozenTabArgumentsList.size());
        CreateFrozenTabArguments arguments = tabCreator.createFrozenTabArgumentsList.get(0);
        assertEquals(mTabState, arguments.state);
        assertEquals(Tab.INVALID_TAB_ID, arguments.id);
        assertEquals(TabModel.INVALID_TAB_INDEX, arguments.index);
    }
}
