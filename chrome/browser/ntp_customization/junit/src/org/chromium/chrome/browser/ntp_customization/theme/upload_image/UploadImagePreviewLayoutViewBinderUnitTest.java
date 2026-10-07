// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.logo.LogoUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link UploadImagePreviewLayoutViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class UploadImagePreviewLayoutViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnClickListener mOnClickListener;

    private UploadImagePreviewLayout mLayoutView;
    private ImageView mLogoView;
    private PropertyModel mModel;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        mBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);

        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mLayoutView =
                (UploadImagePreviewLayout)
                        LayoutInflater.from(activity)
                                .inflate(
                                        R.layout.ntp_customization_theme_preview_dialog_layout,
                                        null);
        mLogoView = mLayoutView.findViewById(R.id.default_search_engine_logo);

        mModel = new PropertyModel(NtpThemeProperty.PREVIEW_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mLayoutView, UploadImagePreviewLayoutViewBinder::bind);
    }

    @Test
    public void testSetBitmapForPreview() {
        mModel.set(NtpThemeProperty.BITMAP_FOR_PREVIEW, mBitmap);
        CropImageView cropImageView = mLayoutView.findViewById(R.id.preview_image);
        assertEquals(mBitmap, ((BitmapDrawable) cropImageView.getDrawable()).getBitmap());
    }

    @Test
    public void testSetSaveClickListener() {
        mModel.set(NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER, mOnClickListener);
        View saveButton = mLayoutView.findViewById(R.id.save_button);
        saveButton.performClick();
        verify(mOnClickListener).onClick(saveButton);
    }

    @Test
    public void testSetCancelClickListener() {
        mModel.set(NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER, mOnClickListener);
        View cancelButton = mLayoutView.findViewById(R.id.cancel_button);
        cancelButton.performClick();
        verify(mOnClickListener).onClick(cancelButton);
    }

    @Test
    public void testSetLogoBitmap() {
        mModel.set(NtpThemeProperty.LOGO_BITMAP, mBitmap);
        assertEquals(mBitmap, ((BitmapDrawable) mLogoView.getDrawable()).getBitmap());

        // A null bitmap falls back to the default Google logo.
        mModel.set(NtpThemeProperty.LOGO_BITMAP, null);
        assertEquals(
                Shadows.shadowOf(LogoUtils.getGoogleLogoDrawable(mLayoutView.getContext()))
                        .getCreatedFromResId(),
                Shadows.shadowOf(mLogoView.getDrawable()).getCreatedFromResId());
    }

    @Test
    public void testSetLogoVisibility() {
        mModel.set(NtpThemeProperty.LOGO_VISIBILITY, View.VISIBLE);
        assertEquals(View.VISIBLE, mLogoView.getVisibility());

        mModel.set(NtpThemeProperty.LOGO_VISIBILITY, View.GONE);
        assertEquals(View.GONE, mLogoView.getVisibility());
    }

    @Test
    public void testSetLogoParams() {
        // Setup: Index 0 = height, Index 1 = topMargin
        int expectedHeight = 150;
        int expectedTopMargin = 40;
        int[] params = new int[] {expectedHeight, expectedTopMargin};

        mModel.set(NtpThemeProperty.LOGO_PARAMS, params);

        ViewGroup.MarginLayoutParams logoParams =
                (ViewGroup.MarginLayoutParams) mLogoView.getLayoutParams();
        assertEquals(expectedHeight, logoParams.height);
        assertEquals(expectedTopMargin, logoParams.topMargin);
    }

    @Test
    public void testSetLogoSearchBoxMargin() {
        int expectedMargin = 45;
        mModel.set(NtpThemeProperty.SEARCH_BOX_TOP_MARGIN, expectedMargin);

        assertEquals(expectedMargin, getSearchBoxContainerParams().topMargin);
    }

    @Test
    public void testSetSearchBoxContainerHeight() {
        int expectedHeight = 56;
        mModel.set(NtpThemeProperty.SEARCH_BOX_HEIGHT, expectedHeight);

        assertEquals(expectedHeight, getSearchBoxContainerParams().height);
    }

    @Test
    public void testTopGuidelineBegin() {
        int topMargin = 105;
        mModel.set(NtpThemeProperty.TOP_GUIDELINE_BEGIN, topMargin);
        ConstraintLayout.LayoutParams params =
                (ConstraintLayout.LayoutParams)
                        mLayoutView.findViewById(R.id.guideline_top).getLayoutParams();
        assertEquals(topMargin, params.guideBegin);
    }

    @Test
    public void testSetSideAndBottomInsets() {
        int originalTopPadding = 7;
        mLayoutView.setPadding(0, originalTopPadding, 0, 0);
        Rect expectedInsets =
                new Rect(/* left= */ 10, /* top= */ 0, /* right= */ 20, /* bottom= */ 30);
        mModel.set(NtpThemeProperty.SIDE_AND_BOTTOM_INSETS, expectedInsets);
        assertEquals(10, mLayoutView.getPaddingLeft());
        assertEquals(originalTopPadding, mLayoutView.getPaddingTop());
        assertEquals(20, mLayoutView.getPaddingRight());
        assertEquals(30, mLayoutView.getPaddingBottom());
    }

    private ViewGroup.MarginLayoutParams getSearchBoxContainerParams() {
        return (ViewGroup.MarginLayoutParams)
                mLayoutView.findViewById(R.id.search_box_container).getLayoutParams();
    }
}
