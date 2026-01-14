// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;

import android.content.Context;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabBottomSheetToolbarContainer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabBottomSheetToolbarContainerTest {
    private Context mContext;
    private FrameLayout mContainer;
    private TabBottomSheetToolbarContainer mToolbarContainer;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContainer = new FrameLayout(mContext);
        mToolbarContainer = new TabBottomSheetToolbarContainer(mContext, mContainer);
    }

    @After
    public void tearDown() {
        mToolbarContainer = null;
        mContainer = null;
        mContext = null;
    }

    @Test
    public void testSetToolbar_simpleAddsViewAndReturnsToolbar() {
        // Initially no toolbar.
        assertNull(mToolbarContainer.getToolbar());

        mToolbarContainer.setToolbar(TabBottomSheetUtils.TabBottomSheetModes.SIMPLE);

        TabBottomSheetToolbar toolbar = mToolbarContainer.getToolbar();
        assertNotNull(toolbar);
        // The container should have the toolbar view as its child.
        assertEquals(1, mContainer.getChildCount());
        assertSame(mContainer.getChildAt(0), toolbar.getToolbarView());
        // And the implementation should be the simple toolbar.
        assertSame(TabBottomSheetSimpleToolbar.class, toolbar.getClass());
    }
}
