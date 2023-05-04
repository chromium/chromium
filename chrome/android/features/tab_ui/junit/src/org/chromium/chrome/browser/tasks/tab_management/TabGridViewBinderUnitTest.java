// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Matrix;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
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
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.TabFaviconFetcher;
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
    private TabListMediator.ThumbnailFetcher mFetcher;
    @Mock
    private TabGridThumbnailView mThumbnailView;
    @Mock
    private ImageView mFaviconView;
    @Captor
    private ArgumentCaptor<Callback<Bitmap>> mCallbackCaptor;
    @Mock
    private TypedArray mTypedArray;

    @Mock
    private TabFavicon mTabFavicon;
    @Mock
    private Drawable mDrawable;

    private Context mContext;
    private PropertyModel mModel;
    private LayoutParams mLayoutParams;
    private Bitmap mBitmap;
    private static final float RESOURCE_DIMEN = 10;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;

        mModel = new PropertyModel.Builder(TabProperties.ALL_KEYS_TAB_GRID)
                         .with(TabProperties.THUMBNAIL_FETCHER, mFetcher)
                         .with(TabProperties.IS_INCOGNITO, false)
                         .with(TabProperties.IS_SELECTED, true)
                         .with(TabProperties.GRID_CARD_SIZE, new Size(INIT_WIDTH, INIT_HEIGHT))
                         .build();
        when(mViewGroup.fastFindViewById(R.id.tab_thumbnail)).thenReturn(mThumbnailView);
        when(mViewGroup.fastFindViewById(R.id.tab_favicon)).thenReturn(mFaviconView);
        when(mFaviconView.getContext()).thenReturn(mContext);
        when(mViewGroup.getContext()).thenReturn(mContext);

        // mModel, view and bitmap all use the same initial values.
        mLayoutParams = new LayoutParams(INIT_WIDTH, INIT_HEIGHT);
        mBitmap = Bitmap.createBitmap(INIT_WIDTH, INIT_HEIGHT, Config.RGB_565);
        when(mViewGroup.getLayoutParams()).thenReturn(mLayoutParams);

        LayoutParams thumbnailParams = new LayoutParams(INIT_WIDTH, INIT_HEIGHT);
        when(mThumbnailView.getLayoutParams()).thenReturn(thumbnailParams);
    }

    @After
    public void tearDown() {
        CachedFeatureFlags.resetFlagsForTesting();
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabWithCardWidth_updateCardAndThumbnail() {
        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        verify(mViewGroup).setMinimumWidth(updatedCardWidth);
        verify(mThumbnailView).setColorThumbnailPlaceHolder(false, true);
        verify(mThumbnailView).setImageDrawable(null);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));

        verify(mFetcher).fetch(mCallbackCaptor.capture(), any(), eq(true));
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageBitmap(mBitmap);
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verify(mThumbnailView).getLayoutParams();
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw348dp")
    public void bindClosableTabWithCardWidth_updateCardAndThumbnail_notSelected() {
        // Update width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 40 = 160.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 40 - 160.
        final int updatedCardWidth = 200;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));
        mModel.set(TabProperties.IS_SELECTED, false);
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        verify(mViewGroup).setMinimumWidth(updatedCardWidth);
        verify(mThumbnailView).setColorThumbnailPlaceHolder(false, false);
        verify(mThumbnailView).setImageDrawable(null);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));

        verify(mFetcher).fetch(mCallbackCaptor.capture(), any(), eq(false));
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageBitmap(mBitmap);
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verify(mThumbnailView).getLayoutParams();
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw600dp")
    public void bindClosableTabWithCardWidthOnTablet_updateCardAndThumbnail() {
        // Update card width.
        // updatedBitmapWidth = updatedCardWidth - margins = 200 - 24 = 176.
        // updatedBitmapHeight = INIT_HEIGHT - margins = 200 - 24 - 176.
        final int updatedCardWidth = 200;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(updatedCardWidth, INIT_HEIGHT));

        // Call.
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        // Verify.
        verify(mViewGroup).setMinimumWidth(updatedCardWidth);
        verify(mThumbnailView).setColorThumbnailPlaceHolder(false, true);
        verify(mThumbnailView).setImageDrawable(null);
        assertThat(mLayoutParams.width, equalTo(updatedCardWidth));
        verify(mFetcher).fetch(mCallbackCaptor.capture(), any(), eq(true));

        // Pass bitmap to callback and verify thumbnail updated with image resize.
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageBitmap(mBitmap);
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verify(mThumbnailView).getLayoutParams();
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedBitmapWidth / INIT_WIDTH = 176 / 100 = 1.76f.
        float expectedScale = 1.76f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (176 - (100*1.76))/2 = 0.
        float expectedXTrans = 0.f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
    }

    @Test
    @org.robolectric.annotation.Config(qualifiers = "sw600dp")
    public void bindClosableTabWithCardHeightOnTablet_updateCardAndThumbnail() {
        // Update card height = 400.
        // updatedBitmapWidth = INIT_WIDTH - margins = 100 - 24 = 76.
        // updatedBitmapHeight = updatedCardHeight - margins = 400 - 60 = 340.
        final int updatedCardHeight = 400;
        mModel.set(TabProperties.GRID_CARD_SIZE, new Size(INIT_WIDTH, updatedCardHeight));
        // Call.
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.GRID_CARD_SIZE);

        // Verify.
        verify(mViewGroup).setMinimumHeight(updatedCardHeight);
        verify(mThumbnailView).setColorThumbnailPlaceHolder(false, true);
        verify(mThumbnailView).setImageDrawable(null);
        assertThat(mLayoutParams.height, equalTo(updatedCardHeight));
        verify(mFetcher).fetch(mCallbackCaptor.capture(), any(), eq(true));

        // Pass bitmap to callback and verify thumbnail updated with image resize.
        mCallbackCaptor.getValue().onResult(mBitmap);

        verify(mThumbnailView).setScaleType(ScaleType.MATRIX);
        verify(mThumbnailView).setImageBitmap(mBitmap);
        ArgumentCaptor<Matrix> matrixCaptor = ArgumentCaptor.forClass(Matrix.class);
        verify(mThumbnailView).setImageMatrix(matrixCaptor.capture());
        verify(mThumbnailView).getLayoutParams();
        verifyNoMoreInteractions(mThumbnailView);

        // Verify metrics scale + translate.
        // Scale = updatedBitmapHeight / INIT_HEIGHT = 340 / 200 = 1.7f.
        float expectedScale = 1.7f;
        // xTranslate = (updatedBitmapWidth - scaledWidth) /2 = (76 - (100*1.7))/2 = -47.
        float expectedXTrans = -47f;
        assertImageMatrix(matrixCaptor, expectedScale, expectedXTrans);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testBindFaviconFetcher() {
        doReturn(mDrawable).when(mTabFavicon).getSelectedDrawable();

        TabFaviconFetcher fetcher = new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> callback) {
                callback.onResult(mTabFavicon);
            }
        };
        mModel.set(TabProperties.FAVICON_FETCHER, fetcher);
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.FAVICON_FETCHER);

        verify(mFaviconView).setImageDrawable(mDrawable);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testBindNullFaviconFetcher() {
        mModel.set(TabProperties.FAVICON_FETCHER, null);
        TabGridViewBinder.bindClosableTab(mModel, mViewGroup, TabProperties.FAVICON_FETCHER);

        verify(mFaviconView).setImageDrawable(null);
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
