// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link UploadImagePreviewLayoutViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UploadImagePreviewLayoutViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UploadImagePreviewLayout mLayoutView;
    @Mock private CropImageView mCropImageView;
    @Mock private TextView mSaveButton;
    @Mock private TextView mCancelButton;
    @Mock private View.OnClickListener mOnClickListener;
    @Mock private View mLogoView;

    private PropertyModel mModel;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        mBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);

        when(mLayoutView.findViewById(R.id.preview_image)).thenReturn(mCropImageView);
        when(mLayoutView.findViewById(R.id.save_button)).thenReturn(mSaveButton);
        when(mLayoutView.findViewById(R.id.cancel_button)).thenReturn(mCancelButton);
        when(mLayoutView.findViewById(R.id.default_search_engine_logo)).thenReturn(mLogoView);

        mModel = new PropertyModel(NtpThemeProperty.PREVIEW_KEYS);
        PropertyModelChangeProcessor.create(
                mModel, mLayoutView, UploadImagePreviewLayoutViewBinder::bind);
    }

    @Test
    public void testSetBitmapForPreview() {
        mModel.set(NtpThemeProperty.BITMAP_FOR_PREVIEW, mBitmap);
        verify(mCropImageView).setImageBitmap(eq(mBitmap));
    }

    @Test
    public void testSetSaveClickListener() {
        mModel.set(NtpThemeProperty.PREVIEW_SAVE_CLICK_LISTENER, mOnClickListener);
        verify(mSaveButton).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetCancelClickListener() {
        mModel.set(NtpThemeProperty.PREVIEW_CANCEL_CLICK_LISTENER, mOnClickListener);
        verify(mCancelButton).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetLogoBitmap() {
        mModel.set(NtpThemeProperty.SET_LOGO_BITMAP, mBitmap);
        verify(mLayoutView).setLogo(eq(mBitmap));

        mModel.set(NtpThemeProperty.SET_LOGO_BITMAP, null);
        verify(mLayoutView).setLogo(eq(null));
    }

    @Test
    public void testSetLogoVisibility() {
        mModel.set(NtpThemeProperty.SET_LOGO_VISIBILITY, View.VISIBLE);
        verify(mLayoutView).setLogoVisibility(eq(View.VISIBLE));

        mModel.set(NtpThemeProperty.SET_LOGO_VISIBILITY, View.GONE);
        verify(mLayoutView).setLogoVisibility(eq(View.GONE));
    }

    @Test
    public void testSetLogoParams() {
        // Setup: Index 0 = height, Index 1 = topMargin
        int expectedHeight = 150;
        int expectedTopMargin = 40;
        int[] params = new int[] {expectedHeight, expectedTopMargin};

        ViewGroup.MarginLayoutParams initialParams = new ViewGroup.MarginLayoutParams(100, 100);
        initialParams.topMargin = 0;
        when(mLogoView.getLayoutParams()).thenReturn(initialParams);

        mModel.set(NtpThemeProperty.SET_LOGO_PARAMS, params);

        ArgumentCaptor<ViewGroup.LayoutParams> captor =
                ArgumentCaptor.forClass(ViewGroup.LayoutParams.class);
        verify(mLogoView).setLayoutParams(captor.capture());

        ViewGroup.MarginLayoutParams updatedParams =
                (ViewGroup.MarginLayoutParams) captor.getValue();

        assertEquals("Height should match the value", expectedHeight, updatedParams.height);
        assertEquals(
                "Top margin should match the value", expectedTopMargin, updatedParams.topMargin);
    }
}
