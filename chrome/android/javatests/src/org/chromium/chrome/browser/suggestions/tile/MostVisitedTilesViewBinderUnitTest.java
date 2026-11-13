// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.CONTAINER_WRAPS_TILES;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_VISIBLE;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.LinearLayout;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesViewBinder.ViewHolder;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.tile.TileView;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Tests for {@link MostVisitedTilesViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public final class MostVisitedTilesViewBinderUnitTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private LinearLayout mMvTilesContainerLayout;
    private MostVisitedTilesLayout mMvTilesLayout;
    private TileView mFirstChildView;
    private TileView mSecondChildView;
    private TileView mThirdChildView;

    private PropertyModel mModel;
    private ViewHolder mViewHolder;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = sActivityTestRule.getActivity();
                    mMvTilesLayout = new MostVisitedTilesLayout(activity, null);
                    mMvTilesLayout.setId(R.id.mv_tiles_layout);
                    mFirstChildView = new TileView(activity, null);
                    mSecondChildView = new TileView(activity, null);
                    mThirdChildView = new TileView(activity, null);
                    mMvTilesLayout.addView(mFirstChildView);
                    mMvTilesLayout.addView(mSecondChildView);
                    mMvTilesLayout.addView(mThirdChildView);

                    mMvTilesContainerLayout = new LinearLayout(activity);
                    mMvTilesContainerLayout.addView(mMvTilesLayout);
                    // Mirror the beside-MVT row, where the container fills the leftover width.
                    LinearLayout row = new LinearLayout(activity);
                    row.addView(
                            mMvTilesContainerLayout,
                            new LinearLayout.LayoutParams(
                                    /* width= */ 0, LayoutParams.WRAP_CONTENT, /* weight= */ 1f));
                    activity.setContentView(row);

                    mModel = new PropertyModel(MostVisitedTilesProperties.ALL_KEYS);
                    mViewHolder = new ViewHolder(mMvTilesContainerLayout, mMvTilesLayout);
                    PropertyModelChangeProcessor.create(
                            mModel, mViewHolder, MostVisitedTilesViewBinder::bind);
                });
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testContainerVisibilitySet() {
        mModel.set(IS_VISIBLE, true);
        Assert.assertEquals(View.VISIBLE, mMvTilesContainerLayout.getVisibility());

        mModel.set(IS_VISIBLE, false);
        Assert.assertEquals(View.GONE, mMvTilesContainerLayout.getVisibility());
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
    public void testContainerWrapsTiles_True() {
        mModel.set(CONTAINER_WRAPS_TILES, true);

        LinearLayout.LayoutParams params =
                (LinearLayout.LayoutParams) mMvTilesContainerLayout.getLayoutParams();
        Assert.assertEquals(LayoutParams.WRAP_CONTENT, params.width);
        Assert.assertEquals(0f, params.weight, 0f);

        // The tiles keep the fixed tablet edge padding even with plenty of room to spare.
        mMvTilesLayout.updateEdgeMarginLff(/* totalWidth= */ 10000);
        int edgePaddingTablet =
                sActivityTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
        MarginLayoutParams firstChildParams =
                (MarginLayoutParams) mFirstChildView.getLayoutParams();
        Assert.assertEquals(edgePaddingTablet, firstChildParams.getMarginStart());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testContainerWrapsTiles_False() {
        // Bind directly, since setting the default false value doesn't notify the binder.
        mModel.set(CONTAINER_WRAPS_TILES, false);
        MostVisitedTilesViewBinder.bind(mModel, mViewHolder, CONTAINER_WRAPS_TILES);

        LinearLayout.LayoutParams params =
                (LinearLayout.LayoutParams) mMvTilesContainerLayout.getLayoutParams();
        Assert.assertEquals(0, params.width);
        Assert.assertEquals(1f, params.weight, 0f);
    }
}
