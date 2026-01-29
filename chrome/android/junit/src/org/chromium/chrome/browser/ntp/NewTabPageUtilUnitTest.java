// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;

/** Unit tests for helper functions in {@link NewTabPage} and {@link NewTabPageLayout} classes. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NewTabPageUtilUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private View mView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView = new View(mContext);
        mView.setLayoutParams(new MarginLayoutParams(100, 100));
    }

    @Test
    public void testIsInNarrowWindowOnTablet() {
        UiConfig uiConfig = Mockito.mock(UiConfig.class);

        UiConfig.DisplayStyle displayStyleWide =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(uiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleWide);

        assertFalse(
                "It isn't a narrow window on tablet when displayStyleWide =="
                        + " HorizontalDisplayStyle.WIDE.",
                NtpCustomizationUtils.isInNarrowWindowOnTablet(true, uiConfig));

        UiConfig.DisplayStyle displayStyleRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(uiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleRegular);
        assertFalse(
                "It isn't a narrow window on tablet when |isTablet| is false.",
                NtpCustomizationUtils.isInNarrowWindowOnTablet(false, uiConfig));

        assertTrue(NtpCustomizationUtils.isInNarrowWindowOnTablet(true, uiConfig));
    }

    @Test
    public void testIsInSingleUrlBarMode_OmniboxMobileParityUpdateV2Enabled() {
        // Verifies isInSingleUrlBarMode() returns false on tablets.
        assertFalse(NewTabPage.isInSingleUrlBarMode(/* isTablet= */ true));

        // Verifies that isInSingleUrlBarMode() return true on phones.
        assertTrue(NewTabPage.isInSingleUrlBarMode(/* isTablet= */ false));
    }

    @Test
    public void testApplyUpdatedLayoutParamsForComposeplateView() {
        Resources resources = mContext.getResources();
        int originalPaddingStart = mView.getPaddingStart();
        int originalPaddingEnd = mView.getPaddingEnd();

        int paddingBottomPx =
                resources.getDimensionPixelSize(
                        R.dimen.composeplate_view_button_padding_for_shadow_bottom);
        int composeplateViewHeight =
                resources.getDimensionPixelSize(
                        R.dimen.composeplate_view_height_with_padding_for_shadow);

        NewTabPageUtils.applyUpdatedLayoutParamsForComposeplateView(mView);

        // Verify padding
        assertEquals(
                originalPaddingStart, mView.getPaddingStart()); // Padding start should not change
        assertEquals(paddingBottomPx, mView.getPaddingTop());
        assertEquals(originalPaddingEnd, mView.getPaddingEnd()); // Padding end should not change
        assertEquals(paddingBottomPx, mView.getPaddingBottom());

        // Verify layout parameters
        MarginLayoutParams layoutParams = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(composeplateViewHeight, layoutParams.height);
        assertEquals(paddingBottomPx, layoutParams.topMargin);
    }

    @Test
    public void testUpdateTilesLayoutTopMargin_shouldShowLogo_phones() {
        Resources resources = mContext.getResources();
        int mvtContainerTopMargin =
                resources.getDimensionPixelSize(R.dimen.mvt_container_top_margin);

        testUpdateTilesLayoutTopMargin_shouldShowLogoImpl(
                /* isTablet*/ false, mvtContainerTopMargin);
    }

    @Test
    public void testUpdateTilesLayoutTopMargin_shouldShowLogo_tablets() {
        Resources resources = mContext.getResources();
        int mvtContainerTopMargin =
                resources.getDimensionPixelSize(R.dimen.mvt_container_top_margin);

        testUpdateTilesLayoutTopMargin_shouldShowLogoImpl(
                /* isTablet*/ true, mvtContainerTopMargin);
    }

    private void testUpdateTilesLayoutTopMargin_shouldShowLogoImpl(
            boolean isTablet, int expectedMvtContainerTopMargin) {
        Resources resources = mContext.getResources();
        int paddingBottomPx =
                resources.getDimensionPixelSize(
                        R.dimen.composeplate_view_button_padding_for_shadow_bottom);

        verifyTilesLayoutTopMargin(
                /* shouldShowLogo= */ true,
                /* isWhiteBackgroundOnSearchBoxApplied= */ false,
                isTablet,
                expectedMvtContainerTopMargin);

        verifyTilesLayoutTopMargin(
                /* shouldShowLogo= */ true,
                /* isWhiteBackgroundOnSearchBoxApplied= */ true,
                isTablet,
                expectedMvtContainerTopMargin - paddingBottomPx);
    }

    @Test
    public void testUpdateTilesLayoutTopMargin_shouldNotShowLogo_phones() {
        Resources resources = mContext.getResources();
        int tileLayoutNoLogoTopMargin =
                resources.getDimensionPixelSize(R.dimen.tile_layout_no_logo_top_margin);

        testUpdateTilesLayoutTopMargin_shouldNotShowLogoImpl(
                /* isTablet*/ false, tileLayoutNoLogoTopMargin);
    }

    @Test
    public void testUpdateTilesLayoutTopMargin_shouldNotShowLogo_tablets() {
        Resources resources = mContext.getResources();
        int expectedTileLayoutTopMargin =
                resources.getDimensionPixelSize(R.dimen.mvt_container_top_margin);

        testUpdateTilesLayoutTopMargin_shouldNotShowLogoImpl(
                /* isTablet*/ true, expectedTileLayoutTopMargin);
    }

    private void testUpdateTilesLayoutTopMargin_shouldNotShowLogoImpl(
            boolean isTablet, int expectedTopMargin) {
        Resources resources = mContext.getResources();
        int paddingBottomPx =
                resources.getDimensionPixelSize(
                        R.dimen.composeplate_view_button_padding_for_shadow_bottom);

        verifyTilesLayoutTopMargin(
                /* shouldShowLogo= */ false,
                /* isWhiteBackgroundOnSearchBoxApplied= */ false,
                isTablet,
                expectedTopMargin);

        verifyTilesLayoutTopMargin(
                /* shouldShowLogo= */ false,
                /* isWhiteBackgroundOnSearchBoxApplied= */ true,
                isTablet,
                expectedTopMargin - paddingBottomPx);
    }

    private void verifyTilesLayoutTopMargin(
            boolean shouldShowLogo,
            boolean isWhiteBackgroundOnSearchBoxApplied,
            boolean isTablet,
            int expectedTopMargin) {
        NewTabPageUtils.updateTilesLayoutTopMargin(
                mView, shouldShowLogo, isWhiteBackgroundOnSearchBoxApplied, isTablet);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(expectedTopMargin, layoutParams.topMargin);
    }
}
