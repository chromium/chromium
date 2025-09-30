// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil.FOLIO_FOOT_LENGTH_DP;

import android.content.Context;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.core.content.res.ResourcesCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTabDelegate.VisualState;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.util.ColorUtils;

/** Tests for {@link StripLayoutTab}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripLayoutTabTest {

    private static final String TAG = "StripLayoutTabTest";
    private static final float DIVIDER_FOLIO_LIGHT_OPACITY = 0.3f;

    private Context mContext;
    private StripLayoutTab mNormalTab;
    private StripLayoutTab mIncognitoTab;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mNormalTab = createStripLayoutTab(false);
        mIncognitoTab = createStripLayoutTab(true);
    }

    @Test
    public void testGetTint() {
        @ColorInt int expectedColor;

        // Normal active tab color.
        mNormalTab.setVisualState(VisualState.SELECTED);
        expectedColor = ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false);
        assertEquals(
                "Normal active folio should match the Surface-0 color.",
                expectedColor,
                mNormalTab.getTint());

        // Normal inactive tab color.
        mNormalTab.setVisualState(VisualState.NORMAL);
        expectedColor = SemanticColorUtils.getDefaultBgColor(mContext);
        assertEquals(
                "Folio inactive tab containers should be Surface-0.",
                expectedColor,
                mNormalTab.getTint());

        // Normal inactive tab hover color.
        mNormalTab.setVisualState(VisualState.HOVERED);
        expectedColor =
                ColorUtils.setAlphaComponentWithFloat(
                        ChromeSemanticColorUtils.getTabInactiveHoverColor(mContext),
                        ResourcesCompat.getFloat(
                                mContext.getResources(),
                                R.dimen.tsr_folio_tab_inactive_hover_alpha));
        assertEquals(
                "Normal hovered inactive folio should be Primary @ 8%.",
                expectedColor, mNormalTab.getTint());

        // Incognito active tab color.
        mIncognitoTab.setVisualState(VisualState.SELECTED);
        expectedColor = mContext.getColor(R.color.toolbar_background_primary_dark);
        assertEquals(
                "Incognito active folio should match the baseline color.",
                expectedColor,
                mIncognitoTab.getTint());

        // Incognito inactive tab color.
        mIncognitoTab.setVisualState(VisualState.NORMAL);
        expectedColor = mContext.getColor(R.color.default_bg_color_dark);
        assertEquals(
                "Incognito inactive folio should be baseline Surface-0.",
                expectedColor,
                mIncognitoTab.getTint());

        // Incognito inactive tab hover color.
        mIncognitoTab.setVisualState(VisualState.HOVERED);
        expectedColor =
                ColorUtils.setAlphaComponentWithFloat(
                        mContext.getColor(R.color.baseline_primary_80),
                        ResourcesCompat.getFloat(
                                mContext.getResources(),
                                R.dimen.tsr_folio_tab_inactive_hover_alpha));
        assertEquals(
                "Incognito hovered inactive folio should be the baseline equivalent of Primary @"
                        + " 8%.",
                expectedColor, mIncognitoTab.getTint());
    }

    @Test
    public void testGetTint_Startup() {
        @ColorInt int expectedColor;

        mNormalTab.setIsPlaceholder(true);
        mIncognitoTab.setIsPlaceholder(true);

        // Normal active tab color.
        mNormalTab.setVisualState(VisualState.SELECTED);
        expectedColor = ChromeColors.getDefaultThemeColor(mContext, /* isIncognito= */ false);
        assertEquals(
                "Normal active should match the regular foreground color.",
                expectedColor,
                mNormalTab.getTint());

        // Normal inactive tab color.
        mNormalTab.setVisualState(VisualState.PLACEHOLDER);
        expectedColor = mContext.getColor(R.color.bg_tabstrip_tab_folio_startup_tint);
        assertEquals(
                "Normal inactive tab should match the placeholder color.",
                expectedColor,
                mNormalTab.getTint());

        // Incognito active tab color.
        mIncognitoTab.setVisualState(VisualState.SELECTED);
        expectedColor = ChromeColors.getDefaultThemeColor(mContext, true);
        assertEquals(
                "Incognito active should match the regular foreground color.",
                expectedColor,
                mIncognitoTab.getTint());

        // Incognito inactive tab color.
        mIncognitoTab.setVisualState(VisualState.PLACEHOLDER);
        expectedColor = mContext.getColor(R.color.bg_tabstrip_tab_folio_startup_tint);
        assertEquals(
                "Incognito inactive tab should match the placeholder color.",
                expectedColor,
                mIncognitoTab.getTint());
    }

    @Test
    public void testGetDividerTint() {
        @ColorInt int expectedColor;

        // Normal.
        expectedColor =
                ColorUtils.setAlphaComponentWithFloat(
                        SemanticColorUtils.getDefaultIconColorAccent1(mContext),
                        DIVIDER_FOLIO_LIGHT_OPACITY);
        assertEquals(
                "Light mode divider uses 30% primary color",
                expectedColor, mNormalTab.getDividerTint());

        // Incognito.
        expectedColor = mContext.getColor(R.color.tab_strip_tablet_divider_bg_incognito);
        assertEquals(
                "Incognito dividers use the baseline color.",
                expectedColor,
                mIncognitoTab.getDividerTint());
    }

    @Test
    @Config(qualifiers = "night")
    public void testGetDividerTint_Night() {
        @ColorInt int expectedColor;

        // Normal.
        expectedColor = SemanticColorUtils.getDividerLineBgColor(mContext);
        assertEquals(
                "Night mode divider uses colorOutlineVariant.",
                expectedColor,
                mNormalTab.getDividerTint());

        // Incognito.
        expectedColor = mContext.getColor(R.color.tab_strip_tablet_divider_bg_incognito);
        assertEquals(
                "Incognito dividers use the baseline color.",
                expectedColor,
                mIncognitoTab.getDividerTint());
    }

    @Test
    public void testNeedsA11yUpdate_TitleChanged() {
        final int resId = 1;
        mNormalTab.setAccessibilityDescription("", "Foo", resId);
        assertTrue(
                "New titles should result in a description update",
                mNormalTab.needsAccessibilityDescriptionUpdate("Bar", resId));
    }

    @Test
    public void testNeedsA11yUpdate_ResourceIdChanged() {
        final String title = "Tab 1";
        mNormalTab.setAccessibilityDescription("", title, 1);
        assertTrue(
                "New resource IDs should result in a description update",
                mNormalTab.needsAccessibilityDescriptionUpdate(title, 2));
    }

    @Test
    public void testNeedsA11yUpdate_TitleAndResourceIdChanged() {
        mNormalTab.setAccessibilityDescription("", "Tab 1", 1);
        assertTrue(
                "A new title and resource ID should result in a description update",
                mNormalTab.needsAccessibilityDescriptionUpdate("Foo", 2));
    }

    @Test
    public void testNeedsA11yUpdate_TitleAndResourceIdUnchanged() {
        final String title = "Tab 1";
        final int resId = 1;
        mNormalTab.setAccessibilityDescription("", title, resId);
        assertFalse(
                "An identical title and resource ID should not result in a description update",
                mNormalTab.needsAccessibilityDescriptionUpdate(title, resId));
    }

    @Test
    public void testNeedsA11yUpdate_NullInitialTitle() {
        final int resId = 1;
        mNormalTab.setAccessibilityDescription("", null, resId);
        assertTrue(
                "Going from a null to non-null title should result in a description update",
                mNormalTab.needsAccessibilityDescriptionUpdate("Bar", resId));
    }

    @Test
    public void testNeedsA11yUpdate_NullNewTitle() {
        final int resId = 1;
        mNormalTab.setAccessibilityDescription("", "Foo", resId);
        assertTrue(
                "Going from a non-null to null title should result in a description update",
                mNormalTab.needsAccessibilityDescriptionUpdate(null, resId));
    }

    @Test
    @SmallTest
    public void testAnchorRect() {
        int folioFootLengthPx =
                Math.round(
                        mContext.getResources().getDisplayMetrics().density * FOLIO_FOOT_LENGTH_DP);
        int widthWithoutFolio = 20;
        int width = folioFootLengthPx + widthWithoutFolio; // Should be > than folioFootLengthPx
        int height = 10; // Arbitrary
        mNormalTab.setWidth(width);
        mNormalTab.setHeight(10);

        Rect rect = new Rect();
        mNormalTab.getAnchorRect(rect);
        assertEquals(new Rect(folioFootLengthPx, 0, widthWithoutFolio, height), rect);
    }

    private StripLayoutTab createStripLayoutTab(boolean incognito) {
        return new StripLayoutTab(mContext, 0, null, null, null, null, incognito, false);
    }
}
