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
import android.os.Build;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.LayoutType;
import org.chromium.chrome.browser.ntp.NewTabPageUtils.PaddingStyle;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.components.browser_ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.components.browser_ui.widget.displaystyle.VerticalDisplayStyle;

/**
 * Unit tests for helper functions in {@link NewTabPage} and {@link NewTabPageCoordinator} classes.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class NewTabPageUtilUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String PADDING_STYLE_PARAM = "padding_style";

    @Mock private EdgeToEdgeController mEdgeToEdgeController;

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
    public void testIsInNarrowWindowOnLff() {
        UiConfig uiConfig = Mockito.mock(UiConfig.class);

        UiConfig.DisplayStyle displayStyleWide =
                new DisplayStyle(HorizontalDisplayStyle.WIDE, VerticalDisplayStyle.REGULAR);
        when(uiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleWide);

        assertFalse(
                "It isn't a narrow window on LFF when displayStyleWide =="
                        + " HorizontalDisplayStyle.WIDE.",
                NtpCustomizationUtils.isInNarrowWindowOnLff(true, uiConfig));

        UiConfig.DisplayStyle displayStyleRegular =
                new DisplayStyle(HorizontalDisplayStyle.REGULAR, VerticalDisplayStyle.REGULAR);
        when(uiConfig.getCurrentDisplayStyle()).thenReturn(displayStyleRegular);
        assertFalse(
                "It isn't a narrow window on LFF when |isLff| is false.",
                NtpCustomizationUtils.isInNarrowWindowOnLff(false, uiConfig));

        assertTrue(NtpCustomizationUtils.isInNarrowWindowOnLff(true, uiConfig));
    }

    @Test
    public void testIsInSingleUrlBarMode_OmniboxMobileParityUpdateV2Enabled() {
        // Verifies isInSingleUrlBarMode() returns false on LFF devices.
        assertFalse(NewTabPage.isInSingleUrlBarMode(/* isLff= */ true));
        // Verifies isInSingleUrlBarMode() returns true on phones.
        assertTrue(NewTabPage.isInSingleUrlBarMode(/* isLff= */ false));
    }

    @Test
    public void testUpdateTilesLayoutTopMargin_shouldShowLogo_phones() {
        Resources resources = mContext.getResources();
        int mvtContainerTopMargin = resources.getDimensionPixelSize(R.dimen.ntp_section_top_margin);

        testUpdateTilesLayoutTopMargin_shouldShowLogoImpl(
                /* isLff= */ false, mvtContainerTopMargin);
    }

    @Test
    public void testUpdateTilesLayoutTopMargin_shouldShowLogo_LFFs() {
        Resources resources = mContext.getResources();
        int mvtContainerTopMargin = resources.getDimensionPixelSize(R.dimen.ntp_section_top_margin);

        testUpdateTilesLayoutTopMargin_shouldShowLogoImpl(/* isLff= */ true, mvtContainerTopMargin);
    }

    private void testUpdateTilesLayoutTopMargin_shouldShowLogoImpl(
            boolean isLff, int expectedMvtContainerTopMargin) {
        verifyTilesLayoutTopMargin(
                /* shouldShowLogo= */ true, isLff, expectedMvtContainerTopMargin);
        verifyTilesLayoutTopMargin(
                /* shouldShowLogo= */ true, isLff, expectedMvtContainerTopMargin);
    }

    @Test
    public void testUpdateTilesLayoutTopMargin_shouldNotShowLogo_phones() {
        Resources resources = mContext.getResources();
        int tileLayoutNoLogoTopMargin =
                resources.getDimensionPixelSize(R.dimen.tile_layout_no_logo_top_margin);

        testUpdateTilesLayoutTopMargin_shouldNotShowLogoImpl(
                /* isLff= */ false, tileLayoutNoLogoTopMargin);
    }

    @Test
    public void testUpdateTilesLayoutTopMargin_shouldNotShowLogo_LFFs() {
        Resources resources = mContext.getResources();
        int expectedTileLayoutTopMargin =
                resources.getDimensionPixelSize(R.dimen.ntp_section_top_margin);

        testUpdateTilesLayoutTopMargin_shouldNotShowLogoImpl(
                /* isLff= */ true, expectedTileLayoutTopMargin);
    }

    @Test
    public void testGetPaddingStyleForAurora() {
        // Default should be DEFAULT.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, PaddingStyle.DEFAULT);
        assertEquals(PaddingStyle.DEFAULT, NewTabPageUtils.getPaddingStyleForAurora());

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, PaddingStyle.SMALL);
        assertEquals(PaddingStyle.SMALL, NewTabPageUtils.getPaddingStyleForAurora());

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, PaddingStyle.MEDIUM);
        assertEquals(PaddingStyle.MEDIUM, NewTabPageUtils.getPaddingStyleForAurora());

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, PaddingStyle.LARGE);
        assertEquals(PaddingStyle.LARGE, NewTabPageUtils.getPaddingStyleForAurora());
    }

    @Test
    public void testGetNtpSectionPaddingPx() {
        Resources resources = mContext.getResources();
        int expectedDefaultMargin = resources.getDimensionPixelSize(R.dimen.ntp_section_top_margin);
        int expectedSmallMargin =
                resources.getDimensionPixelSize(R.dimen.ntp_section_top_margin_small);

        // Default should be ntp_section_top_margin.
        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, PaddingStyle.DEFAULT);
        assertEquals(expectedDefaultMargin, NewTabPageUtils.getNtpSectionPaddingPx(resources));

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, PaddingStyle.SMALL);
        assertEquals(expectedSmallMargin, NewTabPageUtils.getNtpSectionPaddingPx(resources));

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, PaddingStyle.MEDIUM);
        assertEquals(expectedSmallMargin, NewTabPageUtils.getNtpSectionPaddingPx(resources));

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA, PADDING_STYLE_PARAM, PaddingStyle.LARGE);
        assertEquals(expectedSmallMargin, NewTabPageUtils.getNtpSectionPaddingPx(resources));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.NTP_AURORA_V2)
    public void testIsComposeplateRemovedInAuroraV2_Disabled() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA_V2,
                ChromeFeatureList.NTP_AURORA_V2_LAYOUT_TYPE,
                LayoutType.INSIDE_MVT);
        assertFalse(NewTabPageUtils.isComposeplateRemovedInAuroraV2());

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA_V2,
                ChromeFeatureList.NTP_AURORA_V2_LAYOUT_TYPE,
                LayoutType.REMOVE);
        assertFalse(NewTabPageUtils.isComposeplateRemovedInAuroraV2());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.NTP_AURORA, ChromeFeatureList.NTP_AURORA_V2})
    public void testIsComposeplateRemovedInAuroraV2() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA_V2,
                ChromeFeatureList.NTP_AURORA_V2_LAYOUT_TYPE,
                LayoutType.DEFAULT);
        assertFalse(NewTabPageUtils.isComposeplateRemovedInAuroraV2());

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA_V2,
                ChromeFeatureList.NTP_AURORA_V2_LAYOUT_TYPE,
                LayoutType.BESIDE_MVT);
        assertFalse(NewTabPageUtils.isComposeplateRemovedInAuroraV2());

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA_V2,
                ChromeFeatureList.NTP_AURORA_V2_LAYOUT_TYPE,
                LayoutType.INSIDE_MVT);
        assertTrue(NewTabPageUtils.isComposeplateRemovedInAuroraV2());

        FeatureOverrides.overrideParam(
                ChromeFeatureList.NTP_AURORA_V2,
                ChromeFeatureList.NTP_AURORA_V2_LAYOUT_TYPE,
                LayoutType.REMOVE);
        assertTrue(NewTabPageUtils.isComposeplateRemovedInAuroraV2());
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/true")
    public void testShouldConsumeTopInset_TopEdgeToEdgeEnabled() {
        assertTrue(EdgeToEdgeUtils.isTopEdgeToEdgeEnabled());
        assertFalse(NewTabPage.shouldConsumeTopInset(50, /* controller= */ null));
        assertFalse(NewTabPage.shouldConsumeTopInset(0, mEdgeToEdgeController));

        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.tabShouldDrawFullTopEdgeToEdge()).thenReturn(true);
        assertTrue(NewTabPage.shouldConsumeTopInset(50, mEdgeToEdgeController));

        // Status indicator visible: both states false.
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(false);
        when(mEdgeToEdgeController.tabShouldDrawFullTopEdgeToEdge()).thenReturn(false);
        assertFalse(NewTabPage.shouldConsumeTopInset(50, mEdgeToEdgeController));

        // Opted-in web tab with top scalp visible: isDrawingToTopEdge() true, full false.
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        assertFalse(NewTabPage.shouldConsumeTopInset(50, mEdgeToEdgeController));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.R)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET + ":enable_top_edge_to_edge/false")
    public void testShouldConsumeTopInset_RefactorOnlyArm() {
        assertFalse(EdgeToEdgeUtils.isTopEdgeToEdgeEnabled());
        assertTrue(EdgeToEdgeUtils.isEdgeToEdgeRefactorEnabled());

        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        assertTrue(NewTabPage.shouldConsumeTopInset(50, mEdgeToEdgeController));
        assertFalse(NewTabPage.shouldConsumeTopInset(0, mEdgeToEdgeController));

        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(false);
        assertFalse(NewTabPage.shouldConsumeTopInset(50, mEdgeToEdgeController));
    }

    private void testUpdateTilesLayoutTopMargin_shouldNotShowLogoImpl(
            boolean isLff, int expectedTopMargin) {
        verifyTilesLayoutTopMargin(/* shouldShowLogo= */ false, isLff, expectedTopMargin);
        verifyTilesLayoutTopMargin(/* shouldShowLogo= */ false, isLff, expectedTopMargin);
    }

    private void verifyTilesLayoutTopMargin(
            boolean shouldShowLogo, boolean isLff, int expectedTopMargin) {
        NewTabPageUtils.updateTilesLayoutTopMargin(mView, shouldShowLogo, isLff);
        MarginLayoutParams layoutParams = (MarginLayoutParams) mView.getLayoutParams();
        assertEquals(expectedTopMargin, layoutParams.topMargin);
    }
}
