// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;

import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link ComposeplateViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ComposeplateViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ComposeplateView mViewMock;
    @Mock private ImageView mVoiceSearchButtonView;
    @Mock private ImageView mLensButtonView;
    @Mock private ImageView mIncognitoButtonView;
    @Mock private View mComposeplateButtonView;
    @Mock private OnClickListener mOnClickListener;

    private Context mContext;
    private PropertyModel mPropertyModel;
    private ComposeplateView mView;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                (ComposeplateView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.composeplate_view_layout_v2, null);

        mPropertyModel = new PropertyModel.Builder(ComposeplateProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mPropertyModel, mViewMock, ComposeplateViewBinder::bind);
    }

    @Test
    public void testSetVisibility() {
        mPropertyModel.set(ComposeplateProperties.IS_VISIBLE, true);
        verify(mViewMock).setVisibility(eq(View.VISIBLE));

        mPropertyModel.set(ComposeplateProperties.IS_VISIBLE, false);
        verify(mViewMock).setVisibility(eq(View.GONE));
    }

    @Test
    public void testSetVoiceSearchButtonClickListener() {
        when(mViewMock.findViewById(eq(R.id.voice_search_button)))
                .thenReturn(mVoiceSearchButtonView);
        mPropertyModel.set(ComposeplateProperties.VOICE_SEARCH_CLICK_LISTENER, mOnClickListener);
        verify(mVoiceSearchButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetLensButtonClickListener() {
        when(mViewMock.findViewById(eq(R.id.lens_camera_button))).thenReturn(mLensButtonView);
        mPropertyModel.set(ComposeplateProperties.LENS_CLICK_LISTENER, mOnClickListener);
        verify(mLensButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetIncognitoButtonClickListener() {
        when(mViewMock.findViewById(eq(R.id.incognito_button))).thenReturn(mIncognitoButtonView);
        mPropertyModel.set(ComposeplateProperties.INCOGNITO_CLICK_LISTENER, mOnClickListener);
        verify(mIncognitoButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetComposeplateButtonClickListener() {
        when(mViewMock.findViewById(eq(R.id.composeplate_button)))
                .thenReturn(mComposeplateButtonView);
        mPropertyModel.set(
                ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER, mOnClickListener);
        verify(mComposeplateButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testApplyWhiteBackgroundWithShadow_withMockView() {
        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, true);
        verify(mViewMock).applyWhiteBackgroundWithShadow(eq(true));

        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, false);
        verify(mViewMock).applyWhiteBackgroundWithShadow(eq(false));
    }

    @Test
    public void testApplyWhiteBackgroundWithShadow() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);

        float expectedElevation =
                mContext.getResources().getDimensionPixelSize(R.dimen.ntp_search_box_elevation);
        assertNotEquals(0, Float.compare(0f, expectedElevation));
        Drawable defaultBackground =
                mContext.getDrawable(R.drawable.home_surface_search_box_background);
        int paddingForShadowPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.composeplate_view_button_padding_for_shadow);

        View composeplateButton = mView.findViewById(R.id.composeplate_button);
        View incognitoButton = mView.findViewById(R.id.incognito_button);

        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, true);
        verifyApplyBackground(composeplateButton, expectedElevation);
        verifyApplyBackground(incognitoButton, expectedElevation);
        assertEquals(paddingForShadowPx, mView.getPaddingStart());
        assertEquals(paddingForShadowPx, mView.getPaddingEnd());

        mPropertyModel.set(ComposeplateProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW, false);
        verifyResetBackground(composeplateButton, defaultBackground);
        verifyResetBackground(incognitoButton, defaultBackground);
        assertEquals(0, mView.getPaddingStart());
        assertEquals(0, mView.getPaddingEnd());
    }

    @Test
    public void testSetColorStateList_withMockView() {
        ColorStateList colorStateList = mock(ColorStateList.class);
        mPropertyModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);
        verify(mViewMock).setColorStateList(eq(colorStateList));
    }

    @Test
    public void testSetColorStateList() {
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);

        ColorStateList colorStateList =
                AppCompatResources.getColorStateList(mContext, R.color.default_red);
        mPropertyModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);

        ImageView composeplateIconView = mView.findViewById(R.id.composeplate_button_icon);
        assertEquals(colorStateList, composeplateIconView.getImageTintList());

        ImageView incognitoIconView = mView.findViewById(R.id.incognito_button_icon);
        assertEquals(colorStateList, incognitoIconView.getImageTintList());
    }

    @Test
    public void testSetColorStateList_v1() {
        mView =
                (ComposeplateView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.composeplate_view_layout, null);
        // Bind PropertyModel with mView.
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);

        ColorStateList colorStateList =
                AppCompatResources.getColorStateList(mContext, R.color.default_red);
        mPropertyModel.set(ComposeplateProperties.COLOR_STATE_LIST, colorStateList);

        ImageView incognitoButton = mView.findViewById(R.id.incognito_button);
        assertEquals(colorStateList, incognitoButton.getImageTintList());

        ImageView voiceSearchButton = mView.findViewById(R.id.voice_search_button);
        assertEquals(colorStateList, voiceSearchButton.getImageTintList());

        ImageView lensButton = mView.findViewById(R.id.lens_camera_button);
        assertEquals(colorStateList, lensButton.getImageTintList());
    }

    @Test
    public void testSetTextStyle() {
        @StyleRes int textStyleResId = R.style.TextAppearance_MediumStyle;
        mPropertyModel.set(ComposeplateProperties.TEXT_STYLE_RES_ID, textStyleResId);
        verify(mViewMock).setTextStyle(eq(textStyleResId));
    }

    private void verifyApplyBackground(View view, float elevation) {
        assertEquals(0, Float.compare(elevation, view.getElevation()));
        assertTrue(view.getClipToOutline());
        // Verifies that the background is set to color white.
        Drawable whiteBackground = view.getBackground();
        assertTrue(whiteBackground instanceof GradientDrawable);
        assertEquals(
                Color.WHITE, ((GradientDrawable) whiteBackground).getColor().getDefaultColor());
    }

    private void verifyResetBackground(View view, Drawable defaultBackground) {
        assertEquals(0, Float.compare(0f, view.getElevation()));
        assertFalse(view.getClipToOutline());
        // Verifies that the background of the view is to reset.
        assertEquals(
                ((GradientDrawable) defaultBackground).getColor().getDefaultColor(),
                ((GradientDrawable) view.getBackground()).getColor().getDefaultColor());
    }
}
