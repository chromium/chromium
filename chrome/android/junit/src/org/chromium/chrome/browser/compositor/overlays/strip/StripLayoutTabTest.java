// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.core.content.res.ResourcesCompat;
import androidx.test.core.app.ApplicationProvider;

import com.google.android.material.color.MaterialColors;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Tests for {@link StripLayoutTab}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripLayoutTabTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    private static final String TAG = "StripLayoutTabTest";

    private Context mContext;
    private StripLayoutTab mNormalTab;
    private StripLayoutTab mIncognitoTab;

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mNormalTab = createStripLayoutTab(false);
        mIncognitoTab = createStripLayoutTab(true);
    }

    @Test
    @Feature("Tab Strip Redesign")
    @EnableFeatures(ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP)
    public void testGetTint() {
        int expectedColor;

        // Normal active tab color.
        expectedColor = MaterialColors.getColor(mContext, R.attr.colorSurface, TAG);
        assertEquals("Normal active tab should match the Surface-0 color.", expectedColor,
                mNormalTab.getTint(true, false));

        // Normal inactive tab color.
        expectedColor =
                ChromeColors.getSurfaceColor(mContext, R.dimen.compositor_background_tab_elevation);
        assertEquals("Normal inactive tab should match the Surface-4 color.", expectedColor,
                mNormalTab.getTint(false, false));

        // Normal inactive tab hover color.
        expectedColor = ColorUtils.getColorWithOverlay(expectedColor,
                MaterialColors.getColor(mContext, R.attr.colorOnSurface, TAG),
                ResourcesCompat.getFloat(
                        mContext.getResources(), R.dimen.gm2_tab_inactive_hover_alpha));
        assertEquals(
                "Normal hovered inactive tab should match the Surface-4 color overlain by OnSurface @ 8%.",
                expectedColor, mNormalTab.getTint(false, true));

        // Incognito active tab color.
        expectedColor = mContext.getColor(R.color.toolbar_background_primary_dark);
        assertEquals("Incognito active tab should match the baseline color.", expectedColor,
                mIncognitoTab.getTint(true, false));

        // Incognito inactive tab color.
        expectedColor = mContext.getResources().getColor(
                R.color.baseline_neutral_10_with_neutral_0_alpha_30);
        assertEquals("Incognito inactive tab should match the baseline color.", expectedColor,
                mIncognitoTab.getTint(false, false));

        // Incognito inactive tab hover color.
        expectedColor = ColorUtils.getColorWithOverlay(expectedColor,
                mContext.getColor(R.color.baseline_neutral_90),
                ResourcesCompat.getFloat(
                        mContext.getResources(), R.dimen.gm2_tab_inactive_hover_alpha));
        assertEquals(
                "Incognito hovered inactive tab should match the baseline color overlain by the baseline equivalent of OnSurface @ 8%.",
                expectedColor, mIncognitoTab.getTint(false, true));
    }

    @Test
    @Feature("Tab Strip Redesign")
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN,
            ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP})
    public void
    testGetTint_TabStripRedesignFolio() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        int expectedColor;

        // Normal active tab color.
        expectedColor = MaterialColors.getColor(mContext, R.attr.colorSurface, TAG);
        assertEquals("Normal active folio should match the Surface-0 color.", expectedColor,
                mNormalTab.getTint(true, false));

        // Normal inactive tab color.
        expectedColor = ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_0);
        assertEquals("Folio inactive tab containers should be Surface-0.", expectedColor,
                mNormalTab.getTint(false, false));

        // Normal inactive tab hover color.
        expectedColor = ColorUtils.setAlphaComponent(
                ChromeSemanticColorUtils.getTabInactiveHoverColor(mContext),
                (int) (ResourcesCompat.getFloat(
                               mContext.getResources(), R.dimen.tsr_folio_tab_inactive_hover_alpha)
                        * 255));
        assertEquals("Normal hovered inactive folio should be Primary @ 8%.", expectedColor,
                mNormalTab.getTint(false, true));

        // Incognito active tab color.
        expectedColor = mContext.getColor(R.color.toolbar_background_primary_dark);
        assertEquals("Incognito active folio should match the baseline color.", expectedColor,
                mIncognitoTab.getTint(true, false));

        // Incognito inactive tab color.
        expectedColor = mContext.getColor(R.color.default_bg_color_dark);
        assertEquals("Incognito inactive folio should be baseline Surface-0.", expectedColor,
                mIncognitoTab.getTint(false, false));

        // Incognito inactive tab hover color.
        expectedColor = ColorUtils.setAlphaComponent(mContext.getColor(R.color.baseline_primary_80),
                (int) (ResourcesCompat.getFloat(
                               mContext.getResources(), R.dimen.tsr_folio_tab_inactive_hover_alpha)
                        * 255));
        assertEquals(
                "Incognito hovered inactive folio should be the baseline equivalent of Primary @ 8%.",
                expectedColor, mIncognitoTab.getTint(false, true));
    }

    @Test
    @Feature("Tab Strip Redesign")
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN,
            ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP})
    public void
    testGetTint_TabStripRedesignDetached() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        int expectedColor;

        // Normal active tab color.
        expectedColor = ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_5);
        assertEquals("Detached normal active should match the Surface-5 color.", expectedColor,
                mNormalTab.getTint(true, false));

        // Normal inactive tab color.
        expectedColor = ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_5);
        assertEquals("Detached inactive tab containers should be Surface-5.", expectedColor,
                mNormalTab.getTint(false, false));

        // Normal inactive tab hover color.
        expectedColor = ColorUtils.setAlphaComponent(
                ChromeSemanticColorUtils.getTabInactiveHoverColor(mContext),
                (int) (ResourcesCompat.getFloat(mContext.getResources(),
                               R.dimen.tsr_detached_tab_inactive_hover_alpha_light)
                        * 255));
        assertEquals("Normal hovered inactive folio should be Primary @ 5%.", expectedColor,
                mNormalTab.getTint(false, true));

        // Incognito active tab color.
        expectedColor = Color.BLACK;
        assertEquals("Detached incognito active should match the color black.", expectedColor,
                mIncognitoTab.getTint(true, false));

        // Incognito inactive tab color.
        expectedColor = mContext.getColor(R.color.default_bg_color_dark_elev_5_baseline);
        assertEquals("Detached incognito inactive should be baseline Surface-5.", expectedColor,
                mIncognitoTab.getTint(false, false));

        // Incognito inactive tab hover color.
        expectedColor = ColorUtils.setAlphaComponent(mContext.getColor(R.color.baseline_primary_80),
                (int) (ResourcesCompat.getFloat(mContext.getResources(),
                               R.dimen.tsr_detached_tab_inactive_hover_alpha_dark)
                        * 255));
        assertEquals(
                "Incognito hovered inactive folio should be the baseline equivalent of Primary @ 12%.",
                expectedColor, mIncognitoTab.getTint(false, true));
    }

    @Test
    @EnableFeatures(
            {ChromeFeatureList.TAB_STRIP_REDESIGN, ChromeFeatureList.TAB_STRIP_STARTUP_REFACTORING,
                    ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP})
    public void
    testGetTint_Startup_TabStripRedesign() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        int expectedColor;

        mNormalTab.setIsPlaceholder(true);
        mIncognitoTab.setIsPlaceholder(true);

        // Normal active tab color.
        expectedColor = ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_5);
        assertEquals("Detached normal active should match the regular foreground color.",
                expectedColor, mNormalTab.getTint(true, false));

        // Normal inactive tab color.
        expectedColor = mContext.getColor(R.color.bg_tabstrip_tab_detached_startup_tint);
        assertEquals("Normal inactive tab should match the placeholder color.", expectedColor,
                mNormalTab.getTint(false, false));

        // Incognito active tab color.
        expectedColor = Color.BLACK;
        assertEquals("Detached incognito active should match the regular foreground color.",
                expectedColor, mIncognitoTab.getTint(true, false));

        // Incognito inactive tab color.
        expectedColor = mContext.getColor(R.color.bg_tabstrip_tab_detached_startup_tint);
        assertEquals("Incognito inactive tab should match the placeholder color.", expectedColor,
                mIncognitoTab.getTint(false, false));
    }

    @Test
    @Feature("Tab Strip Redesign")
    @EnableFeatures({ChromeFeatureList.ADVANCED_PERIPHERALS_SUPPORT_TAB_STRIP})
    public void testGetOutlineTint() {
        int expectedColor;

        // Normal active tab outline.
        expectedColor = MaterialColors.getColor(mContext, R.attr.colorSurface, TAG);
        assertEquals("Normal active tab outline should match the container color.", expectedColor,
                mNormalTab.getOutlineTint(true));

        // Normal inactive tab outline.
        final int baseColor =
                ChromeColors.getSurfaceColor(mContext, R.dimen.compositor_background_tab_elevation);
        final int overlayColor = MaterialColors.getColor(mContext, R.attr.colorOutline, TAG);
        final float overlayAlpha = ResourcesCompat.getFloat(
                mContext.getResources(), R.dimen.compositor_background_tab_outline_alpha);
        expectedColor = ColorUtils.getColorWithOverlay(baseColor, overlayColor, overlayAlpha);
        assertEquals("Normal inactive tabs should have an overlain color.", expectedColor,
                mNormalTab.getOutlineTint(false));

        // Incognito active tab outline.
        expectedColor = mContext.getColor(R.color.toolbar_background_primary_dark);
        assertEquals("Incognito active tab outline should match the container color.",
                expectedColor, mIncognitoTab.getOutlineTint(true));

        // Incognito inactive tab outline.
        expectedColor = mContext.getResources().getColor(
                R.color.baseline_neutral_10_with_neutral_0_alpha_30_with_neutral_variant_60_alpha_15);
        assertEquals("Inactive tab outline should match the baseline color.", expectedColor,
                mIncognitoTab.getOutlineTint(false));
    }

    @Test
    @Feature("Tab Strip Redesign")
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
    public void testGetOutlineTint_TabStripRedesignFolio() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        int expectedColor = Color.TRANSPARENT;

        // Normal.
        assertEquals("TSR Folio containers should not have an outline.", expectedColor,
                mNormalTab.getOutlineTint(true));
        assertEquals("TSR Folio containers should not have an outline.", expectedColor,
                mNormalTab.getOutlineTint(false));

        // Incognito.
        assertEquals("TSR Folio containers should not have an outline.", expectedColor,
                mIncognitoTab.getOutlineTint(true));
        assertEquals("TSR Folio containers should not have an outline.", expectedColor,
                mIncognitoTab.getOutlineTint(false));
    }

    @Test
    @Feature("Tab Strip Redesign")
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
    public void testGetOutlineTint_TabStripRedesignDetached() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        int expectedColor = Color.TRANSPARENT;

        // Normal.
        assertEquals("TSR Detached containers should not have an outline.", expectedColor,
                mNormalTab.getOutlineTint(true));
        assertEquals("TSR Detached containers should not have an outline.", expectedColor,
                mNormalTab.getOutlineTint(false));

        // Incognito.
        assertEquals("TSR Detached containers should not have an outline.", expectedColor,
                mIncognitoTab.getOutlineTint(true));
        assertEquals("TSR Detached containers should not have an outline.", expectedColor,
                mIncognitoTab.getOutlineTint(false));
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetDividerTint() {
        int expectedColor = Color.TRANSPARENT;

        // Normal.
        assertEquals("Non-TSR tabs should not have a divider.", expectedColor,
                mNormalTab.getDividerTint());

        // Incognito.
        assertEquals("Non-TSR tabs should not have a divider.", expectedColor,
                mNormalTab.getDividerTint());
    }

    @Test
    @Feature("Tab Strip Redesign")
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
    public void testGetDividerTint_TabStripRedesignFolio() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        int expectedColor;

        // Normal.
        expectedColor = androidx.core.graphics.ColorUtils.setAlphaComponent(
                SemanticColorUtils.getDefaultIconColorAccent1(mContext),
                (int) (StripLayoutTab.DIVIDER_FOLIO_LIGHT_OPACITY * 255));
        assertEquals("TSR Folio light mode divider uses 20% icon color", expectedColor,
                mNormalTab.getDividerTint());

        // Incognito.
        expectedColor = mContext.getColor(R.color.divider_line_bg_color_light);
        assertEquals("TSR incognito dividers use the baseline color.", expectedColor,
                mIncognitoTab.getDividerTint());
    }

    @Test
    @Feature("Tab Strip Redesign")
    @EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
    public void testGetDividerTint_TabStripRedesignDetached() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        int expectedColor;

        // Normal.
        expectedColor = MaterialColors.getColor(mContext, R.attr.colorSurfaceVariant, TAG);
        assertEquals("TSR detached divider uses surface variant.", expectedColor,
                mNormalTab.getDividerTint());

        // Incognito.
        expectedColor = mContext.getColor(R.color.divider_line_bg_color_light);
        assertEquals("TSR incognito dividers use the baseline color.", expectedColor,
                mIncognitoTab.getDividerTint());
    }

    private StripLayoutTab createStripLayoutTab(boolean incognito) {
        return new StripLayoutTab(mContext, 0, null, null, null, incognito);
    }
}
