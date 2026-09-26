// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.COLOR_DATA;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link ToolbarBrandingOverlayViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ToolbarBrandingOverlayViewBinderUnitTest {
    private Activity mActivity;
    private PropertyModel mModel;
    private View mView;
    private ImageView mIcon;
    private TextView mText;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mActivity.setContentView(R.layout.custom_tabs_toolbar_branding_layout);
        mView = mActivity.findViewById(android.R.id.content);
        mIcon = mView.findViewById(R.id.branding_icon);
        mText = mView.findViewById(R.id.branding_text);

        mModel = new PropertyModel.Builder(ToolbarBrandingOverlayProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, ToolbarBrandingOverlayViewBinder::bind);
    }

    @Test
    public void testAppDefault() {
        var colorData =
                new ToolbarBrandingOverlayProperties.ColorData(
                        Color.WHITE, BrandedColorScheme.APP_DEFAULT);
        mModel.set(COLOR_DATA, colorData);

        assertEquals(Color.WHITE, ((ColorDrawable) mView.getBackground()).getColor());
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, BrandedColorScheme.APP_DEFAULT),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(mActivity, BrandedColorScheme.APP_DEFAULT),
                mIcon.getImageTintList());
    }

    @Test
    public void testDarkBrandedTheme() {
        var colorData =
                new ToolbarBrandingOverlayProperties.ColorData(
                        Color.BLACK, BrandedColorScheme.DARK_BRANDED_THEME);
        mModel.set(COLOR_DATA, colorData);

        assertEquals(Color.BLACK, ((ColorDrawable) mView.getBackground()).getColor());
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(
                        mActivity, BrandedColorScheme.DARK_BRANDED_THEME),
                mIcon.getImageTintList());
    }

    @Test
    public void testLightBrandedTheme() {
        var colorData =
                new ToolbarBrandingOverlayProperties.ColorData(
                        Color.WHITE, BrandedColorScheme.LIGHT_BRANDED_THEME);
        mModel.set(COLOR_DATA, colorData);

        assertEquals(Color.WHITE, ((ColorDrawable) mView.getBackground()).getColor());
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(
                        mActivity, BrandedColorScheme.LIGHT_BRANDED_THEME),
                mIcon.getImageTintList());
    }

    @Test
    public void testIncognito() {
        var colorData =
                new ToolbarBrandingOverlayProperties.ColorData(
                        Color.DKGRAY, BrandedColorScheme.INCOGNITO);
        mModel.set(COLOR_DATA, colorData);

        assertEquals(Color.DKGRAY, ((ColorDrawable) mView.getBackground()).getColor());
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        mActivity, BrandedColorScheme.INCOGNITO),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(mActivity, BrandedColorScheme.INCOGNITO),
                mIcon.getImageTintList());
    }
}
