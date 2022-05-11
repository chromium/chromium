// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.junit.Assert.assertThat;
import static org.hamcrest.CoreMatchers.equalTo;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Matrix;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView.ScaleType;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/** Junit Tests for {@link TabGridViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TabGridViewBinderUnitTest {
    private static final int INIT_WIDTH = 100;
    private static final int INIT_HEIGHT = 200;
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Mock
    private ViewLookupCachingFrameLayout mViewGroup;
    @Mock
    private Context mContext;
    @Mock
    private Resources mResources;
    @Mock
    private TabListMediator.ThumbnailFetcher mFetcher;
    @Mock
    private TabGridThumbnailView mThumbnailView;
    @Captor
    private ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;

    private PropertyModel mModel;
    private LayoutParams mLayoutParams;
    private Bitmap mBitmap;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                         .with(TabProperties.THUMBNAIL_FETCHER, mFetcher)
                         .with(TabProperties.IS_INCOGNITO, false)
                         .with(TabProperties.IS_SELECTED, true)
                         .with(TabProperties.GRID_CARD_WIDTH, INIT_WIDTH)
                         .with(TabProperties.GRID_CARD_HEIGHT, INIT_HEIGHT)
                         .build();
        when(mViewGroup.fastFindViewById(R.id.tab_thumbnail)).thenReturn(mThumbnailView);
        when(mViewGroup.getContext()).thenReturn(mContext);
        when(mContext.getResources()).thenReturn(mResources);
        // Mock tablet.
        when(mResources.getInteger(org.chromium.ui.R.integer.min_screen_width_bucket)).thenReturn(3);

        // mModel, view and bitmap all use the same initial values.
        mLayoutParams = new LayoutParams(INIT_WIDTH, INIT_HEIGHT);
        mBitmap = Bitmap.createBitmap(INIT_WIDTH, INIT_HEIGHT, Config.RGB_565);
        when(mViewGroup.getLayoutParams()).thenReturn(mLayoutParams);
    }

    @After
    public void tearDown() {
        TabUiFeatureUtilities.setTabletGridTabSwitcherPolishEnabledForTesting(null);
    }

    @Test
    public void bindClosableTabWithCardWidth_updateCardAndThumbnail() {
        // Update width - 200.
        final int updatedWidth = 200;
        mModel.set(TabProperties.GRID_CARD_WIDTH, updatedWidth);
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.GRID_CARD_WIDTH);

        verify(mViewGroup).setMinimumWidth(updatedWidth);
        verify(mThumbnailView).setColorThumbnailPlaceHolder(false, true);
        assertThat(mLayoutParams.width, equalTo(updatedWidth));

        verify(mFetcher).fetch(mCallbackCaptor.capture());
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mThumbnailView).setScaleType(ScaleType.FIT_CENTER);
        verify(mThumbnailView).setAdjustViewBounds(true);
        verify(mThumbnailView).setImageBitmap(mBitmap);
        verify(mThumbnailView).maybeAdjustThumbnailHeight();

        verifyNoMoreInteractions(mThumbnailView);
    }

    @Test
    public void bindClosableTabWithCardWidthWithPolishEnabled_updateCardAndThumbnail() {
        TabUiFeatureUtilities.setTabletGridTabSwitcherPolishEnabledForTesting(true);
        // Update width.
        final int updatedWidth = 200;
        mModel.set(TabProperties.GRID_CARD_WIDTH, updatedWidth);

        // Call.
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.GRID_CARD_WIDTH);

        // Verify.
        verify(mViewGroup).setMinimumWidth(updatedWidth);
        verify(mThumbnailView).setColorThumbnailPlaceHolder(false, true);
        assertThat(mLayoutParams.width, equalTo(updatedWidth));
        verify(mFetcher).fetch(mCallbackCaptor.capture());

        // Pass bitmap to callback and verify thumbnail updated with image resize.
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageBitmap(mBitmap);
        verify(mThumbnailView).maybeAdjustThumbnailHeight();
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedWidth / INIT_WIDTH = 200 / 100 = 2.
        float expectedScale = 2.f;
        // xTranslate = (destinationWidth - scaledWidth) /2 = (200 - (100*2))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
    }

    @Test
    public void bindClosableTabWithCardHeightWithPolishEnabled_updateCardAndThumbnail() {
        TabUiFeatureUtilities.setTabletGridTabSwitcherPolishEnabledForTesting(true);
        LayoutParams thumbnailParams = new LayoutParams(INIT_WIDTH, INIT_HEIGHT);
        when(mThumbnailView.getLayoutParams()).thenReturn(thumbnailParams);

        // Update height.
        final int updatedHeight = 400;
        mModel.set(TabProperties.GRID_CARD_HEIGHT, updatedHeight);

        // Call.
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.GRID_CARD_HEIGHT);

        // Verify.
        verify(mViewGroup).setMinimumHeight(updatedHeight);
        verify(mThumbnailView).setColorThumbnailPlaceHolder(false, true);
        assertThat(mLayoutParams.height, equalTo(updatedHeight));
        assertThat(thumbnailParams.height, equalTo(LayoutParams.MATCH_PARENT));
        verify(mFetcher).fetch(mCallbackCaptor.capture());

        // Pass bitmap to callback and verify thumbnail updated with image resize.
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageBitmap(mBitmap);
        verify(mThumbnailView).maybeAdjustThumbnailHeight();
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verify(mThumbnailView).getLayoutParams();
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedHeight / INIT_HEIGHT = 400 / 200 = 2.
        float expectedScale = 2.f;
        // xTranslate = (destinationWidth - scaledWidth) /2 = (100 - (100*2))/2 = -50.
        float expectedXTrans = -50.f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
    }

    private void assertImageMatrix(
            ArgumentCaptor<Matrix> matrixCaptor, float expectedScale, float expectedTrans) {
        float[] matValues = new float[9];
        matrixCaptor.getValue().getValues(matValues);
        float scaleX = matValues[0];
        float scaleY = matValues[4];
        float transX = matValues[2];
        float transY = matValues[5];
        assertThat("Incorrect image matrix scaleX", scaleX, equalTo(expectedScale));
        assertThat("Incorrect image matrix scaleY", scaleY, equalTo(expectedScale));
        assertThat("Incorrect image matrix transY", transY, equalTo(0f));
        assertThat("Incorrect image matrix transX", transX, equalTo(expectedTrans));
    }
}
