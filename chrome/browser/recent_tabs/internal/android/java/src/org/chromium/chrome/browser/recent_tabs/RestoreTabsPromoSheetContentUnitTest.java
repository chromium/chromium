// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import android.view.View;
import android.widget.ScrollView;

import androidx.recyclerview.widget.RecyclerView;

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
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for the RestoreTabsPromoSheetContent class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsPromoSheetContentUnitTest {
    @Mock private View mContentView;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private RecyclerView mRecyclerView;
    @Mock private View mChildView;
    @Mock private ScrollView mScrollView;

    private RestoreTabsPromoSheetContent mSheetContent;
    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mSheetContent =
                new RestoreTabsPromoSheetContent(mContentView, mModel, mBottomSheetController);
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
    public void testSheetContent_getVerticalScrollOffsetDefault() {
        Assert.assertEquals(0, mSheetContent.getVerticalScrollOffset());
    }

    @Test
    public void testSheetContent_getVerticalScrollOffsetRecyclerView() {
        mSheetContent.setRecyclerViewForTesting(mRecyclerView);
        mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);
        when(mRecyclerView.getChildAt(0)).thenReturn(mChildView);
        when(mChildView.getTop()).thenReturn(0);
        when(mRecyclerView.getPaddingTop()).thenReturn(1);
        Assert.assertEquals(1, mSheetContent.getVerticalScrollOffset());
        mSheetContent.setRecyclerViewForTesting(null);
    }

    @Test
    public void testSheetContent_getVerticalScrollOffsetScrollView() {
        mSheetContent.setScrollViewForTesting(mScrollView);
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);
        when(mScrollView.getScrollY()).thenReturn(1);
        Assert.assertEquals(1, mSheetContent.getVerticalScrollOffset());
        mSheetContent.setScrollViewForTesting(null);
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
        Assert.assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                mSheetContent.getFullHeightRatio(),
                0.05);
    }

    @Test
    public void testSheetContent_handleBackPressDeviceScreen() {
        mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);
        Assert.assertTrue(mSheetContent.handleBackPress());
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), HOME_SCREEN);
    }

    @Test
    public void testSheetContent_handleBackPressReviewTabsScreen() {
        mModel.set(CURRENT_SCREEN, REVIEW_TABS_SCREEN);
        Assert.assertTrue(mSheetContent.handleBackPress());
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), HOME_SCREEN);
    }

    @Test
    public void testSheetContent_handleBackPressHomeScreen() {
        RestoreTabsMetricsHelper.setPromoShownCount(1);
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);
        Assert.assertTrue(mSheetContent.handleBackPress());
        Assert.assertFalse(mModel.get(VISIBLE));
        RestoreTabsMetricsHelper.setPromoShownCount(0);
    }

    @Test
    public void testSheetContent_handleBackPressUninitializedScreen() {
        Assert.assertFalse(mSheetContent.handleBackPress());
    }

    @Test
    public void testSheetContent_onBackPressedDeviceScreen() {
        mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);
        mSheetContent.onBackPressed();
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), HOME_SCREEN);
    }

    @Test
    public void testSheetContent_onBackPressedReviewTabsScreen() {
        mModel.set(CURRENT_SCREEN, REVIEW_TABS_SCREEN);
        mSheetContent.onBackPressed();
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), HOME_SCREEN);
    }

    @Test
    public void testSheetContent_onBackPressedHomeScreen() {
        RestoreTabsMetricsHelper.setPromoShownCount(1);
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);
        mSheetContent.onBackPressed();
        Assert.assertFalse(mModel.get(VISIBLE));
        RestoreTabsMetricsHelper.setPromoShownCount(0);
    }

    @Test
    public void testSheetContent_onBackPressedUninitializedScreen() {
        mSheetContent.onBackPressed();
    }

    @Test
    public void testSheetContent_swipeToDismissEnabled() {
        Assert.assertTrue(mSheetContent.swipeToDismissEnabled());
    }

    @Test
    public void testSheetContent_getSheetContentDescriptionStringId() {
        Assert.assertEquals(
                R.string.restore_tabs_content_description,
                mSheetContent.getSheetContentDescriptionStringId());
    }

    @Test
    public void testSheetContent_getSheetClosedAccessibilityStringId() {
        Assert.assertEquals(
                R.string.restore_tabs_sheet_closed,
                mSheetContent.getSheetClosedAccessibilityStringId());
    }

    @Test
    public void testSheetContent_getSheetHalfHeightAccessibilityStringId() {
        Assert.assertEquals(
                R.string.restore_tabs_content_description,
                mSheetContent.getSheetHalfHeightAccessibilityStringId());
    }

    @Test
    public void testSheetContent_getSheetFullHeightAccessibilityStringId() {
        Assert.assertEquals(
                R.string.restore_tabs_content_description,
                mSheetContent.getSheetFullHeightAccessibilityStringId());
    }
}
