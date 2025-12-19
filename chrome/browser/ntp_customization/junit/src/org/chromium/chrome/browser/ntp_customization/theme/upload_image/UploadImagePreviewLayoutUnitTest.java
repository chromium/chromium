// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.constraintlayout.widget.Guideline;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.ntp_customization.R;

/** Unit tests for {@link UploadImagePreviewLayout}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UploadImagePreviewLayoutUnitTest {
    private Activity mActivity;
    private UploadImagePreviewLayout mLayout;
    private ImageView mLogoView;
    private Guideline mGuidelineTop;
    private Guideline mGuidelineBottom;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mLayout =
                (UploadImagePreviewLayout)
                        LayoutInflater.from(mActivity)
                                .inflate(
                                        R.layout.ntp_customization_theme_preview_dialog_layout,
                                        null);

        mLogoView = mLayout.findViewById(R.id.default_search_engine_logo);
        mGuidelineTop = mLayout.findViewById(R.id.guideline_top);
        mGuidelineBottom = mLayout.findViewById(R.id.guideline_bottom);
    }

    @Test
    public void testSetLogo_WithBitmap() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        mLayout.setLogo(bitmap);

        Drawable drawable = mLogoView.getDrawable();
        assertEquals("Bitmap should match", bitmap, ((BitmapDrawable) drawable).getBitmap());
    }

    @Test
    public void testSetLogo_NullBitmap() {
        // Call the method under test
        mLayout.setLogo(null);

        Drawable actual = mLogoView.getDrawable();
        Drawable expected = LogoUtils.getGoogleLogoDrawable(mActivity);

        // Verifies that the actual drawable comes from the same resource ID as the expected one.
        // We cannot use assertEquals(expected, actual) because they are different instances.
        int expectedResId = Shadows.shadowOf(expected).getCreatedFromResId();
        int actualResId = Shadows.shadowOf(actual).getCreatedFromResId();

        assertEquals("Drawable should come from the same resource ID", expectedResId, actualResId);
    }

    @Test
    public void testSetLogoVisibility() {
        mLayout.setLogoVisibility(View.VISIBLE);
        assertEquals(View.VISIBLE, mLogoView.getVisibility());

        mLayout.setLogoVisibility(View.GONE);
        assertEquals(View.GONE, mLogoView.getVisibility());
    }

    @Test
    public void testSetLogoViewLayoutParams() {
        int expectedHeight = 150;
        int expectedTopMargin = 40;

        mLayout.setLogoViewLayoutParams(expectedHeight, expectedTopMargin);

        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) mLogoView.getLayoutParams();

        assertEquals("Height should be updated", expectedHeight, params.height);
        assertEquals("Top margin should be updated", expectedTopMargin, params.topMargin);
    }

    @Test
    public void testSetTopInsets() {
        int logoHeight = 100;
        int logoTopMargin = 50;
        mLayout.setLogoViewLayoutParams(logoHeight, logoTopMargin);

        int topInsetAndToolBarHeight = 120;
        mLayout.setTopInsets(topInsetAndToolBarHeight);

        // Verifies the guideline was moved
        ConstraintLayout.LayoutParams params =
                (ConstraintLayout.LayoutParams) mGuidelineTop.getLayoutParams();

        assertEquals(
                "Guideline begin should be sum of inset and logo margin",
                topInsetAndToolBarHeight + logoTopMargin,
                params.guideBegin);
    }

    @Test
    public void testSetBottomInsets() {
        int inputBottomInset = 80;
        int resourceMargin =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.ntp_customization_back_button_margin_start);

        mLayout.setBottomInsets(inputBottomInset);

        ConstraintLayout.LayoutParams params =
                (ConstraintLayout.LayoutParams) mGuidelineBottom.getLayoutParams();

        assertEquals(
                "Guideline end should be sum of inset and resource margin",
                inputBottomInset + resourceMargin,
                params.guideEnd);
    }
}
