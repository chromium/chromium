// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * Unit tests for the RestoreTabsPromoSheetContent class.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsPromoSheetContentUnitTest {
    @Mock
    private View mContentView;

    private RestoreTabsPromoSheetContent mSheetContent;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSheetContent = new RestoreTabsPromoSheetContent(mContentView);
    }

    @After
    public void tearDown() {
        mSheetContent.destroy();
    }

    @Test
    public void testSheetContent_getContentView() {
        Assert.assertEquals(mContentView, mSheetContent.getContentView());
    }

    @Test
    public void testSheetContent_getToolbarView() {
        Assert.assertNull(mSheetContent.getToolbarView());
    }

    @Test
    public void testSheetContent_getVerticalScrollOffset() {
        Assert.assertEquals(0, mSheetContent.getVerticalScrollOffset());
    }

    @Test
    public void testSheetContent_getPriority() {
        Assert.assertEquals(BottomSheetContent.ContentPriority.HIGH, mSheetContent.getPriority());
    }

    @Test
    public void testSheetContent_getPeekHeight() {
        Assert.assertEquals(BottomSheetContent.HeightMode.DISABLED, mSheetContent.getPeekHeight());
    }

    @Test
    public void testSheetContent_getFullHeightRatio() {
        Assert.assertEquals(BottomSheetContent.HeightMode.WRAP_CONTENT, mSheetContent.getFullHeightRatio(), 0.05);
    }

    @Test
    public void testSheetContent_swipeToDismissEnabled() {
        Assert.assertTrue(mSheetContent.swipeToDismissEnabled());
    }

    @Test
    public void testSheetContent_getSheetContentDescriptionStringId() {
        Assert.assertEquals(R.string.restore_tabs_content_description, mSheetContent.getSheetContentDescriptionStringId());
    }

    @Test
    public void testSheetContent_getSheetClosedAccessibilityStringId() {
        Assert.assertEquals(R.string.restore_tabs_sheet_closed, mSheetContent.getSheetClosedAccessibilityStringId());
    }

    @Test
    public void testSheetContent_getSheetHalfHeightAccessibilityStringId() {
        Assert.assertEquals(R.string.restore_tabs_content_description, mSheetContent.getSheetHalfHeightAccessibilityStringId());
    }

    @Test
    public void testSheetContent_getSheetFullHeightAccessibilityStringId() {
        Assert.assertEquals(R.string.restore_tabs_content_description, mSheetContent.getSheetFullHeightAccessibilityStringId());
    }
}