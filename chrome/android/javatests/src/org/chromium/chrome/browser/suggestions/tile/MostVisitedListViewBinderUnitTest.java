// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.LEFT_RIGHT_MARGINS;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/**
 * Tests for {@link MostVisitedListViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public final class MostVisitedListViewBinderUnitTest extends BlankUiTestActivityTestCase {
    private MvTilesLayout mView;
    private TileView mFirstChildView;
    private TileView mSecondChildView;
    private TileView mThirdChildView;

    private PropertyModel mModel;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mView = new MvTilesLayout(getActivity(), null);
            mFirstChildView = new TileView(getActivity(), null);
            mSecondChildView = new TileView(getActivity(), null);
            mThirdChildView = new TileView(getActivity(), null);
            mView.addView(mFirstChildView);
            mView.addView(mSecondChildView);
            mView.addView(mThirdChildView);
            getActivity().setContentView(mView);

            mModel = new PropertyModel(MostVisitedListProperties.ALL_KEYS);
            PropertyModelChangeProcessor.create(mModel, mView, MostVisitedListViewBinder::bind);
        });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testVisibilitySet() {
        mModel.set(IS_VISIBLE, true);
        Assert.assertEquals(View.VISIBLE, mView.getVisibility());

        mModel.set(IS_VISIBLE, false);
        Assert.assertEquals(View.GONE, mView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testIntervalPaddingsSet() {
        mModel.set(INTERVAL_PADDINGS, 10);
        MarginLayoutParams params = (MarginLayoutParams) mSecondChildView.getLayoutParams();
        Assert.assertEquals(10, params.leftMargin);
        params = (MarginLayoutParams) mThirdChildView.getLayoutParams();
        Assert.assertEquals(10, params.leftMargin);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testEdgePaddingsSet() {
        mModel.set(EDGE_PADDINGS, 11);
        MarginLayoutParams params = (MarginLayoutParams) mFirstChildView.getLayoutParams();
        Assert.assertEquals(11, params.leftMargin);
        params = (MarginLayoutParams) mThirdChildView.getLayoutParams();
        Assert.assertEquals(11, params.rightMargin);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testLeftRightMarginsSet() {
        mModel.set(LEFT_RIGHT_MARGINS, 12);
        ViewGroup parentView = (ViewGroup) mView.getParent();
        MarginLayoutParams params = (MarginLayoutParams) parentView.getLayoutParams();
        Assert.assertEquals(12, params.leftMargin);
        Assert.assertEquals(12, params.rightMargin);
    }
}
