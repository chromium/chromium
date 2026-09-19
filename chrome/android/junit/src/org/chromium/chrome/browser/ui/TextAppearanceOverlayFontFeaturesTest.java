// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.content.res.TypedArray;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;

/**
 * Tests that ligature-forming OpenType features stay disabled for text styles that live outside
 * //ui/android, and so are not covered by {@code TextAppearanceFontFeaturesTest}.
 *
 * <p>Two styles escape the base hierarchy:
 *
 * <ul>
 *   <li>//chrome/android overlays the root {@code TextAppearance} for API 33+. An aapt2 resource
 *       overlay replaces a style wholesale rather than merging items into it, so that copy has to
 *       repeat every item. Editing the base style in //ui/android without updating the overlay
 *       silently drops the setting on every modern device, which is the most likely way this change
 *       regresses.
 *   <li>{@code TextAppearance.AlertDialogTitleStyle} parents to an AppCompat style rather than to
 *       {@code TextAppearance}, despite its name, so it inherits nothing from the root.
 * </ul>
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TextAppearanceOverlayFontFeaturesTest {
    private static final int[] FONT_FEATURE_SETTINGS = {android.R.attr.fontFeatureSettings};
    private static final int[] FONT_FAMILY = {android.R.attr.fontFamily};

    /** The literal font family hard-coded by the base style in //ui/android. */
    private static final String BASE_FONT_FAMILY = "sans-serif";

    private Context mContext;
    private String mExpectedSettings;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mExpectedSettings = mContext.getString(R.string.default_font_feature_settings);
    }

    private String attrOf(int styleRes, int[] attr) {
        TypedArray a = mContext.getTheme().obtainStyledAttributes(styleRes, attr);
        try {
            return a.getString(0);
        } finally {
            a.recycle();
        }
    }

    @Test
    @Config(sdk = 33)
    public void v33OverlayDisablesLigatures() {
        assertEquals(
                "The API 33+ overlay of TextAppearance in //chrome/android must repeat"
                        + " android:fontFeatureSettings. The overlay replaces the base style"
                        + " rather than merging with it, so editing only"
                        + " //ui/android/java/res/values/styles.xml leaves modern devices"
                        + " unprotected.",
                mExpectedSettings,
                attrOf(R.style.TextAppearance, FONT_FEATURE_SETTINGS));
    }

    @Test
    @Config(sdk = 30)
    public void baseTextAppearanceDisablesLigaturesBelowV33() {
        assertEquals(mExpectedSettings, attrOf(R.style.TextAppearance, FONT_FEATURE_SETTINGS));
    }

    /**
     * Canary proving {@link #v33OverlayDisablesLigatures} is not vacuous.
     *
     * <p>The two copies of the style differ only in how they pick the font family: the base style
     * hard-codes "sans-serif" while the overlay indirects through {@code ?attr/defaultFontFamily}.
     * Read against a bare application theme, which does not define that attribute, the overlay
     * therefore yields something other than the literal. If this assertion fails with "sans-serif",
     * the resource overlay is not being applied in the Robolectric environment, and {@link
     * #v33OverlayDisablesLigatures} is silently reading the base style instead of the overlay -- so
     * it would not catch the regression it exists to catch.
     */
    @Test
    @Config(sdk = 33)
    public void v33OverlayIsActuallyApplied() {
        assertNotEquals(
                "Expected the API 33+ resource overlay to replace the base TextAppearance, but"
                        + " the base style's literal font family came back. The overlay is not"
                        + " applied here, so v33OverlayDisablesLigatures is not testing the"
                        + " overlay.",
                BASE_FONT_FAMILY,
                attrOf(R.style.TextAppearance, FONT_FAMILY));
    }

    @Test
    @Config(sdk = 30)
    public void baseTextAppearanceUsesLiteralFontFamilyBelowV33() {
        // The other half of the canary: below API 33 the overlay must not apply.
        assertEquals(BASE_FONT_FAMILY, attrOf(R.style.TextAppearance, FONT_FAMILY));
    }

    @Test
    public void alertDialogTitleStyleDisablesLigatures() {
        assertEquals(
                "TextAppearance.AlertDialogTitleStyle parents to an AppCompat style, not to"
                        + " TextAppearance, so it has to set android:fontFeatureSettings itself."
                        + " It also selects the accent font, which is where the problem"
                        + " ligatures live.",
                mExpectedSettings,
                attrOf(
                        org.chromium.components.browser_ui.styles.R.style
                                .TextAppearance_AlertDialogTitleStyle,
                        FONT_FEATURE_SETTINGS));
    }
}
