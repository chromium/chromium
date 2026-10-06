// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.DimenRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.PaddingStyle;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;

/** Unit tests for {@link MostVisitedTilesCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.NTP_AURORA + ":padding_style/0"})
public class MostVisitedTilesCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String PADDING_STYLE_PARAM = "padding_style";
    private static final int START_PADDING = 10;
    private static final int TOP_PADDING = 11;
    private static final int END_PADDING = 20;
    private static final int BOTTOM_PADDING = 21;
    private static final int INITIAL_TOP_MARGIN = 7;
    private static final int NO_MARGIN_EXPECTED = 0;

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private MostVisitedTilesMediator mMediator;
    @Mock private UiConfig mUiConfig;

    private Activity mActivity;
    private View mMvTilesContainerLayout;
    private MostVisitedTilesCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mMvTilesContainerLayout =
                LayoutInflater.from(mActivity).inflate(R.layout.mv_tiles_layout, null);
        when(mUiConfig.getCurrentDisplayStyle())
                .thenReturn(
                        new DisplayStyle(
                                HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR));
        mCoordinator =
                new MostVisitedTilesCoordinator(
                        mActivity,
                        mActivityLifecycleDispatcher,
                        mMvTilesContainerLayout,
                        mUiConfig,
                        null,
                        null);
        mCoordinator.setMediatorForTesting(mMediator);
    }

    @Test
    public void testUpdateMvtVisibility() {
        mCoordinator.updateMvtVisibility();
        verify(mMediator).updateMvtVisibility();
    }

    @Test
    public void testUpdateMvtWidth_WithWidth() {
        int widthMvt = 1000;
        int lateralMargin = 48;
        mMvTilesContainerLayout.setVisibility(View.VISIBLE);
        mCoordinator.updateMvtWidth(widthMvt, lateralMargin);
        verify(mMediator).updateMvtWidth(eq(widthMvt), eq(lateralMargin));

        clearInvocations(mMediator);
        mMvTilesContainerLayout.setVisibility(View.GONE);
        mCoordinator.updateMvtWidth(widthMvt, lateralMargin);
        verify(mMediator, never()).updateMvtWidth(anyInt(), anyInt());
    }

    @Test
    public void testUpdateTilesLayoutMargins() {
        mCoordinator.updateTilesLayoutMargins(/* shouldShowLogo= */ true, /* isLff= */ false);
        verify(mMediator).updateTilesLayoutMargins(eq(true), eq(false));
    }

    @Test
    public void testConstructor_withDefaultPaddingStyle() {
        verifyMvtPaddingsAndTopMargin(
                PaddingStyle.DEFAULT,
                /* expectPaddingSet= */ false,
                /* expectedTopMarginDimen= */ NO_MARGIN_EXPECTED);
    }

    @Test
    public void testConstructor_withAuroraPaddingStyleSmall() {
        verifyMvtPaddingsAndTopMargin(
                PaddingStyle.SMALL,
                /* expectPaddingSet= */ true,
                R.dimen.mvt_container_top_margin_medium);
    }

    @Test
    public void testConstructor_withAuroraPaddingStyleMedium() {
        verifyMvtPaddingsAndTopMargin(
                PaddingStyle.MEDIUM,
                /* expectPaddingSet= */ true,
                R.dimen.mvt_container_top_margin_large);
    }

    @Test
    public void testConstructor_withAuroraPaddingStyleLarge() {
        verifyMvtPaddingsAndTopMargin(
                PaddingStyle.LARGE,
                /* expectPaddingSet= */ true,
                R.dimen.mvt_container_top_margin_large);
    }

    /**
     * Verifies that the Aurora top margin is re-applied, rather than the logo-based one, when the
     * margins are updated, e.g. after the default search engine changes.
     */
    @Test
    public void testUpdateTilesLayoutMargins_withAuroraPaddingStyleSmall() {
        testUpdateTilesLayoutMarginsWithAuroraImpl(
                PaddingStyle.SMALL, R.dimen.mvt_container_top_margin_medium);
    }

    @Test
    public void testUpdateTilesLayoutMargins_withAuroraPaddingStyleLarge() {
        testUpdateTilesLayoutMarginsWithAuroraImpl(
                PaddingStyle.LARGE, R.dimen.mvt_container_top_margin_large);
    }

    private void testUpdateTilesLayoutMarginsWithAuroraImpl(
            @PaddingStyle int paddingStyle, @DimenRes int expectedTopMarginDimen) {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, paddingStyle);
        MarginLayoutParams marginLayoutParams = new MarginLayoutParams(100, 100);
        mMvTilesContainerLayout.setLayoutParams(marginLayoutParams);

        mCoordinator.updateTilesLayoutMargins(/* shouldShowLogo= */ false, /* isLff= */ false);

        verify(mMediator, never()).updateTilesLayoutMargins(anyBoolean(), anyBoolean());
        assertEquals(
                mActivity.getResources().getDimensionPixelSize(expectedTopMarginDimen),
                marginLayoutParams.topMargin);
    }

    private void verifyMvtPaddingsAndTopMargin(
            @PaddingStyle int paddingStyle, boolean expectPaddingSet, int expectedTopMarginDimen) {
        Resources res = mActivity.getResources();
        View containerLayout =
                LayoutInflater.from(mActivity).inflate(R.layout.mv_tiles_layout, null);
        MarginLayoutParams marginLayoutParams = new MarginLayoutParams(100, 100);
        marginLayoutParams.topMargin = INITIAL_TOP_MARGIN;
        containerLayout.setLayoutParams(marginLayoutParams);
        containerLayout.setPaddingRelative(START_PADDING, TOP_PADDING, END_PADDING, BOTTOM_PADDING);
        if (paddingStyle != PaddingStyle.DEFAULT) {
            FeatureOverrides.overrideParam(
                    ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, paddingStyle);
        }

        new MostVisitedTilesCoordinator(
                mActivity, mActivityLifecycleDispatcher, containerLayout, mUiConfig, null, null);

        if (expectPaddingSet) {
            int expectedTopPadding =
                    res.getDimensionPixelSize(R.dimen.mvt_container_top_padding_small);
            int expectedBottomPadding =
                    res.getDimensionPixelSize(R.dimen.mvt_container_bottom_padding_small);
            assertEquals(START_PADDING, containerLayout.getPaddingStart());
            assertEquals(expectedTopPadding, containerLayout.getPaddingTop());
            assertEquals(END_PADDING, containerLayout.getPaddingEnd());
            assertEquals(expectedBottomPadding, containerLayout.getPaddingBottom());
        } else {
            assertEquals(START_PADDING, containerLayout.getPaddingStart());
            assertEquals(TOP_PADDING, containerLayout.getPaddingTop());
            assertEquals(END_PADDING, containerLayout.getPaddingEnd());
            assertEquals(BOTTOM_PADDING, containerLayout.getPaddingBottom());
        }

        if (expectedTopMarginDimen != NO_MARGIN_EXPECTED) {
            int expectedTopMargin = res.getDimensionPixelSize(expectedTopMarginDimen);
            assertEquals(expectedTopMargin, marginLayoutParams.topMargin);
        } else {
            assertEquals(INITIAL_TOP_MARGIN, marginLayoutParams.topMargin);
        }
    }
}
