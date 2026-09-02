// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

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
public class MostVisitedTilesCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String PADDING_STYLE_PARAM = "padding_style";
    private static final int START_PADDING = 10;
    private static final int END_PADDING = 20;
    private static final int NO_MARGIN_EXPECTED = 0;

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private View mMvTilesContainerLayout;
    @Mock private MostVisitedTilesLayout mMvTilesLayout;
    @Mock private MostVisitedTilesMediator mMediator;
    @Mock private UiConfig mUiConfig;

    private Activity mActivity;
    private MostVisitedTilesCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mMvTilesContainerLayout.findViewById(R.id.mv_tiles_layout)).thenReturn(mMvTilesLayout);
        when(mMvTilesLayout.getContext()).thenReturn(mActivity);
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
        int totalWidth = 1000;
        when(mMvTilesContainerLayout.getVisibility()).thenReturn(View.VISIBLE);
        mCoordinator.updateMvtWidth(totalWidth);
        verify(mMediator).updateMvtWidth(eq(totalWidth));

        clearInvocations(mMediator);
        when(mMvTilesContainerLayout.getVisibility()).thenReturn(View.GONE);
        mCoordinator.updateMvtWidth(totalWidth);
        verify(mMediator, never()).updateMvtWidth(any(Integer.class));
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

    private void verifyMvtPaddingsAndTopMargin(
            @PaddingStyle int paddingStyle, boolean expectPaddingSet, int expectedTopMarginDimen) {
        Resources res = mActivity.getResources();
        MarginLayoutParams marginLayoutParams = null;
        if (paddingStyle != PaddingStyle.DEFAULT) {
            marginLayoutParams = new MarginLayoutParams(100, 100);
            when(mMvTilesContainerLayout.getLayoutParams()).thenReturn(marginLayoutParams);
            FeatureOverrides.overrideParam(
                    ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, paddingStyle);
            when(mMvTilesContainerLayout.getPaddingStart()).thenReturn(START_PADDING);
            when(mMvTilesContainerLayout.getPaddingEnd()).thenReturn(END_PADDING);
        }

        new MostVisitedTilesCoordinator(
                mActivity,
                mActivityLifecycleDispatcher,
                mMvTilesContainerLayout,
                mUiConfig,
                null,
                null);

        if (expectPaddingSet) {
            int expectedTopPadding =
                    res.getDimensionPixelSize(R.dimen.mvt_container_top_padding_small);
            int expectedBottomPadding =
                    res.getDimensionPixelSize(R.dimen.mvt_container_bottom_padding_small);

            verify(mMvTilesContainerLayout)
                    .setPaddingRelative(
                            START_PADDING, expectedTopPadding, END_PADDING, expectedBottomPadding);
        } else {
            verify(mMvTilesContainerLayout, never())
                    .setPaddingRelative(
                            any(Integer.class),
                            any(Integer.class),
                            any(Integer.class),
                            any(Integer.class));
        }

        if (expectedTopMarginDimen != NO_MARGIN_EXPECTED) {
            int expectedTopMargin = res.getDimensionPixelSize(expectedTopMarginDimen);
            verify(mMvTilesContainerLayout).setLayoutParams(marginLayoutParams);
            assertEquals(expectedTopMargin, marginLayoutParams.topMargin);
        } else {
            verify(mMvTilesContainerLayout, never()).setLayoutParams(any(MarginLayoutParams.class));
        }
    }
}
