// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
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
import static org.chromium.chrome.browser.suggestions.tile.MostVisitedTilesProperties.IS_VISIBLE;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;

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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
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
    @Mock UiConfig mUiConfig;
    @Mock ViewGroup mMvTilesContainerLayout;
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

    private Context mContext;
    private Resources mResources;
    private FakeMostVisitedSites mMostVisitedSites;
    private PropertyModel mModel;
    private MostVisitedTilesMediator mMediator;

    private int mTileViewPaddingEdgePortrait;
    private int mTileViewPaddingLandscape;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mResources = mContext.getResources();
        mResources.getDisplayMetrics().widthPixels = 1000;
        mModel = new PropertyModel(MostVisitedTilesProperties.ALL_KEYS);

        mTileViewPaddingEdgePortrait =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_portrait);
        mTileViewPaddingLandscape =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_landscape);

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
        verify(mSnapshotTileGridChangedRunnable, atLeastOnce()).run();
    }

    /** Verifies the container visibility should only depend on the toggle state. */
    @Test
    @Features.EnableFeatures(ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION)
    public void testOnMvtToggleChanged() {
        createMediator();
        verify(mNtpCustomizationConfigManager)
                .addListener(mHomepageStateListenerCaptor.capture(), eq(mContext), eq(false));
        NtpCustomizationConfigManager.HomepageStateListener listener =
                mHomepageStateListenerCaptor.getValue();

        verifyMvtSectionVisibility(
                listener,
                /* toggleIsOn= */ true,
                /* hasTiles= */ true,
                /* expectedVisibility= */ true);
        verifyMvtSectionVisibility(
                listener,
                /* toggleIsOn= */ false,
                /* hasTiles= */ true,
                /* expectedVisibility= */ false);
        verifyMvtSectionVisibility(
                listener,
                /* toggleIsOn= */ true,
                /* hasTiles= */ false,
                /* expectedVisibility= */ true);
        verifyMvtSectionVisibility(
                listener,
                /* toggleIsOn= */ false,
                /* hasTiles= */ false,
                /* expectedVisibility= */ false);
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
    @SuppressWarnings("DirectInvocationOnMock")
    public void testSetPortraitPaddings_NotSmallDevice() {
        mResources.getConfiguration().orientation = Configuration.ORIENTATION_PORTRAIT;
        createMediator();
        mMediator.onTileDataChanged();

        Assert.assertEquals(
                mTileViewPaddingEdgePortrait, (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));

        int tileViewWidth = mResources.getDimensionPixelOffset(R.dimen.tile_view_width);
        int lateralMarginSum =
                mResources.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin) * 2;
        Assert.assertEquals(
                (int)
                        ((mResources.getDisplayMetrics().widthPixels
                                        - lateralMarginSum
                                        - mModel.get(HORIZONTAL_EDGE_PADDINGS)
                                        - tileViewWidth * 4.5)
                                / 4),
                (int) mModel.get(HORIZONTAL_INTERVAL_PADDINGS));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testSetPortraitPaddings_SmallDevice() {
        mResources.getConfiguration().orientation = Configuration.ORIENTATION_PORTRAIT;
        when(mUiConfig.getCurrentDisplayStyle())
                .thenReturn(
                        new DisplayStyle(HorizontalDisplayStyle.NARROW, VerticalDisplayStyle.FLAT));
        createMediator();
        mMediator.onTileDataChanged();

        Assert.assertEquals(
                mTileViewPaddingEdgePortrait, (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
        int tileViewWidth = mResources.getDimensionPixelOffset(R.dimen.tile_view_width_condensed);
        int lateralMarginSum =
                mResources.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin) * 2;
        Assert.assertEquals(
                Integer.max(
                        0,
                        (int)
                                ((mResources.getDisplayMetrics().widthPixels
                                                - lateralMarginSum
                                                - mModel.get(HORIZONTAL_EDGE_PADDINGS)
                                                - tileViewWidth * 4.5)
                                        / 4)),
                (int) mModel.get(HORIZONTAL_INTERVAL_PADDINGS));
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testSetLandscapePaddings() {
        mResources.getConfiguration().orientation = Configuration.ORIENTATION_LANDSCAPE;
        createMediator();
        mMediator.onTileDataChanged();

        Assert.assertEquals(mTileViewPaddingLandscape, (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
        Assert.assertEquals(
                mTileViewPaddingLandscape, (int) mModel.get(HORIZONTAL_INTERVAL_PADDINGS));
    }

    @Test
    public void testDestroy() {
        createMediator();

        mMediator.destroy();

        verify(mMvTilesLayout).destroy();
    }

    @Test
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUpdateTilesView_Tablet() {
        int expectedTileViewEdgePadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_edge_tablet);
        int expectedTileViewIntervalPadding =
                mResources.getDimensionPixelSize(R.dimen.tile_view_padding_interval_tablet);
        mResources.getConfiguration().orientation = Configuration.ORIENTATION_PORTRAIT;
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

        mResources.getConfiguration().orientation = Configuration.ORIENTATION_LANDSCAPE;
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
    @SuppressWarnings("DirectInvocationOnMock")
    public void testUpdateTilesView_Phone() {
        mResources.getConfiguration().orientation = Configuration.ORIENTATION_PORTRAIT;
        createMediator(/* isTablet= */ false);
        mMediator.onTileDataChanged();
        // tile_view_padding_edge_portrait
        Assert.assertEquals(
                "The horizontal edge padding passed to the model is wrong",
                mTileViewPaddingEdgePortrait,
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));

        mResources.getConfiguration().orientation = Configuration.ORIENTATION_LANDSCAPE;
        createMediator(/* isTablet= */ false);
        mMediator.onTileDataChanged();
        Assert.assertEquals(
                "The horizontal edge padding passed to the model is wrong",
                mTileViewPaddingLandscape,
                (int) mModel.get(HORIZONTAL_EDGE_PADDINGS));
    }

    /**
     * Verifies that the container is visible if and only if the MVT toggle is on. The presence of
     * tiles should have no effect.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION)
    public void testMvtContainerOnTileCountChanged() {
        createMediator();
        verifyMvtSectionVisibility(
                /* listener= */ null,
                /* toggleIsOn= */ true,
                /* hasTiles= */ true,
                /* expectedVisibility= */ true);
        verifyMvtSectionVisibility(
                /* listener= */ null,
                /* toggleIsOn= */ false,
                /* hasTiles= */ true,
                /* expectedVisibility= */ false);
        verifyMvtSectionVisibility(
                /* listener= */ null,
                /* toggleIsOn= */ true,
                /* hasTiles= */ false,
                /* expectedVisibility= */ true);
        verifyMvtSectionVisibility(
                /* listener= */ null,
                /* toggleIsOn= */ false,
                /* hasTiles= */ false,
                /* expectedVisibility= */ false);
    }

    private void verifyMvtSectionVisibility(
            NtpCustomizationConfigManager.HomepageStateListener listener,
            boolean toggleIsOn,
            boolean hasTiles,
            boolean expectedVisibility) {
        if (hasTiles) {
            mMostVisitedSites.setTileSuggestions(JUnitTestGURLs.HTTP_URL.getSpec());
        } else {
            mMostVisitedSites.setTileSuggestions(new ArrayList<>());
        }

        when(mNtpCustomizationConfigManager.getPrefIsMvtToggleOn()).thenReturn(toggleIsOn);

        if (listener == null) {
            mMediator.onTileCountChanged();
        } else {
            listener.onMvtToggleChanged();
        }

        Assert.assertEquals(expectedVisibility, mModel.get(IS_VISIBLE));
    }

    @Test
    public void testAddAndRemoveListener() {
        createMediator();
        verify(mNtpCustomizationConfigManager).addListener(any(), eq(mContext), eq(false));

        mMediator.destroy();
        verify(mNtpCustomizationConfigManager).removeListener(any());
    }

    @Test
    public void testUpdateMvtWidth_Tablet() {
        createMediator(/* isTablet= */ true);
        int totalWidth = 1000;
        ViewGroup.MarginLayoutParams marginLayoutParams =
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        when(mMvTilesContainerLayout.getLayoutParams()).thenReturn(marginLayoutParams);
        when(mMvTilesLayout.contentFitsOnTablet(totalWidth)).thenReturn(true);

        // Test case of regular tablets.
        UiConfig.DisplayStyle displayStyleWide =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleWide);
        assertFalse(
                NtpCustomizationUtils.isInNarrowWindowOnTablet(/* isTablet= */ true, mUiConfig));

        int lateralMargin = mResources.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin);
        mMediator.updateMvtWidth(totalWidth);
        verifyLayoutParams(marginLayoutParams, LayoutParams.WRAP_CONTENT, lateralMargin);

        // Test case of narrow window on tablets.
        when(mMvTilesLayout.contentFitsOnTablet(totalWidth)).thenReturn(false);
        UiConfig.DisplayStyle displayStyleRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleRegular);
        assertTrue(NtpCustomizationUtils.isInNarrowWindowOnTablet(true, mUiConfig));

        int lateralMarginNarrowWindowTablet =
                mResources.getDimensionPixelSize(
                        R.dimen.ntp_search_box_lateral_margin_narrow_window_tablet);
        mMediator.updateMvtWidth(totalWidth);
        verifyLayoutParams(marginLayoutParams, totalWidth, lateralMarginNarrowWindowTablet);
    }

    @Test
    public void testUpdateMvtWidth_Phone() {
        createMediator(/* isTablet= */ false);
        int totalWidth = 1000;
        ViewGroup.MarginLayoutParams marginLayoutParams =
                new ViewGroup.MarginLayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        when(mMvTilesContainerLayout.getLayoutParams()).thenReturn(marginLayoutParams);

        // On phones, the display style shouldn't trigger narrow window tablet logic.
        UiConfig.DisplayStyle displayStyleRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(mUiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleRegular);
        assertFalse(
                NtpCustomizationUtils.isInNarrowWindowOnTablet(/* isTablet= */ false, mUiConfig));

        int lateralMargin = mResources.getDimensionPixelSize(R.dimen.mvt_container_lateral_margin);

        // When width is provided, it should be applied directly.
        mMediator.updateMvtWidth(totalWidth);
        verifyLayoutParams(marginLayoutParams, totalWidth, lateralMargin);

        // When width is null, it should fall back to MATCH_PARENT.
        mMediator.updateMvtWidth(null);
        verifyLayoutParams(marginLayoutParams, LayoutParams.MATCH_PARENT, lateralMargin);
    }

    private void createMediator() {
        createMediator(/* isTablet= */ false);
    }

    private void createMediator(boolean isTablet) {
        mMvTilesLayout = Mockito.mock(MostVisitedTilesLayout.class);
        when(mMvTilesContainerLayout.findViewById(R.id.mv_tiles_layout)).thenReturn(mMvTilesLayout);

        when(mMvTilesLayout.getResources()).thenReturn(mResources);
        when(mMvTilesLayout.getChildCount()).thenReturn(1);
        when(mMvTilesLayout.getChildAt(0)).thenReturn(mTileView);
        when(mMvTilesLayout.getTileCount()).thenReturn(1);
        when(mMvTilesLayout.getTileAt(0)).thenReturn(mTileView);

        mMediator =
                new MostVisitedTilesMediator(
                        mContext,
                        mUiConfig,
                        mMvTilesContainerLayout,
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

    private void verifyLayoutParams(
            MarginLayoutParams marginLayoutParams, int expectedWidth, int expectedLateralMargin) {
        Assert.assertEquals(expectedWidth, marginLayoutParams.width);
        Assert.assertEquals(expectedLateralMargin, marginLayoutParams.leftMargin);
        Assert.assertEquals(expectedLateralMargin, marginLayoutParams.rightMargin);
    }
}
