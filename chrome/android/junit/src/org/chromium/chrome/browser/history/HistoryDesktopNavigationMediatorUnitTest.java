// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.widget.navigation_pane.NavigationPaneProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/** Unit tests for {@link HistoryDesktopNavigationMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HistoryDesktopNavigationMediatorUnitTest {
    private Context mContext;
    private ModelList mModelList;
    private Runnable mRunnable;
    private boolean mRunnableInvoked;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mModelList = new ModelList();
        mRunnable = () -> mRunnableInvoked = true;
    }

    @Test
    public void testPopulatesList() {
        HistoryDesktopNavigationMediator mediator =
                new HistoryDesktopNavigationMediator(mContext, mModelList, mRunnable, mRunnable);

        assertEquals(2, mModelList.size());

        // First item is History folder
        assertEquals(NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM, mModelList.get(0).type);
        assertNotNull(mModelList.get(0).model.get(NavigationPaneProperties.TITLE));
        assertEquals(true, mModelList.get(0).model.get(NavigationPaneProperties.IS_SELECTED));

        // Second item is Clear browsing data folder
        assertEquals(NavigationPaneProperties.ITEM_TYPE_NAVIGATION_ITEM, mModelList.get(1).type);
        assertNotNull(mModelList.get(1).model.get(NavigationPaneProperties.TITLE));
        assertEquals(false, mModelList.get(1).model.get(NavigationPaneProperties.IS_SELECTED));

        mModelList.get(1).model.get(NavigationPaneProperties.ON_CLICK_HANDLER).run();
        assertEquals(true, mRunnableInvoked);
    }
}
