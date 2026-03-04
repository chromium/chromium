// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private PropertyModel mModel;
    private TabBottomSheetMediator mMediator;

    @Mock private CoBrowseViews mCoBrowseViews;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(TabBottomSheetProperties.ALL_KEYS)
                        .with(TabBottomSheetProperties.BOTTOM_SHEET_VIEWS, mCoBrowseViews)
                        .build();
        mMediator = new TabBottomSheetMediator(mModel, mCoBrowseViews);
    }

    @Test
    public void testOnSheetOffsetChanged_FullHeight() {
        when(mCoBrowseViews.getThinWebViewHeight()).thenReturn(1000);
        when(mCoBrowseViews.getFuseboxHeight()).thenReturn(200);
        when(mCoBrowseViews.getToolbarHeight()).thenReturn(100);

        // totalHeight = 500 (partially open)
        // webUi = 500 - 200 - 100 = 200.
        // inset = 1000 - 500 = 500.
        mMediator.onSheetOffsetChanged(500f);

        Assert.assertEquals(1000, (int) mModel.get(TabBottomSheetProperties.THIN_WEB_VIEW_HEIGHT));
        Assert.assertEquals(
                200, (int) mModel.get(TabBottomSheetProperties.WEB_UI_CONTAINER_HEIGHT));
        Assert.assertEquals(
                500, (int) mModel.get(TabBottomSheetProperties.THIN_WEB_VIEW_INSET_BOTTOM));
    }

    @Test
    public void testOnSheetOffsetChanged_ZeroHeight() {
        when(mCoBrowseViews.getThinWebViewHeight()).thenReturn(1000);
        when(mCoBrowseViews.getFuseboxHeight()).thenReturn(200);
        when(mCoBrowseViews.getToolbarHeight()).thenReturn(100);

        mMediator.onSheetOffsetChanged(0f);

        Assert.assertEquals(1000, (int) mModel.get(TabBottomSheetProperties.THIN_WEB_VIEW_HEIGHT));
        Assert.assertEquals(0, (int) mModel.get(TabBottomSheetProperties.WEB_UI_CONTAINER_HEIGHT));
        Assert.assertEquals(
                1000, (int) mModel.get(TabBottomSheetProperties.THIN_WEB_VIEW_INSET_BOTTOM));
    }

    @Test
    public void testOnSheetOffsetChanged_ClampNegativeWebUi() {
        when(mCoBrowseViews.getThinWebViewHeight()).thenReturn(1000);
        when(mCoBrowseViews.getFuseboxHeight()).thenReturn(200);
        when(mCoBrowseViews.getToolbarHeight()).thenReturn(100);

        // totalHeight = 100 (less than fusebox + toolbar)
        // webUi = 100 - 200 - 100 = -200 -> clamped to 0.
        mMediator.onSheetOffsetChanged(100f);

        Assert.assertEquals(0, (int) mModel.get(TabBottomSheetProperties.WEB_UI_CONTAINER_HEIGHT));
    }

    @Test
    public void testOnSheetOffsetChanged_ClampNegativeInset() {
        when(mCoBrowseViews.getThinWebViewHeight()).thenReturn(1000);
        when(mCoBrowseViews.getFuseboxHeight()).thenReturn(200);
        when(mCoBrowseViews.getToolbarHeight()).thenReturn(100);

        // Sheet is taller than ThinWebView (e.g. 1200)
        mMediator.onSheetOffsetChanged(1200f);

        // webUi = 1200 - 200 - 100 = 900.
        Assert.assertEquals(
                900, (int) mModel.get(TabBottomSheetProperties.WEB_UI_CONTAINER_HEIGHT));
        // inset = 1000 - 1200 = -200 -> clamped to 0.
        Assert.assertEquals(
                0, (int) mModel.get(TabBottomSheetProperties.THIN_WEB_VIEW_INSET_BOTTOM));
    }
}
