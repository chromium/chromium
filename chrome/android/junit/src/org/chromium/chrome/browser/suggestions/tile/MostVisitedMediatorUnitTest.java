// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedListProperties.LEFT_RIGHT_MARGINS;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.DisplayMetrics;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.ShadowGURL;

/** Tests for {@link MostVisitedListViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class})
public class MostVisitedMediatorUnitTest {
    @Mock
    Resources mResources;
    @Mock
    Configuration mConfiguration;
    @Mock
    DisplayMetrics mDisplayMetrics;
    @Mock
    MvTilesLayout mMvTilesLayout;
    @Mock
    Tile mTile;
    @Mock
    SuggestionsTileView mTileView;
    @Mock
    TileRenderer mTileRenderer;
    @Mock
    SuggestionsUiDelegate mSuggestionsUiDelegate;
    @Mock
    ContextMenuManager mContextMenuManager;
    @Mock
    TileGroup.Delegate mTileGroupDelegate;
    @Mock
    OfflinePageBridge mOfflinePageBridge;

    private FakeMostVisitedSites mMostVisitedSites;
    private PropertyModel mModel;
    private MostVisitedListMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel(MostVisitedListProperties.ALL_KEYS);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        mDisplayMetrics.widthPixels = 1000;
        when(mResources.getDisplayMetrics()).thenReturn(mDisplayMetrics);

        when(mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait))
                .thenReturn(12);
        when(mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape)).thenReturn(16);
        when(mResources.getDimensionPixelOffset(R.dimen.tile_view_width)).thenReturn(80);

        mMvTilesLayout.addView(mTileView);
        when(mMvTilesLayout.getChildCount()).thenReturn(1);
        when(mMvTilesLayout.findTileView(mTile)).thenReturn(mTileView);

        mMostVisitedSites = new FakeMostVisitedSites();
        doAnswer(invocation -> {
            mMostVisitedSites.setObserver(
                    invocation.getArgument(0), invocation.<Integer>getArgument(1));
            return null;
        })
                .when(mTileGroupDelegate)
                .setMostVisitedSitesObserver(any(MostVisitedSites.Observer.class), anyInt());
    }

    @Test
    public void testOnTileDataChanged() {
        createMediator();
        mMediator.onTileDataChanged();

        verify(mMvTilesLayout).addView(any());
    }

    @Test
    public void testOnTileIconChanged() {
        createMediator();
        mMediator.onTileIconChanged(mTile);

        verify(mTileView).renderIcon(mTile);
    }

    @Test
    public void testOnTileOfflineBadgeVisibilityChanged() {
        createMediator();
        mMediator.onTileOfflineBadgeVisibilityChanged(mTile);

        verify(mTileView).renderOfflineBadge(mTile);
    }

    @Test
    public void testSetLeftRightMargins() {
        int parentViewStartMargin = 30;
        createMediator(parentViewStartMargin);

        Assert.assertEquals(-parentViewStartMargin, mModel.get(LEFT_RIGHT_MARGINS));
    }

    @Test
    public void testSetPortraitPaddings() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        createMediator();
        mMediator.onTileDataChanged();

        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait),
                mModel.get(EDGE_PADDINGS));
        Assert.assertEquals(
                (int) ((mDisplayMetrics.widthPixels - mModel.get(EDGE_PADDINGS)
                               - mResources.getDimensionPixelOffset(R.dimen.tile_view_width) * 4.5)
                        / 4),
                mModel.get(INTERVAL_PADDINGS));
    }

    @Test
    public void testSetLandscapePaddings() {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        createMediator();
        mMediator.onTileDataChanged();

        Assert.assertEquals(mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                mModel.get(EDGE_PADDINGS));
        Assert.assertEquals(mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                mModel.get(INTERVAL_PADDINGS));
    }

    private void createMediator() {
        createMediator(/*parentViewStartMargin=*/0);
    }

    private void createMediator(int parentViewStartMargin) {
        mMediator = new MostVisitedListMediator(mResources, mMvTilesLayout, mTileRenderer, mModel,
                false, parentViewStartMargin, false);
        mMediator.initWithNative(mSuggestionsUiDelegate, mContextMenuManager, mTileGroupDelegate,
                mOfflinePageBridge, mTileRenderer);
    }
}
