// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link TabBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private View mView;
    private PropertyModel mModel;
    private TabBottomSheetMediator mMediator;

    @Mock private CoBrowseViews mCoBrowseViews;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mView = new FrameLayout(mContext);
        when(mCoBrowseViews.getView()).thenReturn(mView);

        mModel = TabBottomSheetProperties.createDefaultModel(mCoBrowseViews);
        mMediator = new TabBottomSheetMediator(mContext, mModel);
    }

    @Test
    @SmallTest
    public void testOnSheetStateChanged_expanded() {
        mMediator.onSheetStateChanged(BottomSheetController.SheetState.FULL);
        assertEquals(BottomSheetController.SheetState.FULL, mMediator.getSheetStateForTesting());
    }

    @Test
    @SmallTest
    public void testSetMaxSheetHeight_setsSheetHeight() {
        int maxHeight = 1000;
        int expectedHeight = Math.round(maxHeight * mMediator.getFullHeightRatioForTesting());
        mMediator.setMaxSheetHeight(maxHeight);
        assertEquals(expectedHeight, mModel.get(TabBottomSheetProperties.SHEET_HEIGHT));
    }
}
