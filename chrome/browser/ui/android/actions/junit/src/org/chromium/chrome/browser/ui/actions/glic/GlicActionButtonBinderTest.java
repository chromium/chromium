// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.glic.GlicUiHelper;
import org.chromium.chrome.browser.ui.actions.ActionButtonBinder;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link GlicActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicActionButtonBinderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private abstract static class AnimatableDrawable extends Drawable implements Animatable {}

    @Mock private Drawable mDrawable;
    @Mock private LayerDrawable mLayerDrawable;
    @Mock private AnimatableDrawable mAnimatableDrawable;

    private Context mContext;
    private ImageView mImageView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mImageView = new ImageView(mContext);
        mModel = new PropertyModel.Builder(GlicActionProperties.ALL_KEYS).build();
    }

    @Test
    public void testBind_SetsDrawable() {
        mModel.set(GlicActionProperties.GLIC_DRAWABLE, mDrawable);
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_DRAWABLE);
        assertEquals(mDrawable, mImageView.getDrawable());
    }

    @Test
    public void testBind_StartsAnimation() {
        when(mLayerDrawable.getNumberOfLayers()).thenReturn(1);
        when(mLayerDrawable.getDrawable(0)).thenReturn(mAnimatableDrawable);

        mModel.set(GlicActionProperties.GLIC_DRAWABLE, mLayerDrawable);
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_DRAWABLE);
        verify(mAnimatableDrawable).start();
    }

    @Test
    public void testWorkingDrawableTinting() {
        when(mDrawable.mutate()).thenReturn(mDrawable);
        Drawable workingDrawable = GlicUiHelper.createWorkingDrawable(mContext, mDrawable);

        mModel.set(GlicActionProperties.GLIC_DRAWABLE, workingDrawable);
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_DRAWABLE);

        ColorStateList tint = ColorStateList.valueOf(Color.BLUE);
        mModel.set(ActionProperties.ICON_TINT, tint);
        ActionButtonBinder.bind(mModel, mImageView, ActionProperties.ICON_TINT);

        verify(mDrawable).setTintList(tint);
    }
}
