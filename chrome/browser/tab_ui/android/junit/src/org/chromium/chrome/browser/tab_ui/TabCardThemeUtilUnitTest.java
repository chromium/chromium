// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.ContextCompat;

import com.google.android.material.color.MaterialColors;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.components.tab_groups.TabGroupColorPickerUtils;

/** Unit tests for {@link TabCardThemeUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabCardThemeUtilUnitTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
    }

    @Test
    public void testGetChromeOwnedFaviconTintColor() {
        // This test verifies that the method correctly delegates to getTitleTextColor.
        @ColorInt
        int expectedSelected =
                TabCardThemeUtil.getTitleTextColor(
                        mContext,
                        /* isIncognito= */ false,
                        /* isSelected= */ true, /* colorId */
                        null);
        @ColorInt
        int actualSelected =
                TabCardThemeUtil.getChromeOwnedFaviconTintColor(
                        mContext, /* isIncognito= */ false, /* isSelected= */ true);
        assertEquals(
                "Selected favicon tint should match selected title text color.",
                expectedSelected,
                actualSelected);

        @ColorInt
        int expectedUnselected =
                TabCardThemeUtil.getTitleTextColor(
                        mContext,
                        /* isIncognito= */ false,
                        /* isSelected= */ false, /* colorId */
                        null);
        @ColorInt
        int actualUnselected =
                TabCardThemeUtil.getChromeOwnedFaviconTintColor(
                        mContext, /* isIncognito= */ false, /* isSelected= */ false);
        assertEquals(
                "Unselected favicon tint should match unselected title text color.",
                expectedUnselected,
                actualUnselected);
    }

    @Test
    public void testGetTitleTextColor_isSelected() {
        @ColorInt int expectedColor = MaterialColors.getColor(mContext, R.attr.colorOnPrimary, "");
        @ColorInt
        int actualColor =
                TabCardThemeUtil.getTitleTextColor(
                        mContext,
                        /* isIncognito= */ false,
                        /* isSelected= */ true, /* colorId */
                        null);
        assertEquals(
                "Selected title text color for non-incognito is incorrect.",
                expectedColor,
                actualColor);

        @ColorInt
        int expectedIncognitoColor = mContext.getColor(R.color.incognito_tab_title_selected_color);
        @ColorInt
        int actualIncognitoColor =
                TabCardThemeUtil.getTitleTextColor(
                        mContext,
                        /* isIncognito= */ true,
                        /* isSelected= */ true, /* colorId */
                        null);
        assertEquals(
                "Selected title text color for incognito is incorrect.",
                expectedIncognitoColor,
                actualIncognitoColor);
    }

    @Test
    public void testGetTitleTextColor_isNotSelected() {
        // This test verifies that the method correctly delegates to TabGroupColorPickerUtils.
        @TabGroupColorId int testColorId = TabGroupColorId.BLUE;
        @ColorInt
        int expectedColor =
                TabGroupColorPickerUtils.getTabGroupCardTextColor(
                        mContext, testColorId, /* isIncognito= */ false);
        @ColorInt
        int actualColor =
                TabCardThemeUtil.getTitleTextColor(
                        mContext, /* isIncognito= */ false, /* isSelected= */ false, testColorId);
        assertEquals(
                "Unselected title text color should be delegated to TabGroupColorPickerUtils.",
                expectedColor,
                actualColor);
    }

    @Test
    public void testGetMiniThumbnailPlaceholderColor_isSelected() {
        int alpha =
                mContext.getResources()
                        .getInteger(R.integer.tab_thumbnail_placeholder_selected_color_alpha);
        @ColorInt int baseColor = SemanticColorUtils.getColorOnPrimary(mContext);
        @ColorInt int expectedColor = MaterialColors.compositeARGBWithAlpha(baseColor, alpha);
        @ColorInt
        int actualColor =
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        mContext,
                        /* isIncognito= */ false,
                        /* isSelected= */ true, /* colorId */
                        null);
        assertEquals("Selected placeholder color is incorrect.", expectedColor, actualColor);

        @ColorInt
        int expectedIncognitoColor =
                mContext.getColor(R.color.incognito_tab_thumbnail_placeholder_selected_color);
        @ColorInt
        int actualIncognitoColor =
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        mContext,
                        /* isIncognito= */ true,
                        /* isSelected= */ true, /* colorId */
                        null);
        assertEquals(
                "Selected incognito placeholder color is incorrect.",
                expectedIncognitoColor,
                actualIncognitoColor);
    }

    @Test
    public void testGetMiniThumbnailPlaceholderColor_isNotSelected() {
        // This test verifies that the method correctly delegates to SurfaceColorUpdateUtils.
        @TabGroupColorId int testColorId = TabGroupColorId.GREEN;
        @ColorInt
        int expectedColor =
                TabGroupColorPickerUtils.getTabGroupCardMiniThumbnailPlaceholderColor(
                        mContext, testColorId, /* isIncognito= */ false);
        @ColorInt
        int actualColor =
                TabCardThemeUtil.getMiniThumbnailPlaceholderColor(
                        mContext, /* isIncognito= */ false, /* isSelected= */ false, testColorId);
        assertEquals(
                "Unselected placeholder color should delegate to SurfaceColorUpdateUtils.",
                expectedColor,
                actualColor);
    }

    @Test
    public void testGetCardViewBackgroundColor_isSelected() {
        @ColorInt int expectedColor = MaterialColors.getColor(mContext, R.attr.colorPrimary, "");
        @ColorInt
        int actualColor =
                TabCardThemeUtil.getCardViewBackgroundColor(
                        mContext,
                        /* isIncognito= */ false,
                        /* isSelected= */ true, /* colorId */
                        null);
        assertEquals("Selected background color is incorrect.", expectedColor, actualColor);

        @ColorInt
        int expectedIncognitoColor =
                ContextCompat.getColor(mContext, R.color.incognito_tab_bg_selected_color);
        @ColorInt
        int actualIncognitoColor =
                TabCardThemeUtil.getCardViewBackgroundColor(
                        mContext,
                        /* isIncognito= */ true,
                        /* isSelected= */ true, /* colorId */
                        null);
        assertEquals(
                "Selected incognito background color is incorrect.",
                expectedIncognitoColor,
                actualIncognitoColor);
    }

    @Test
    public void testGetCardViewBackgroundColor_isNotSelected() {
        // This test verifies that the method correctly delegates to TabGroupColorPickerUtils.
        @TabGroupColorId int testColorId = TabGroupColorId.RED;
        @ColorInt
        int expectedColor =
                TabGroupColorPickerUtils.getTabGroupCardColor(
                        mContext, testColorId, /* isIncognito= */ false);
        @ColorInt
        int actualColor =
                TabCardThemeUtil.getCardViewBackgroundColor(
                        mContext, /* isIncognito= */ false, /* isSelected= */ false, testColorId);
        assertEquals(
                "Unselected background color should be delegated to TabGroupColorPickerUtils.",
                expectedColor,
                actualColor);
    }

    @Test
    public void testGetTabGroupNumberTextColor_isSelected() {
        @ColorInt int expectedColor = MaterialColors.getColor(mContext, R.attr.colorOnPrimary, "");
        @ColorInt
        int actualColor =
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        mContext, /* isIncognito= */ false, /* isSelected= */ true, null);
        assertEquals(
                "Selected number text color for non-incognito is incorrect.",
                expectedColor,
                actualColor);

        @ColorInt
        int expectedIncognitoColor =
                mContext.getColor(R.color.incognito_tab_tile_number_selected_color);
        @ColorInt
        int actualIncognitoColor =
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        mContext, /* isIncognito= */ true, /* isSelected= */ true, null);
        assertEquals(
                "Selected number text color for incognito is incorrect.",
                expectedIncognitoColor,
                actualIncognitoColor);
    }

    @Test
    public void testGetTabGroupNumberTextColor_isNotSelected() {
        // This test verifies that the method correctly delegates to SurfaceColorUpdateUtils.
        @TabGroupColorId int testColorId = TabGroupColorId.CYAN;
        @ColorInt
        int expectedColor =
                TabGroupColorPickerUtils.getTabGroupCardTextColor(
                        mContext, testColorId, /* isIncognito= */ false);
        @ColorInt
        int actualColor =
                TabCardThemeUtil.getTabGroupNumberTextColor(
                        mContext, /* isIncognito= */ false, /* isSelected= */ false, testColorId);
        assertEquals(
                "Unselected number text color should delegate to SurfaceColorUpdateUtils.",
                expectedColor,
                actualColor);
    }

    @Test
    public void testGetActionButtonTintList_isSelected() {
        ColorStateList expectedColor =
                ColorStateList.valueOf(
                        MaterialColors.getColor(mContext, R.attr.colorOnPrimary, ""));
        ColorStateList actualColor =
                TabCardThemeUtil.getActionButtonTintList(
                        mContext, /* isIncognito= */ false, /* isSelected= */ true, null);
        assertEquals(
                "Selected action button tint for non-incognito is incorrect.",
                expectedColor,
                actualColor);

        ColorStateList expectedIncognitoColor =
                AppCompatResources.getColorStateList(
                        mContext, R.color.incognito_tab_action_button_selected_color);
        ColorStateList actualIncognitoColor =
                TabCardThemeUtil.getActionButtonTintList(
                        mContext, /* isIncognito= */ true, /* isSelected= */ true, null);
        assertEquals(
                "Selected action button tint for incognito is incorrect.",
                expectedIncognitoColor,
                actualIncognitoColor);
    }

    @Test
    public void testGetActionButtonTintList_isNotSelected() {
        // This test verifies that the method correctly delegates to SurfaceColorUpdateUtils.
        @TabGroupColorId int testColorId = TabGroupColorId.ORANGE;
        ColorStateList expectedColor =
                ColorStateList.valueOf(
                        TabGroupColorPickerUtils.getTabGroupCardTextColor(
                                mContext, testColorId, /* isIncognito= */ false));
        ColorStateList actualColor =
                TabCardThemeUtil.getActionButtonTintList(
                        mContext, /* isIncognito= */ false, /* isSelected= */ false, testColorId);
        assertEquals(
                "Unselected action button tint should delegate to SurfaceColorUpdateUtils.",
                expectedColor,
                actualColor);
    }

    @Test
    public void testGetToggleActionButtonBackgroundTintList() {
        // This test verifies that the method correctly delegates to getActionButtonTintList.
        ColorStateList expected =
                TabCardThemeUtil.getActionButtonTintList(
                        mContext,
                        /* isIncognito= */ false,
                        /* isSelected= */ true,
                        /* colorId= */ null);
        ColorStateList actual =
                TabCardThemeUtil.getToggleActionButtonBackgroundTintList(
                        mContext, /* isIncognito= */ false, /* isSelected= */ true);
        assertEquals(
                "Toggle action button tint should delegate to the main action button tint method.",
                expected,
                actual);
    }
}
