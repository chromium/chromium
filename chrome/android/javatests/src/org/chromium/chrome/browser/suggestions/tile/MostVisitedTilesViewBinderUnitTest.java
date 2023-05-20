// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_CONTAINER_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_MULTI_COLUMN_FEED_ON_TABLET_ENABLED;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_MVT_LAYOUT_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.PLACEHOLDER_VIEW;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.UPDATE_INTERVAL_PADDINGS_TABLET;

import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.view.ViewStub;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesViewBinder.ViewHolder;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/**
 * Tests for {@link MostVisitedTilesViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public final class MostVisitedTilesViewBinderUnitTest extends BlankUiTestActivityTestCase {
    private ViewStub mNoMvPlaceholderStub;
    private View mNoMvPlaceholder;
    private LinearLayout mMvTilesContainerLayout;
    private MostVisitedTilesCarouselLayout mMvTilesLayout;
    private TileView mFirstChildView;
    private TileView mSecondChildView;
    private TileView mThirdChildView;

    private PropertyModel mModel;

    @Mock
    private MostVisitedTilesCarouselLayout mMockMvTilesLayout;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMvTilesLayout = new MostVisitedTilesCarouselLayout(getActivity(), null);
            mMvTilesLayout.setId(R.id.mv_tiles_layout);
            mFirstChildView = new TileView(getActivity(), null);
            mSecondChildView = new TileView(getActivity(), null);
            mThirdChildView = new TileView(getActivity(), null);
            mMvTilesLayout.addView(mFirstChildView);
            mMvTilesLayout.addView(mSecondChildView);
            mMvTilesLayout.addView(mThirdChildView);

            mNoMvPlaceholder = new View(getActivity());
            mNoMvPlaceholder.setId(R.id.tile_grid_placeholder);
            mNoMvPlaceholderStub = new ViewStub(getActivity());
            mNoMvPlaceholderStub.setId(R.id.tile_grid_placeholder_stub);
            mNoMvPlaceholderStub.setInflatedId(R.id.tile_grid_placeholder);

            mMvTilesContainerLayout = new LinearLayout(getActivity());
            mMvTilesContainerLayout.addView(mMvTilesLayout);
            mMvTilesContainerLayout.addView(mNoMvPlaceholderStub);
            getActivity().setContentView(mMvTilesContainerLayout);

            mModel = new PropertyModel(MostVisitedTilesProperties.ALL_KEYS);
            PropertyModelChangeProcessor.create(mModel,
                    new ViewHolder(mMvTilesContainerLayout, mMvTilesLayout),
                    MostVisitedTilesViewBinder::bind);
        });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testContainerVisibilitySet() {
        mModel.set(IS_CONTAINER_VISIBLE, true);
        Assert.assertEquals(View.VISIBLE, mMvTilesContainerLayout.getVisibility());

        mModel.set(IS_CONTAINER_VISIBLE, false);
        Assert.assertEquals(View.GONE, mMvTilesContainerLayout.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testMvTilesLayoutAndPlaceholderVisibilitySet() {
        mModel.set(PLACEHOLDER_VIEW, mNoMvPlaceholder);
        Assert.assertNotNull(mModel.get(PLACEHOLDER_VIEW));

        mModel.set(IS_MVT_LAYOUT_VISIBLE, true);
        Assert.assertEquals(View.VISIBLE, mMvTilesLayout.getVisibility());
        Assert.assertEquals(View.GONE, mNoMvPlaceholder.getVisibility());

        mModel.set(IS_MVT_LAYOUT_VISIBLE, false);
        Assert.assertEquals(View.GONE, mMvTilesLayout.getVisibility());
        Assert.assertEquals(View.VISIBLE, mNoMvPlaceholder.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testIntervalPaddingsSet() {
        mModel.set(HORIZONTAL_INTERVAL_PADDINGS, 10);
        MarginLayoutParams params = (MarginLayoutParams) mSecondChildView.getLayoutParams();
        Assert.assertEquals(10, params.leftMargin);
        params = (MarginLayoutParams) mThirdChildView.getLayoutParams();
        Assert.assertEquals(10, params.leftMargin);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testEdgePaddingsSet() {
        mModel.set(HORIZONTAL_EDGE_PADDINGS, 11);
        MarginLayoutParams params = (MarginLayoutParams) mFirstChildView.getLayoutParams();
        Assert.assertEquals(11, params.leftMargin);
        params = (MarginLayoutParams) mThirdChildView.getLayoutParams();
        Assert.assertEquals(11, params.rightMargin);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testIsMultiColumnFeedOnTabletEnabledSet() {
        mModel.set(IS_MULTI_COLUMN_FEED_ON_TABLET_ENABLED, true);
        Assert.assertEquals(true, mMvTilesLayout.getIsMultiColumnFeedOnTabletEnabledForTesting());

        mModel.set(IS_MULTI_COLUMN_FEED_ON_TABLET_ENABLED, false);
        Assert.assertEquals(false, mMvTilesLayout.getIsMultiColumnFeedOnTabletEnabledForTesting());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testUpdateIntervalPaddingsTablet() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMockMvTilesLayout = mock(MostVisitedTilesCarouselLayout.class);
            LinearLayout mvTilesContainerLayout = new LinearLayout(getActivity());
            PropertyModel model = new PropertyModel(MostVisitedTilesProperties.ALL_KEYS);
            PropertyModelChangeProcessor.create(model,
                    new ViewHolder(mvTilesContainerLayout, mMockMvTilesLayout),
                    MostVisitedTilesViewBinder::bind);

            model.set(UPDATE_INTERVAL_PADDINGS_TABLET, true);
            verify(mMockMvTilesLayout, times(1)).updateIntervalPaddingsTablet(true);

            model.set(UPDATE_INTERVAL_PADDINGS_TABLET, true);
            verify(mMockMvTilesLayout, times(2)).updateIntervalPaddingsTablet(true);

            model.set(UPDATE_INTERVAL_PADDINGS_TABLET, false);
            verify(mMockMvTilesLayout, times(1)).updateIntervalPaddingsTablet(false);
        });
    }
}
