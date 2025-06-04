// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;

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
    @Mock private View mView;
    @Mock private ImageView mVoiceSearchButtonView;
    @Mock private ImageView mLensButtonView;
    @Mock private ImageView mIncognitoButtonView;
    @Mock private OnClickListener mOnClickListener;

    private PropertyModel mPropertyModel;

    @Before
    public void setup() {
        mPropertyModel = new PropertyModel.Builder(ComposeplateProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mPropertyModel, mView, ComposeplateViewBinder::bind);
    }

    @Test
    public void testSetVisibility() {
        mPropertyModel.set(ComposeplateProperties.IS_VISIBLE, true);
        verify(mView).setVisibility(eq(View.VISIBLE));

        mPropertyModel.set(ComposeplateProperties.IS_VISIBLE, false);
        verify(mView).setVisibility(eq(View.GONE));
    }

    @Test
    public void testSetVoiceSearchButtonClickListener() {
        when(mView.findViewById(eq(R.id.voice_search_button))).thenReturn(mVoiceSearchButtonView);
        mPropertyModel.set(ComposeplateProperties.VOICE_SEARCH_CLICK_LISTENER, mOnClickListener);
        verify(mVoiceSearchButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetLensButtonClickListener() {
        when(mView.findViewById(eq(R.id.lens_camera_button))).thenReturn(mLensButtonView);
        mPropertyModel.set(ComposeplateProperties.LENS_CLICK_LISTENER, mOnClickListener);
        verify(mLensButtonView).setOnClickListener(eq(mOnClickListener));
    }

    @Test
    public void testSetIncognitoButtonClickListener() {
        when(mView.findViewById(eq(R.id.incognito_button))).thenReturn(mIncognitoButtonView);
        mPropertyModel.set(ComposeplateProperties.INCOGNITO_CLICK_LISTENER, mOnClickListener);
        verify(mIncognitoButtonView).setOnClickListener(eq(mOnClickListener));
    }
}
