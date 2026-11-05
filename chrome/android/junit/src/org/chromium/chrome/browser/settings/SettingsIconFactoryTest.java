// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.view.Gravity;

import androidx.annotation.ColorRes;
import androidx.test.core.app.ApplicationProvider;

import com.google.android.material.color.MaterialColors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.SettingsIconFactory.IconColor;

/** Unit tests for {@link SettingsIconFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsIconFactoryTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mContext.setTheme(R.style.Theme_Chromium_Settings);
    }

    @Test
    public void testCreate_layersCircleAndCenteredGlyph() {
        Drawable drawable =
                SettingsIconFactory.create(
                        mContext, R.drawable.settings_search_24dp, IconColor.PURPLE);

        assertTrue(drawable instanceof LayerDrawable);
        LayerDrawable icon = (LayerDrawable) drawable;
        assertEquals(2, icon.getNumberOfLayers());
        assertTrue(icon.getDrawable(0) instanceof GradientDrawable);

        int circleSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.settings_row_icon_size);
        int glyphSize =
                mContext.getResources().getDimensionPixelSize(R.dimen.settings_row_icon_glyph_size);
        assertEquals(circleSize, icon.getLayerWidth(0));
        assertEquals(circleSize, icon.getLayerHeight(0));
        assertEquals(glyphSize, icon.getLayerWidth(1));
        assertEquals(glyphSize, icon.getLayerHeight(1));
        assertEquals(Gravity.CENTER, icon.getLayerGravity(1));
        assertEquals(circleSize, icon.getIntrinsicWidth());
    }

    @Test
    public void testCreate_circleUsesContainerColorOfHue() {
        assertCircleColor(IconColor.BLUE, harmonized(R.color.settings_icon_blue_container));
        assertCircleColor(IconColor.PURPLE, harmonized(R.color.settings_icon_purple_container));
        assertCircleColor(IconColor.PINK, harmonized(R.color.settings_icon_pink_container));
        assertCircleColor(IconColor.ORANGE, harmonized(R.color.settings_icon_orange_container));
        assertCircleColor(IconColor.CYAN, harmonized(R.color.settings_icon_cyan_container));
        assertCircleColor(
                IconColor.NEUTRAL,
                MaterialColors.getColor(mContext, R.attr.colorSurfaceContainerHighest, "test"));
    }

    @Test
    public void testGetOnContainerColor() {
        assertEquals(
                harmonized(R.color.settings_icon_on_pink_container),
                SettingsIconFactory.getOnContainerColor(mContext, IconColor.PINK));
        assertEquals(
                MaterialColors.getColor(mContext, R.attr.colorOnSurfaceVariant, "test"),
                SettingsIconFactory.getOnContainerColor(mContext, IconColor.NEUTRAL));
    }

    @Test
    public void testCreate_glyphIsMutated() {
        LayerDrawable first =
                (LayerDrawable)
                        SettingsIconFactory.create(
                                mContext, R.drawable.settings_search_24dp, IconColor.PURPLE);
        LayerDrawable second =
                (LayerDrawable)
                        SettingsIconFactory.create(
                                mContext, R.drawable.settings_search_24dp, IconColor.CYAN);

        // Tinting one icon's glyph must not change another icon built from the same resource.
        assertNotSame(first.getDrawable(1), second.getDrawable(1));
    }

    @Test
    public void testCreateForKey_mappedRowGetsIcon() {
        assertTrue(
                SettingsIconFactory.createForKey(mContext, MainSettings.PREF_PRIVACY)
                        instanceof LayerDrawable);
    }

    @Test
    public void testCreateForKey_unmappedRowsKeepTheirOwnIcon() {
        assertNull(SettingsIconFactory.createForKey(mContext, MainSettings.PREF_SIGN_IN));
        assertNull(SettingsIconFactory.createForKey(mContext, MainSettings.PREF_GOOGLE_SERVICES));
        assertNull(SettingsIconFactory.createForKey(mContext, "unknown_key"));
    }

    @Test
    public void testCreateForKey_colorOverride() {
        LayerDrawable icon =
                (LayerDrawable)
                        SettingsIconFactory.createForKey(
                                mContext, MainSettings.PREF_PRIVACY, IconColor.PINK);
        GradientDrawable circle = (GradientDrawable) icon.getDrawable(0);
        assertEquals(
                harmonized(R.color.settings_icon_pink_container),
                circle.getColor().getDefaultColor());
        assertNull(
                SettingsIconFactory.createForKey(
                        mContext, MainSettings.PREF_SIGN_IN, IconColor.PINK));
    }

    /** The fixed hues are harmonized with the theme's primary color. */
    private int harmonized(@ColorRes int colorRes) {
        return MaterialColors.harmonizeWithPrimary(mContext, mContext.getColor(colorRes));
    }

    private void assertCircleColor(@IconColor int iconColor, int expected) {
        LayerDrawable icon =
                (LayerDrawable)
                        SettingsIconFactory.create(
                                mContext, R.drawable.settings_search_24dp, iconColor);
        GradientDrawable circle = (GradientDrawable) icon.getDrawable(0);
        assertEquals(expected, circle.getColor().getDefaultColor());
    }
}
