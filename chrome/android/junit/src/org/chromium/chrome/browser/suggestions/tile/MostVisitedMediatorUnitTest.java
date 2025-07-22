// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_EDGE_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.HORIZONTAL_INTERVAL_PADDINGS;
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_CONTAINER_VISIBLE;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.util.DisplayMetrics;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.mostvisited.MostVisitedSites;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Tests for {@link MostVisitedTilesMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MostVisitedMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock Resources mResources;
    @Mock Configuration mConfiguration;
    @Mock UiConfig mUiConfig;
    @Mock DisplayMetrics mDisplayMetrics;
    @Mock MostVisitedTilesLayout mMvTilesLayout;
    @Mock Tile mTile;
    @Mock SuggestionsTileView mTileView;
    @Mock SiteSuggestion mData;
    @Mock TileRenderer mTileRenderer;
    @Mock UserEducationHelper mUserEducationHelper;
    @Mock SuggestionsUiDelegate mSuggestionsUiDelegate;
    @Mock ContextMenuManager mContextMenuManager;
    @Mock TileGroup.Delegate mTileGroupDelegate;
    @Mock OfflinePageBridge mOfflinePageBridge;
    @Mock private Profile mProfile;
    @Mock private Runnable mSnapshotTileGridChangedRunnable;
    @Mock private Runnable mTileCountChangedRunnable;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;

    @Captor
    private ArgumentCaptor<NtpCustomizationConfigManager.HomepageStateListener>
            mHomepageStateListenerCaptor;

    private FakeMostVisitedSites mMostVisitedSites;
    private PropertyModel mModel;
    private MostVisitedTilesMediator mMediator;

    @Before
    public void setUp() {
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

        NtpCustomizationConfigManager.setInstanceForTesting(mNtpCustomizationConfigManager);
    }

    @Test
    public void testOnTileDataChanged() {
        createMediator();
        mMediator.onTileDataChanged();

        verify(mTileRenderer, atLeastOnce())
                .renderTileSection(anyList(), eq(mMvTilesLayout), any());
        verify(mMvTilesLayout).addTile(any());
        verify(mSnapshotTileGridChangedRunnable, atLeastOnce()).run();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION)
    public void testOnTileCountChanged() {
        // When the there's no mv tile, the mvt section shouldn't be shown.
        mMostVisitedSites.setTileSuggestions(new ArrayList<>());
        createMediator();

        Assert.assertFalse(mModel.get(IS_CONTAINER_VISIBLE));

        // When the there's mv tile, the mvt section should be shown.
        mMostVisitedSites.setTileSuggestions(JUnitTestGURLs.HTTP_URL.getSpec());
        createMediator();

        Assert.assertTrue(mModel.get(IS_CONTAINER_VISIBLE));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testMvtContainerOnTileCountChanged_DisableMvtCustomization() {
        doTestMvtContainerOnTileCountChanged();
    }

    @Test
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testMvtContainerOnTileCountChanged_EndableMvtCustomization() {
        doTestMvtContainerOnTileCountChanged();
    }

    private void doTestMvtContainerOnTileCountChanged() {
        ArrayList<SiteSuggestion> array = new ArrayList<>();
        array.add(mData);
        mMostVisitedSites.setTileSuggestions(array);
        createMediator();

        Assert.assertTrue(mModel.get(IS_CONTAINER_VISIBLE));

        mMostVisitedSites.setTileSuggestions(new ArrayList<>());
        mMediator.onTileCountChanged();
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION)) {
            // When there's no mv tile, the mv tiles container should show (with the "Add new"
            // button).
            Assert.assertTrue(mModel.get(IS_CONTAINER_VISIBLE));
        } else {
            // When there's no mv tile, the mv tiles container should be hidden.
            Assert.assertFalse(mModel.get(IS_CONTAINER_VISIBLE));
        }

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

    @Test
    public void testSetMvtVisibility() {
        createMediatorWithMockPropertyModel();
        clearInvocations(mModel);
        mMediator.setMvtVisibility(true);
        verify(mModel).set(eq(IS_CONTAINER_VISIBLE), eq(true));

        mMediator.setMvtVisibility(false);
        verify(mModel).set(eq(IS_CONTAINER_VISIBLE), eq(false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_FOR_MVT)
    public void testOnMvtVisibilityChanged() {
        createMediatorWithMockPropertyModel();
        clearInvocations(mModel);
        verify(mNtpCustomizationConfigManager).addListener(mHomepageStateListenerCaptor.capture());
        NtpCustomizationConfigManager.HomepageStateListener listener =
                mHomepageStateListenerCaptor.getValue();

        // Verifies when the listener is notified, the visibility of the Most Visited Tiles section
        // is changed properly.
        listener.onMvtVisibilityChanged(/* isMvtVisible= */ true);
        verify(mModel).set(eq(IS_CONTAINER_VISIBLE), eq(true));

        listener.onMvtVisibilityChanged(/* isMvtVisible= */ false);
        verify(mModel).set(eq(IS_CONTAINER_VISIBLE), eq(false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_FOR_MVT)
    public void testAddAndRemoveListener_FeatureEnabled() {
        createMediator();
        verify(mNtpCustomizationConfigManager).addListener(any());

        mMediator.destroy();
        verify(mNtpCustomizationConfigManager).removeListener(any());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_FOR_MVT)
    public void testAddAndRemoveListener_FeatureDisabled() {
        createMediator();
        verify(mNtpCustomizationConfigManager, never()).addListener(any());

        mMediator.destroy();
        verify(mNtpCustomizationConfigManager, never()).removeListener(any());
    }

    private void createMediatorWithMockPropertyModel() {
        mModel = mock(PropertyModel.class);
        createMediator(/* isTablet= */ false);
    }

    private void createMediator() {
        createMediator(/* isTablet= */ false);
    }

    private void createMediator(boolean isTablet) {
        mMvTilesLayout = Mockito.mock(MostVisitedTilesLayout.class);

        when(mMvTilesLayout.getResources()).thenReturn(mResources);
        when(mMvTilesLayout.getChildCount()).thenReturn(1);
        when(mMvTilesLayout.getChildAt(0)).thenReturn(mTileView);
        when(mMvTilesLayout.getTileCount()).thenReturn(1);
        when(mMvTilesLayout.getTileAt(0)).thenReturn(mTileView);
        mMvTilesLayout.addTile(mTileView);

        mMediator =
                new MostVisitedTilesMediator(
                        mResources,
                        mUiConfig,
                        mMvTilesLayout,
                        mTileRenderer,
                        mModel,
                        isTablet,
                        mSnapshotTileGridChangedRunnable,
                        mTileCountChangedRunnable);
        mMediator.initWithNative(
                mProfile,
                mUserEducationHelper,
                mSuggestionsUiDelegate,
                mContextMenuManager,
                mTileGroupDelegate,
                mOfflinePageBridge,
                mTileRenderer);
    }
}
