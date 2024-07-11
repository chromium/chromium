// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static junit.framework.TestCase.assertEquals;

import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.COLOR_DATA;

import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** On-device unit tests for {@link ToolbarBrandingOverlayViewBinder}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ToolbarBrandingOverlayViewBinderUnitTest extends BlankUiTestActivityTestCase {
    private PropertyModel mModel;
    private View mView;
    private ImageView mIcon;
    private TextView mText;

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().setContentView(R.layout.custom_tabs_toolbar_branding_layout);
                    mView = getActivity().findViewById(android.R.id.content);
                    mIcon = mView.findViewById(R.id.branding_icon);
                    mText = mView.findViewById(R.id.branding_text);

                    mModel =
                            new PropertyModel.Builder(ToolbarBrandingOverlayProperties.ALL_KEYS)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            mModel, mView, ToolbarBrandingOverlayViewBinder::bind);
                });
    }

    @Test
    @SmallTest
    public void testAppDefault() {
        var colorData =
                new ToolbarBrandingOverlayProperties.ColorData(
                        Color.WHITE, BrandedColorScheme.APP_DEFAULT);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(COLOR_DATA, colorData));

        assertEquals(Color.WHITE, ((ColorDrawable) mView.getBackground()).getColor());
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        getActivity(), BrandedColorScheme.APP_DEFAULT),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(getActivity(), BrandedColorScheme.APP_DEFAULT),
                mIcon.getImageTintList());
    }

    @Test
    @SmallTest
    public void testDarkBrandedTheme() {
        var colorData =
                new ToolbarBrandingOverlayProperties.ColorData(
                        Color.BLACK, BrandedColorScheme.DARK_BRANDED_THEME);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(COLOR_DATA, colorData));

        assertEquals(Color.BLACK, ((ColorDrawable) mView.getBackground()).getColor());
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        getActivity(), BrandedColorScheme.DARK_BRANDED_THEME),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(
                        getActivity(), BrandedColorScheme.DARK_BRANDED_THEME),
                mIcon.getImageTintList());
    }

    @Test
    @SmallTest
    public void testLightBrandedTheme() {
        var colorData =
                new ToolbarBrandingOverlayProperties.ColorData(
                        Color.WHITE, BrandedColorScheme.LIGHT_BRANDED_THEME);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(COLOR_DATA, colorData));

        assertEquals(Color.WHITE, ((ColorDrawable) mView.getBackground()).getColor());
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        getActivity(), BrandedColorScheme.LIGHT_BRANDED_THEME),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(
                        getActivity(), BrandedColorScheme.LIGHT_BRANDED_THEME),
                mIcon.getImageTintList());
    }

    @Test
    @SmallTest
    public void testIncognito() {
        var colorData =
                new ToolbarBrandingOverlayProperties.ColorData(
                        Color.DKGRAY, BrandedColorScheme.INCOGNITO);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(COLOR_DATA, colorData));

        assertEquals(Color.DKGRAY, ((ColorDrawable) mView.getBackground()).getColor());
        assertEquals(
                OmniboxResourceProvider.getUrlBarPrimaryTextColor(
                        getActivity(), BrandedColorScheme.INCOGNITO),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(getActivity(), BrandedColorScheme.INCOGNITO),
                mIcon.getImageTintList());
    }
}
