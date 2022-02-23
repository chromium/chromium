// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.suggestions.ImageFetcher;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig.TileStyle;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.url.GURL;

/** A simple test for {@link TileRenderer} using real {@link android.view.View} objects. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TileRendererTest {
    private static final int TITLE_LINES = 1;

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock
    private ImageFetcher mMockImageFetcher;

    @Mock
    private TileGroup.TileSetupDelegate mTileSetupDelegate;

    private LinearLayout mSharedParent;
    private Tile mSharedTile;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(null);
        MockitoAnnotations.initMocks(this);

        mSharedParent = new LinearLayout(mActivityTestRule.getActivity());
        SiteSuggestion siteSuggestion = new SiteSuggestion("Example", GURL.emptyGURL(), 0, 0, 0);
        mSharedTile = new Tile(siteSuggestion, 0);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBuildTestView_Modern() {
        TileRenderer tileRenderer = new TileRenderer(
                mActivityTestRule.getActivity(), TileStyle.MODERN, TITLE_LINES, mMockImageFetcher);

        SuggestionsTileView tileView =
                tileRenderer.buildTileView(mSharedTile, mSharedParent, mTileSetupDelegate);
        Assert.assertNotNull(tileView);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testBuildTileView_ModernCondensed() {
        TileRenderer tileRenderer = new TileRenderer(mActivityTestRule.getActivity(),
                TileStyle.MODERN_CONDENSED, TITLE_LINES, mMockImageFetcher);

        SuggestionsTileView tileView =
                tileRenderer.buildTileView(mSharedTile, mSharedParent, mTileSetupDelegate);
        Assert.assertNotNull(tileView);
    }
}
