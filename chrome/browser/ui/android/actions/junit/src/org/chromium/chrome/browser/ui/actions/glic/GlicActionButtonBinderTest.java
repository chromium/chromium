// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.actions.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link GlicActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GlicActionButtonBinderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

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
    public void testBind_WorkingState_SetsDrawable() {
        mModel.set(GlicActionProperties.GLIC_STATE, GlicActionProperties.GlicState.WORKING);
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_STATE);

        // Verify that a drawable was set.
        assertNotNull(mImageView.getDrawable());
    }

    @Test
    public void testBind_NeedsReviewState_SetsDrawable() {
        mModel.set(GlicActionProperties.GLIC_STATE, GlicActionProperties.GlicState.NEEDS_REVIEW);
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_STATE);

        assertNotNull(mImageView.getDrawable());
    }

    @Test
    public void testBind_DoneState_SetsDrawable() {
        mModel.set(GlicActionProperties.GLIC_STATE, GlicActionProperties.GlicState.DONE);
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_STATE);

        assertNotNull(mImageView.getDrawable());
    }

    @Test
    public void testBind_OtherState_SetsDrawable() {
        mModel.set(GlicActionProperties.GLIC_STATE, 999); // Some other state
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_STATE);

        assertNotNull(mImageView.getDrawable());
    }

    @Test
    public void testBind_DefaultState_AlwaysUseFilledIconFalse_SetsOutlinedDrawable() {
        mModel.set(GlicActionProperties.GLIC_STATE, GlicActionProperties.GlicState.DEFAULT);
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_STATE);

        assertEquals(
                R.drawable.ic_spark_outlined_24dp,
                shadowOf(mImageView.getDrawable()).getCreatedFromResId());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR + ":always_use_filled_glic_icon/true")
    public void testBind_DefaultState_AlwaysUseFilledIconTrue_SetsFilledDrawable() {
        mModel.set(GlicActionProperties.GLIC_STATE, GlicActionProperties.GlicState.DEFAULT);
        GlicActionButtonBinder.bind(mModel, mImageView, GlicActionProperties.GLIC_STATE);

        assertEquals(
                R.drawable.ic_spark_filled_24dp,
                shadowOf(mImageView.getDrawable()).getCreatedFromResId());
    }
}
