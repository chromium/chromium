// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_CONTAINER_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_MVT_LAYOUT_VISIBLE;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.PLACEHOLDER_VIEW;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewStub;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Tests for {@link MostVisitedTilesMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MostVisitedMediatorUnitTest {
    @Mock Resources mResources;
    @Mock Configuration mConfiguration;
    @Mock UiConfig mUiConfig;
    @Mock DisplayMetrics mDisplayMetrics;
    @Mock MostVisitedTilesLayout mMvTilesLayout;
    @Mock ViewStub mNoMvPlaceholderStub;
    @Mock View mNoMvPlaceholder;
    @Mock Tile mTile;
    @Mock SuggestionsTileView mTileView;
    @Mock SiteSuggestion mData;
    @Mock TileRenderer mTileRenderer;
    @Mock SuggestionsUiDelegate mSuggestionsUiDelegate;
    @Mock ContextMenuManager mContextMenuManager;
    @Mock TileGroup.Delegate mTileGroupDelegate;
    @Mock OfflinePageBridge mOfflinePageBridge;
    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private Runnable mSnapshotTileGridChangedRunnable;
    @Mock private Runnable mTileCountChangedRunnable;

    private FakeMostVisitedSites mMostVisitedSites;
    private PropertyModel mModel;
    private MostVisitedTilesMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel(MostVisitedTilesProperties.ALL_KEYS);
        when(mResources.getConfiguration()).thenReturn(mConfiguration);
        mDisplayMetrics.widthPixels = 1000;
        when(mResources.getDisplayMetrics()).thenReturn(mDisplayMetrics);
        when(mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait))
                .thenReturn(12);
        when(mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape)).thenReturn(16);
        when(mResources.getDimensionPixelOffset(R.dimen.tile_view_width)).thenReturn(80);

        when(mUiConfig.getCurrentDisplayStyle())
                .thenReturn(
                        new DisplayStyle(
                                HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR));

        when(mTileView.getData()).thenReturn(mData);
        when(mTile.getData()).thenReturn(mData);

        mMostVisitedSites = new FakeMostVisitedSites();
        doAnswer(
                        invocation -> {
                            mMostVisitedSites.setObserver(
                                    invocation.getArgument(0), invocation.<Integer>getArgument(1));
                            return null;
                        })
                .when(mTileGroupDelegate)
                .setMostVisitedSitesObserver(any(MostVisitedSites.Observer.class), anyInt());

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
    }

    @Test
    public void testOnTileDataChanged() {
        createMediator();
        mMediator.onTileDataChanged();

        verify(mTileRenderer, atLeastOnce())
                .renderTileSection(anyList(), eq(mMvTilesLayout), any());
        verify(mMvTilesLayout).addView(any());
        verify(mSnapshotTileGridChangedRunnable, atLeastOnce()).run();
    }

    @Test
    public void testOnTileCountChanged() {
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);
        ArrayList<SiteSuggestion> array = new ArrayList<>();
        array.add(mData);
        mMostVisitedSites.setTileSuggestions(array);

        createMediator();

        Assert.assertFalse(mMediator.isMVTilesCleanedUp());
        Assert.assertTrue(mModel.get(IS_MVT_LAYOUT_VISIBLE));
        Assert.assertNull(mModel.get(PLACEHOLDER_VIEW));

        // When there's no mv tile and the default search engine doesn't have logo, the placeholder
        // should be shown and the mv tiles layout should be hidden.
        mMostVisitedSites.setTileSuggestions(new ArrayList<>());

        mMediator.onTileCountChanged();

        Assert.assertFalse(mModel.get(IS_MVT_LAYOUT_VISIBLE));
        Assert.assertNotNull(mModel.get(PLACEHOLDER_VIEW));
        verify(mTileCountChangedRunnable, atLeastOnce()).run();

        // When there is mv tile and the default search engine doesn't have logo, the placeholder
        // should be hidden and the mv tiles layout should be shown.
        mMostVisitedSites.setTileSuggestions(JUnitTestGURLs.HTTP_URL.getSpec());

        mMediator.onTileCountChanged();

        Assert.assertTrue(mModel.get(IS_MVT_LAYOUT_VISIBLE));
        Assert.assertNotNull(mModel.get(PLACEHOLDER_VIEW));
    }

    @Test
    public void testMvtContainerOnTileCountChanged() {
        ArrayList<SiteSuggestion> array = new ArrayList<>();
        array.add(mData);
        mMostVisitedSites.setTileSuggestions(array);
        createMediator();

        Assert.assertTrue(mModel.get(IS_CONTAINER_VISIBLE));

        // When there's no mv tile, the mv tiles container should be hidden.
        mMostVisitedSites.setTileSuggestions(new ArrayList<>());
        mMediator.onTileCountChanged();
        Assert.assertFalse(mModel.get(IS_CONTAINER_VISIBLE));

        // When there is mv tile, the mv tiles container should be shown.
        mMostVisitedSites.setTileSuggestions(JUnitTestGURLs.HTTP_URL.getSpec());
        mMediator.onTileCountChanged();
        Assert.assertTrue(mModel.get(IS_CONTAINER_VISIBLE));
    }

    @Test
    public void testOnTileIconChanged() {
        createMediator();
        mMediator.onTileIconChanged(mTile);

        verify(mTileView).renderIcon(mTile);
        verify(mSnapshotTileGridChangedRunnable, atLeastOnce()).run();
    }

    @Test
    public void testOnTileOfflineBadgeVisibilityChanged() {
        createMediator();
        mMediator.onTileOfflineBadgeVisibilityChanged(mTile);

        verify(mTileView).renderOfflineBadge(mTile);
        verify(mSnapshotTileGridChangedRunnable, atLeastOnce()).run();
    }

    @Test
    public void testOnTemplateURLServiceChanged() {
        mMostVisitedSites.setTileSuggestions(new ArrayList<>());
        createMediator();

        Assert.assertTrue(mModel.get(IS_MVT_LAYOUT_VISIBLE));
        Assert.assertNull(mModel.get(PLACEHOLDER_VIEW));

        // When the default search engine has logo and there's no mv tile, the placeholder
        // should be hidden and the mv tiles layout should be shown.
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(false);

        mMediator.onTemplateURLServiceChanged();

        Assert.assertFalse(mModel.get(IS_MVT_LAYOUT_VISIBLE));
        Assert.assertNotNull(mModel.get(PLACEHOLDER_VIEW));

        // When the default search engine doesn't have logo and there's no mv tile, the placeholder
        // should be shown and the mv tiles layout should be hidden.
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);

        mMediator.onTemplateURLServiceChanged();

        Assert.assertTrue(mModel.get(IS_MVT_LAYOUT_VISIBLE));
        Assert.assertNotNull(mModel.get(PLACEHOLDER_VIEW));
    }

    @Test
    public void testSetPortraitPaddings_NotSmallDevice() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        createMediator();
        mMediator.onTileDataChanged();

        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait),
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
        int tileViewWidth = mResources.getDimensionPixelOffset(R.dimen.tile_view_width);
        Assert.assertEquals(
                (int)
                        ((mDisplayMetrics.widthPixels
                                        - mModel.get(HORIZONTAL_EDGE_PADDINGS)
                                        - tileViewWidth * 4.5)
                                / 4),
                (int) mModel.get(HORIZONTAL_INTERVAL_PADDINGS));
    }

    @Test
    public void testSetPortraitPaddings_SmallDevice() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        when(mUiConfig.getCurrentDisplayStyle())
                .thenReturn(
                        new DisplayStyle(HorizontalDisplayStyle.NARROW, VerticalDisplayStyle.FLAT));
        createMediator();
        mMediator.onTileDataChanged();

        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait),
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
        int tileViewWidth = mResources.getDimensionPixelOffset(R.dimen.tile_view_width_condensed);
        Assert.assertEquals(
                Integer.max(
                        0,
                        (int)
                                ((mDisplayMetrics.widthPixels
                                                - mModel.get(HORIZONTAL_EDGE_PADDINGS)
                                                - tileViewWidth * 4.5)
                                        / 4)),
                (int) mModel.get(HORIZONTAL_INTERVAL_PADDINGS));
    }

    @Test
    public void testSetLandscapePaddings() {
        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        createMediator();
        mMediator.onTileDataChanged();

        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
        Assert.assertEquals(
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                (int) mModel.get(HORIZONTAL_INTERVAL_PADDINGS));
    }

    @Test
    public void testDestroy() {
        createMediator();

        mMediator.destroy();

        verify(mMvTilesLayout).destroy();
        verify(mTemplateUrlService).removeObserver(mMediator);
    }

    @Test
    public void testUpdateTilesView_Tablet() {
        int expectedTileViewEdgePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
        int expectedTileViewIntervalPadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_interval_tablet);
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        createMediator(/* isTablet= */ true);
        mMediator.onTileDataChanged();
        Assert.assertEquals(
                "The horizontal edge padding passed to the model is wrong",
                expectedTileViewEdgePadding,
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
        Assert.assertEquals(
                "The horizontal interval padding passed to the model is wrong",
                expectedTileViewIntervalPadding,
                (int) mModel.get(HORIZONTAL_INTERVAL_PADDINGS));

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        createMediator(/* isTablet= */ true);
        mMediator.onTileDataChanged();
        Assert.assertEquals(
                "The horizontal edge padding passed to the model is wrong",
                expectedTileViewEdgePadding,
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
        Assert.assertEquals(
                "The horizontal interval padding passed to the model is wrong",
                expectedTileViewIntervalPadding,
                (int) mModel.get(HORIZONTAL_INTERVAL_PADDINGS));
    }

    @Test
    public void testUpdateTilesView_Phone() {
        mConfiguration.orientation = Configuration.ORIENTATION_PORTRAIT;
        createMediator(/* isTablet= */ false);
        mMediator.onTileDataChanged();
        // tile_view_padding_edge_portrait
        Assert.assertEquals(
                "The horizontal edge padding passed to the model is wrong",
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait),
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));

        mConfiguration.orientation = Configuration.ORIENTATION_LANDSCAPE;
        createMediator(/* isTablet= */ false);
        mMediator.onTileDataChanged();
        Assert.assertEquals(
                "The horizontal edge padding passed to the model is wrong",
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape),
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
    }

    private void createMediator() {
        createMediator(/* isTablet= */ false);
    }

    private void createMediator(boolean isTablet) {
        mMvTilesLayout = Mockito.mock(MostVisitedTilesLayout.class);

        mMvTilesLayout.addView(mTileView);
        when(mMvTilesLayout.getChildCount()).thenReturn(1);
        when(mMvTilesLayout.getChildAt(0)).thenReturn(mTileView);
        when(mNoMvPlaceholderStub.inflate()).thenReturn(mNoMvPlaceholder);

        mMediator =
                new MostVisitedTilesMediator(
                        mResources,
                        mUiConfig,
                        mMvTilesLayout,
                        mNoMvPlaceholderStub,
                        mTileRenderer,
                        mModel,
                        isTablet,
                        mSnapshotTileGridChangedRunnable,
                        mTileCountChangedRunnable);
        mMediator.initWithNative(
                mProfile,
                mSuggestionsUiDelegate,
                mContextMenuManager,
                mTileGroupDelegate,
                mOfflinePageBridge,
                mTileRenderer);
    }
}
