// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static junit.framework.TestCase.assertEquals;

import static org.chromium.chrome.browser.customtabs.features.branding.ToolbarBrandingOverlayProperties.COLOR_DATA;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** On-device unit tests for {@link ToolbarBrandingOverlayViewBinder}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ToolbarBrandingOverlayViewBinderUnitTest {
    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private PropertyModel mModel;
    private View mView;
    private ImageView mIcon;
    private TextView mText;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sActivity = ThreadUtils.runOnUiThreadBlocking(() -> sActivityTestRule.getActivity());
    }

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.setContentView(R.layout.custom_tabs_toolbar_branding_layout);
                    mView = sActivity.findViewById(android.R.id.content);
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
                        sActivity, BrandedColorScheme.APP_DEFAULT),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(sActivity, BrandedColorScheme.APP_DEFAULT),
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
                        sActivity, BrandedColorScheme.DARK_BRANDED_THEME),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(
                        sActivity, BrandedColorScheme.DARK_BRANDED_THEME),
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
                        sActivity, BrandedColorScheme.LIGHT_BRANDED_THEME),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(
                        sActivity, BrandedColorScheme.LIGHT_BRANDED_THEME),
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
                        sActivity, BrandedColorScheme.INCOGNITO),
                mText.getTextColors().getDefaultColor());
        assertEquals(
                ThemeUtils.getThemedToolbarIconTint(sActivity, BrandedColorScheme.INCOGNITO),
                mIcon.getImageTintList());
    }
}
